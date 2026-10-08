import os
import json
import time
import random
import asyncio
from typing import Dict, Any, Optional
from datetime import datetime, timezone

from fastapi import FastAPI, Header, HTTPException, Request, Response, status
from fastapi.responses import JSONResponse
import jsonschema

from .state import MockServerState

# Load JSON schema for telemetry validation
SCHEMA_PATH = os.path.join(
    os.path.dirname(__file__), "..", "..", "..", "contracts", "v1", "telemetry.schema.json"
)
SCHEMA_PATH = os.path.abspath(SCHEMA_PATH)

telemetry_schema = {}
if os.path.exists(SCHEMA_PATH):
    with open(SCHEMA_PATH, "r", encoding="utf-8") as f:
        telemetry_schema = json.load(f)

app = FastAPI(
    title="ReLife Hardware Mock Server",
    description="Mock backend server for ReLife ESP32 telemetry, heartbeat, and command testing.",
    version="1.0.0"
)

# Initialize global state
state = MockServerState(
    storage_dir=os.path.join(os.path.dirname(__file__), "data"),
    valid_device_key=os.environ.get(
        "RELIFE_DEVICE_KEY",
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
    )
)

def make_error_response(status_code: int, error_code: str, message: str, details: Optional[Dict[str, Any]] = None, headers: Optional[Dict[str, str]] = None) -> JSONResponse:
    content = {
        "error_code": error_code,
        "message": message,
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "details": details or {}
    }
    return JSONResponse(status_code=status_code, content=content, headers=headers)

@app.middleware("http")
async def failure_and_auth_middleware(request: Request, call_next):
    path = request.url.path
    # Admin endpoints bypass auth and failure simulation
    if path.startswith("/_admin") or path.startswith("/docs") or path.startswith("/openapi.json"):
        return await call_next(request)

    # 1. Fixed latency injection
    if state.failure_config.fixed_latency_ms > 0:
        await asyncio.sleep(state.failure_config.fixed_latency_ms / 1000.0)

    # 2. Force 401 injection
    if state.failure_config.force_401:
        return make_error_response(401, "FORCED_UNAUTHORIZED", "Simulated 401 Unauthorized via failure injection")

    # 3. Force 429 injection
    if state.failure_config.force_429:
        headers = {"Retry-After": str(state.failure_config.retry_after_s)}
        return make_error_response(429, "RATE_LIMIT_EXCEEDED", "Simulated 429 Rate Limit Exceeded", headers=headers)

    # 4. Random 500 rate
    if state.failure_config.random_500_rate > 0:
        if random.random() < state.failure_config.random_500_rate:
            return make_error_response(500, "INTERNAL_ERROR", "Simulated 500 Internal Server Error")

    # 5. Device Key Authentication
    device_key = request.headers.get("X-Device-Key")
    if not device_key:
        return make_error_response(401, "MISSING_AUTH", "Missing required X-Device-Key header")
    if device_key != state.valid_device_key:
        return make_error_response(401, "INVALID_AUTH", "Invalid X-Device-Key credential")

    response = await call_next(request)
    return response

# --- Device Endpoints (contracts/v1) ---

@app.post("/api/v1/devices/{device_id}/telemetry")
async def ingest_telemetry(device_id: str, request: Request):
    try:
        payload = await request.json()
    except Exception as e:
        return make_error_response(400, "MALFORMED_JSON", f"Invalid JSON payload: {str(e)}")

    # Check path device_id vs payload device_id
    if payload.get("device_id") != device_id:
        return make_error_response(
            400, "DEVICE_ID_MISMATCH",
            f"Path parameter device_id '{device_id}' does not match body device_id '{payload.get('device_id')}'"
        )

    # Validate against JSON Schema
    if telemetry_schema:
        try:
            jsonschema.validate(instance=payload, schema=telemetry_schema)
        except jsonschema.ValidationError as err:
            return make_error_response(
                400, "SCHEMA_VALIDATION_FAILED",
                f"Schema validation error: {err.message}",
                details={"path": list(err.path), "validator": err.validator}
            )

    # Store batch idempotently
    ack_seq, received_count, duplicates, rejected = state.store_telemetry_batch(payload)

    # Drop response after store simulation
    if state.failure_config.drop_response_after_store:
        return make_error_response(504, "RESPONSE_DROPPED", "Simulated dropped response after storing records")

    return {
        "ack_up_to_seq": ack_seq,
        "received_count": received_count,
        "duplicates": duplicates,
        "rejected": rejected
    }

@app.post("/api/v1/devices/{device_id}/heartbeat")
async def report_heartbeat(device_id: str, request: Request):
    try:
        payload = await request.json()
    except Exception as e:
        return make_error_response(400, "MALFORMED_JSON", f"Invalid JSON payload: {str(e)}")

    required_fields = ["boot_id", "timestamp", "uptime_s", "wifi_rssi_dbm", "free_heap_bytes", "sd_free_bytes", "firmware_version", "interlock_ok"]
    missing = [f for f in required_fields if f not in payload]
    if missing:
        return make_error_response(400, "MISSING_FIELDS", f"Missing required heartbeat fields: {missing}")

    state.store_heartbeat(device_id, payload)
    return {
        "status": "ok",
        "server_time": datetime.now(timezone.utc).isoformat()
    }

@app.get("/api/v1/devices/{device_id}/commands")
async def get_commands(device_id: str):
    cmds = state.get_pending_commands(device_id)
    return {
        "commands": cmds
    }

@app.post("/api/v1/devices/{device_id}/commands/{command_id}/ack")
async def acknowledge_command(device_id: str, command_id: str, request: Request):
    try:
        payload = await request.json()
    except Exception as e:
        return make_error_response(400, "MALFORMED_JSON", f"Invalid JSON payload: {str(e)}")

    if "status" not in payload:
        return make_error_response(400, "MISSING_FIELDS", "Missing 'status' in command acknowledgement")

    state.acknowledge_command(command_id, payload["status"], payload.get("error"))
    return {
        "acknowledged": True
    }

# --- Admin & Failure Injection Endpoints ---

@app.get("/_admin/state")
async def admin_get_state():
    return {
        "total_unique_records_stored": len(state.stored_records),
        "active_devices_boot_count": len(state.boot_sequences),
        "failure_config": state.failure_config.to_dict(),
        "latest_heartbeats": state.heartbeats,
        "command_history": state.command_history
    }

@app.post("/_admin/failure")
async def admin_set_failure(request: Request):
    payload = await request.json()
    for k, v in payload.items():
        if hasattr(state.failure_config, k):
            setattr(state.failure_config, k, v)
    return {
        "status": "updated",
        "failure_config": state.failure_config.to_dict()
    }

@app.post("/_admin/command")
async def admin_enqueue_command(request: Request):
    payload = await request.json()
    device_id = payload.get("device_id", "ESP32-RIG-001")
    command = payload.get("command")
    params = payload.get("params", {})
    if not command:
        raise HTTPException(status_code=400, detail="command is required")
    cmd_id = state.enqueue_command(device_id, command, params)
    return {
        "status": "enqueued",
        "command_id": cmd_id,
        "device_id": device_id,
        "command": command
    }

@app.post("/_admin/reset")
async def admin_reset():
    state.reset_state()
    return {"status": "reset_complete"}

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
