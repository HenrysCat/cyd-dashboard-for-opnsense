"""Runtime configuration.

Config is persisted to ``data/config.json`` on a mounted volume rather than only
read from process env vars. On first boot it's seeded from `.env`/environment;
after that, `config.json` is the source of truth. This is deliberate: it's the
seam a future settings web UI would write to, without any storage rework.
"""
import json
import os
from dataclasses import asdict, dataclass
from pathlib import Path

from dotenv import load_dotenv

load_dotenv()

DATA_DIR = Path(os.environ.get("DATA_DIR", "data"))
CONFIG_PATH = DATA_DIR / "config.json"


def _normalize_host(value: str) -> str:
    """Accepts a bare host or a pasted URL (e.g. "https://192.168.0.1/") and
    reduces it to just the host, since base_url below adds the scheme itself."""
    value = value.strip()
    for prefix in ("https://", "http://"):
        if value.startswith(prefix):
            value = value[len(prefix):]
    return value.rstrip("/")


@dataclass
class Config:
    opnsense_host: str
    api_key: str
    api_secret: str
    verify_ssl: bool = True
    poll_interval_fast: float = 2.0
    poll_interval_slow: float = 15.0

    def __post_init__(self):
        # Runs whether the Config came from env vars or a persisted config.json,
        # so a bad value saved from a previous run gets cleaned up too.
        self.opnsense_host = _normalize_host(self.opnsense_host)

    @property
    def base_url(self) -> str:
        return f"https://{self.opnsense_host}/api"


def _from_env() -> Config:
    return Config(
        opnsense_host=os.environ.get("OPNSENSE_HOST", ""),
        api_key=os.environ.get("OPNSENSE_API_KEY", ""),
        api_secret=os.environ.get("OPNSENSE_API_SECRET", ""),
        verify_ssl=os.environ.get("VERIFY_SSL", "true").lower() not in ("false", "0", "no"),
        poll_interval_fast=float(os.environ.get("POLL_INTERVAL_FAST", "2.0")),
        poll_interval_slow=float(os.environ.get("POLL_INTERVAL_SLOW", "15.0")),
    )


def load_config() -> Config:
    if CONFIG_PATH.exists():
        return Config(**json.loads(CONFIG_PATH.read_text()))

    config = _from_env()
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    CONFIG_PATH.write_text(json.dumps(asdict(config), indent=2))
    return config
