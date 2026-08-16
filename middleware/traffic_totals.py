"""Day- and month-to-date WAN byte totals, persisted across restarts.

The aggregator already diffs OPNsense's cumulative interface counters to get a
live bps figure; this accumulates those same deltas so the dashboard can show
how much has actually been used, not just how fast it is moving right now.

Counting deltas rather than reading the firewall's absolute counters is what
makes the totals survive an OPNsense reboot: a counter reset shows up as a
negative delta, which is discarded, instead of a total that jumps backwards.
The trade-off is that traffic passing while the middleware is down is never
counted -- there is no way to attribute it to a particular day after the fact,
so it is dropped rather than guessed at.

Stored next to config.json on the mounted volume. Writes are throttled: at the
2s fast-poll cadence an unthrottled save would rewrite the file ~43,000 times a
day, and an unclean shutdown costs at most FLUSH_INTERVAL_SECONDS of counting.

Day and month boundaries follow the container's local time, so set TZ in the
compose environment to have them line up with your own midnight rather than UTC.
"""
import json
import logging
import os
import time
from datetime import datetime
from pathlib import Path

logger = logging.getLogger("traffic_totals")

FLUSH_INTERVAL_SECONDS = 60.0


class TrafficTotals:
    def __init__(self, path: Path, flush_interval: float = FLUSH_INTERVAL_SECONDS):
        self._path = path
        self._flush_interval = flush_interval
        # Seeded with the clock rather than 0, or the first poll would always
        # flush immediately (monotonic() starts at the host's uptime, which is
        # already well past any sane interval).
        self._last_flush = time.monotonic()
        self._dirty = False
        self._state = self._load()

    def _load(self) -> dict:
        if self._path.exists():
            try:
                state = json.loads(self._path.read_text())
                # The period markers are what give the byte counts meaning; a
                # file without them cannot be attributed to a day or month, so
                # it is discarded rather than silently counted as today's.
                # Missing *byte* keys are just filled in, which keeps the file
                # forward-compatible if more counters are added later.
                if "day" in state and "month" in state:
                    return {**self._empty(), **state}
                logger.warning("%s has no day/month marker; starting totals from zero",
                               self._path)
            except (json.JSONDecodeError, OSError) as exc:
                # Losing history is annoying but not worth refusing to start
                # over -- a corrupt file would otherwise wedge the container.
                logger.warning("Could not read %s (%s); starting totals from zero",
                               self._path, exc)
        return self._empty()

    @staticmethod
    def _empty() -> dict:
        now = datetime.now()
        return {
            "day": now.strftime("%Y-%m-%d"),
            "month": now.strftime("%Y-%m"),
            "day_in_bytes": 0,
            "day_out_bytes": 0,
            "month_in_bytes": 0,
            "month_out_bytes": 0,
        }

    def _roll_over(self) -> None:
        """Zero the day and/or month buckets when the calendar has moved on."""
        now = datetime.now()
        day = now.strftime("%Y-%m-%d")
        month = now.strftime("%Y-%m")

        if self._state["day"] != day:
            self._state["day"] = day
            self._state["day_in_bytes"] = 0
            self._state["day_out_bytes"] = 0
            self._dirty = True
        if self._state["month"] != month:
            self._state["month"] = month
            self._state["month_in_bytes"] = 0
            self._state["month_out_bytes"] = 0
            self._dirty = True

    def add(self, in_bytes: int, out_bytes: int) -> None:
        """Accumulate one poll's worth of traffic. Negative deltas (a firewall
        counter reset) are ignored by the caller before they reach here."""
        self._roll_over()
        if in_bytes <= 0 and out_bytes <= 0:
            # Still flush on an idle link: a rollover above may need saving.
            self._maybe_flush()
            return

        self._state["day_in_bytes"] += max(0, in_bytes)
        self._state["day_out_bytes"] += max(0, out_bytes)
        self._state["month_in_bytes"] += max(0, in_bytes)
        self._state["month_out_bytes"] += max(0, out_bytes)
        self._dirty = True
        self._maybe_flush()

    def _maybe_flush(self) -> None:
        if not self._dirty:
            return
        now = time.monotonic()
        if now - self._last_flush < self._flush_interval:
            return
        self.flush()

    def flush(self) -> None:
        if not self._dirty:
            return
        try:
            self._path.parent.mkdir(parents=True, exist_ok=True)
            # Write-then-rename, so a crash mid-write cannot leave a truncated
            # file that the next start would have to throw away.
            tmp = self._path.with_suffix(".tmp")
            tmp.write_text(json.dumps(self._state, indent=2))
            os.replace(tmp, self._path)
            self._dirty = False
            self._last_flush = time.monotonic()
        except OSError as exc:
            logger.warning("Could not persist traffic totals to %s: %s", self._path, exc)

    def as_dict(self) -> dict:
        """The subset the dashboard payload carries, with the rollover applied
        so a page loaded just after midnight does not show yesterday's figure."""
        self._roll_over()
        return {
            "wan_today_in_bytes": self._state["day_in_bytes"],
            "wan_today_out_bytes": self._state["day_out_bytes"],
            "wan_month_in_bytes": self._state["month_in_bytes"],
            "wan_month_out_bytes": self._state["month_out_bytes"],
            "totals_since_day": self._state["day"],
            "totals_since_month": self._state["month"],
        }
