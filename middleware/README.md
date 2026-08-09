# Middleware

The bridge between OPNsense and the display. It logs into your firewall's API, gathers everything
the dashboard needs, and republishes it as one small, simple JSON response.

Run this **before** setting up the display - the display asks for this machine's address during
setup.

## Why it exists

The ESP32 has about 320KB of memory and no room to spare. Talking to OPNsense directly would mean
doing HTTPS, parsing very large responses (the firewall log alone returns roughly 700KB by
default) and calculating rates from raw counters. All of that happens here instead, on a machine
that can afford it. The display just fetches a few kilobytes of ready-made numbers.

It also means your firewall's API credentials live on a normal server rather than on a device
screwed to a wall.

## What you need

- A machine on your network that is always on (a NAS, a Pi, a home server) with **Docker**.
- Access to your OPNsense web interface, to create an API key.

## Step 1: Create an API key in OPNsense

1. In OPNsense, go to **System > Access > Users**.
2. Create a user just for this dashboard, or pick an existing one. Give it **read-only** access -
   it never needs to change anything. Exactly which privileges are required varies by OPNsense
   version; start minimal and widen only if the logs show permission errors.
3. On that user's page, find **API keys** and click **+**.
4. A file downloads containing the key and secret. **Save it now** - the secret is not shown again.

## Step 2: Configure and start it

Two ways in. **Option A** needs nothing but a browser and is the one to use with Portainer.
**Option B** is the command line.

### Option A: Portainer (copy and paste)

**Stacks > Add stack > Web editor**, give it a name, and paste this in whole. Edit the three
marked values, then **Deploy the stack** -- there is nothing to clone and nothing to build.

```yaml
services:
  cyd-dashboard-middleware:
    image: ghcr.io/henryscat/cyd-dashboard-middleware:latest
    restart: unless-stopped
    ports:
      # Change 8098 if that port is taken on this machine. Leave :8000 alone --
      # that is the port inside the container. Whatever you put on the left is
      # what you type into the display during setup.
      - "8098:8000"
    environment:
      # ---------------- EDIT THESE THREE ----------------
      OPNSENSE_HOST: 192.168.1.1
      OPNSENSE_API_KEY: paste-your-api-key-here
      OPNSENSE_API_SECRET: paste-your-api-secret-here
      # --------------------------------------------------
      # OPNsense ships a self-signed certificate, which is rejected unless this
      # is "false" or you have installed a trusted certificate on the firewall.
      VERIFY_SSL: "false"
      POLL_INTERVAL_FAST: "2.0"
      POLL_INTERVAL_SLOW: "15.0"
      DATA_DIR: /app/data
    volumes:
      - dashboard_data:/app/data
    healthcheck:
      test: ["CMD", "python", "-c", "import urllib.request; urllib.request.urlopen('http://127.0.0.1:8000/health', timeout=3)"]
      interval: 30s
      timeout: 5s
      retries: 3
      start_period: 15s

volumes:
  dashboard_data:
```

