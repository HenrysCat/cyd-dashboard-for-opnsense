import logging
from contextlib import asynccontextmanager

from fastapi import FastAPI, HTTPException

from aggregator import Aggregator
from config import load_config
from opnsense_client import OpnsenseClient

logging.basicConfig(level=logging.INFO)


@asynccontextmanager
async def lifespan(app: FastAPI):
    config = load_config()
    client = OpnsenseClient(config)
    aggregator = Aggregator(config, client)
    aggregator.start()
    app.state.aggregator = aggregator

    yield

    await aggregator.stop()


app = FastAPI(title="OPNsense CYD Dashboard Middleware", lifespan=lifespan)


@app.get("/health")
async def health():
    return {"status": "ok"}


@app.get("/dashboard")
async def dashboard():
    snapshot = await app.state.aggregator.get_snapshot()
    if all(value is None for value in snapshot.values()):
        raise HTTPException(status_code=503, detail="No data polled yet")
    return snapshot