The image is public, so Portainer needs no registry credentials. Skip to
[checking it works](#check-it-works).

> **Changing a key later takes one extra step.** The environment variables above seed
> `config.json` on the volume on **first run only** -- after that `config.json` is the source of
> truth, so editing the stack and redeploying will *not* pick up a new key, host or interval. To
> genuinely change one, delete the stack **with its volume** (in Portainer, tick *Remove volumes*)
> and deploy it again.

Prefer to track this repo instead of pasting? **Stacks > Add stack > Repository** also works ---
repository URL of this repo, compose path `middleware/docker-compose.yml`, and add the variables
from `.env.example` under the stack's own environment variables, since Portainer does not read a
`.env` file out of a repo.

### Option B: command line

```
cd middleware
cp .env.example .env
```

Open `.env` and fill in your firewall's address and the key and secret from step 1:

```
OPNSENSE_HOST=192.168.1.1
OPNSENSE_API_KEY=...
OPNSENSE_API_SECRET=...
VERIFY_SSL=false
```

> **`VERIFY_SSL`** - OPNsense ships with a self-signed certificate, which will be rejected unless
> you set this to `false` or install a trusted certificate on your firewall.

Then start it:

```
docker compose up -d
```

That pulls a prebuilt image from GitHub's container registry -- nothing is compiled on your
machine. To build it from source instead (if you are changing the code, or you would rather not
pull a published image):

```
docker compose -f docker-compose.yml -f docker-compose.build.yml up -d --build
```

### Check it works

```
curl http://localhost:8098/dashboard
```

You should get a wall of JSON with your firewall's hostname in it. That address (with your
machine's LAN IP instead of `localhost`) is what you enter on the display during setup.

> **Port 8098, not 8000** - Portainer commonly uses 8000 on the same machine. Change it with
> `MIDDLEWARE_PORT` in `.env` if 8098 clashes with something of yours.

## Updating

`middleware/docker-compose.yml` pulls a tagged image rather than building one, and that is what
makes updating work properly in Portainer:

```
docker compose pull && docker compose up -d
```

In Portainer, use **Stacks > (your stack) > Pull and redeploy** with **Re-pull image** ticked.
Note that *Recreate > Re-pull image* on the container alone only re-checks the registry -- it does
not fetch a newer compose file from the repo.

> **Why not build in Portainer?** Portainer does not rebuild a `build:` stack when the repository's
> source changes, so "Pull and redeploy" quietly keeps serving the previously cached image
> ([portainer/portainer#12508](https://github.com/portainer/portainer/issues/12508)). Ticking
> "Re-pull image" does not help there either. Pulling an image built by CI avoids the problem
> entirely.

New images are published by [`.github/workflows/middleware-image.yml`](../.github/workflows/middleware-image.yml)
on every push that touches `middleware/`, for both `linux/amd64` and `linux/arm64`.

### Running it without Docker

```
cd middleware
pip install -r requirements.txt
cp .env.example .env    # then fill it in
uvicorn app:app --host 0.0.0.0 --port 8000
```

## Endpoints

| Path | Purpose |
|---|---|
| `GET /dashboard` | Everything the display needs, as one JSON object |
| `GET /health` | Liveness check, also used by the Docker healthcheck |

## Something not working?

`docker compose logs -f` shows exactly which OPNsense call is failing and why.

| What you see | Usually means |
|---|---|
| `certificate verify failed` | Set `VERIFY_SSL=false` in `.env` |
| `401` or `403` | Wrong API key/secret, or the user lacks permission for that page |
| `404` on one call | Your OPNsense version names that endpoint differently (see below) |
| `ConnectTimeout` | Wrong `OPNSENSE_HOST`, or this machine cannot reach the firewall |
| `"No data polled yet"` | It has only just started, or every call is failing - check the logs |

If a single call 404s, your OPNsense version has probably renamed it. Every OPNsense module page
has an **API** tab showing the exact path for *your* install - compare it against the `ENDPOINTS`
dictionary in `opnsense_client.py` and correct that one line.

## How it works

Four independent polling loops, so one slow call never delays another:

| Loop | Every | Fetches |
|---|---|---|
| fast | 2s | memory, disk, temperatures, traffic counters |
| slow | 15s | system info, interfaces, gateways, services, clients, firmware status, CrowdSec |
| firewall | 5s | firewall log (capped at 50 entries) |
| cpu | continuous | CPU usage, which OPNsense only exposes as a live stream |

Each poll is isolated: a failing call logs and keeps its last good value rather than blanking the
whole response. Every section carries its own `updated_at`.

Configuration is seeded from `.env` into `data/config.json` on a Docker volume on first run, and
read from there afterwards.

### Things computed here rather than passed through

- **Traffic rates.** OPNsense reports cumulative byte counters; bits per second come from the
  difference between polls.
- **Firewall block rate.** Calculated from the spread of the returned log timestamps, *not* by
  comparing them against this container's clock. The log reports the firewall's local time while
  the container runs UTC, so a direct comparison would be out by your timezone offset.
- **Client names.** DHCP leases frequently have no hostname, so the network card manufacturer from
  the ARP table is used as a fallback label.

### Optional plugins

The `crowdsec` section only appears if you have the CrowdSec plugin installed. If not, its
endpoints return 404, the middleware notices once and stops asking, and the display leaves that
page out entirely.

### Update notifications

`updates.available` only reflects what OPNsense itself last found. This service deliberately does
**not** trigger update checks against OPNsense's mirrors on a timer. Turn on OPNsense's own
periodic firmware check (**System > Firmware > Settings**) and the display's update badge starts
working.
