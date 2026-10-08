#!/usr/bin/env python3
"""
ReLife Contract Acceptance Test Suite
Verifies mock server endpoints against contracts/v1 schema and contract specifications.
"""
import os
import sys
import json
import time
import pytest
import requests
from fastapi.testclient import TestClient

from tools.hw.mock_server.main import app, state

VALID_DEVICE_KEY = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
EXAMPLES_DIR = os.path.join(os.path.dirname(__file__), "..", "..", "contracts", "v1", "examples")

@pytest.fixture(autouse=True)
def reset_server_state():
    state.reset_state()
    state.failure_config.fixed_latency_ms = 0
    state.failure_config.random_500_rate = 0
    state.failure_config.force_401 = False
    state.failure_config.force_429 = False
    state.failure_config.drop_response_after_store = False
    state.failure_config.reject_next_n = 0
    yield

def test_telemetry_ingest_and_duplicate_idempotency():
    client = TestClient(app)
    batch_path = os.path.join(EXAMPLES_DIR, "telemetry_batch.json")
    with open(batch_path, "r", encoding="utf-8") as f:
        payload = json.load(f)

    headers = {"X-Device-Key": VALID_DEVICE_KEY}
    device_id = payload["device_id"]

    # 1. First POST: should accept all records and return ack_up_to_seq
    resp = client.post(f"/api/v1/devices/{device_id}/telemetry", json=payload, headers=headers)
    assert resp.status_code == 200
    data = resp.json()
    assert data["received_count"] == 2
    assert data["ack_up_to_seq"] == 2
    assert data["duplicates"] == 0
    assert len(data["rejected"]) == 0

    # 2. Second POST (same payload): should report all as duplicates, ack unchanged
    resp2 = client.post(f"/api/v1/devices/{device_id}/telemetry", json=payload, headers=headers)
    assert resp2.status_code == 200
    data2 = resp2.json()
    assert data2["received_count"] == 2
    assert data2["ack_up_to_seq"] == 2
    assert data2["duplicates"] == 2
    assert len(data2["rejected"]) == 0

def test_telemetry_authentication():
    client = TestClient(app)
    batch_path = os.path.join(EXAMPLES_DIR, "telemetry_batch.json")
    with open(batch_path, "r", encoding="utf-8") as f:
        payload = json.load(f)

    device_id = payload["device_id"]

    # Missing auth header
    resp = client.post(f"/api/v1/devices/{device_id}/telemetry", json=payload)
    assert resp.status_code == 401
    assert resp.json()["error_code"] == "MISSING_AUTH"

    # Invalid auth key
    resp2 = client.post(
        f"/api/v1/devices/{device_id}/telemetry",
        json=payload,
        headers={"X-Device-Key": "wrong-key"}
    )
    assert resp2.status_code == 401
    assert resp2.json()["error_code"] == "INVALID_AUTH"

def test_heartbeat_reporting():
    client = TestClient(app)
    hb_path = os.path.join(EXAMPLES_DIR, "heartbeat.json")
    with open(hb_path, "r", encoding="utf-8") as f:
        payload = json.load(f)

    device_id = "ESP32-RIG-001"
    headers = {"X-Device-Key": VALID_DEVICE_KEY}

    resp = client.post(f"/api/v1/devices/{device_id}/heartbeat", json=payload, headers=headers)
    assert resp.status_code == 200
    data = resp.json()
    assert data["status"] == "ok"
    assert "server_time" in data

def test_command_lifecycle():
    client = TestClient(app)
    device_id = "ESP32-RIG-001"
    headers = {"X-Device-Key": VALID_DEVICE_KEY}

    # 1. Admin enqueues command
    cmd_resp = client.post("/_admin/command", json={
        "device_id": device_id,
        "command": "START_TEST",
        "params": {"test_type": "capacity", "battery_id": "RL-BAT-0042"}
    })
    assert cmd_resp.status_code == 200
    cmd_id = cmd_resp.json()["command_id"]

    # 2. Device polls commands
    poll_resp = client.get(f"/api/v1/devices/{device_id}/commands", headers=headers)
    assert poll_resp.status_code == 200
    cmds = poll_resp.json()["commands"]
    assert len(cmds) == 1
    assert cmds[0]["command_id"] == cmd_id
    assert cmds[0]["command"] == "START_TEST"

    # 3. Device acks command
    ack_path = os.path.join(EXAMPLES_DIR, "command_ack.json")
    with open(ack_path, "r", encoding="utf-8") as f:
        ack_payload = json.load(f)

    ack_resp = client.post(f"/api/v1/devices/{device_id}/commands/{cmd_id}/ack", json=ack_payload, headers=headers)
    assert ack_resp.status_code == 200
    assert ack_resp.json()["acknowledged"] is True

def test_failure_injection():
    client = TestClient(app)
    device_id = "ESP32-RIG-001"
    headers = {"X-Device-Key": VALID_DEVICE_KEY}

    # Test Force 429
    client.post("/_admin/failure", json={"force_429": True, "retry_after_s": 10})
    resp = client.get(f"/api/v1/devices/{device_id}/commands", headers=headers)
    assert resp.status_code == 429
    assert resp.headers.get("retry-after") == "10"
    assert resp.json()["error_code"] == "RATE_LIMIT_EXCEEDED"

    # Reset failure
    client.post("/_admin/failure", json={"force_429": False})
    resp = client.get(f"/api/v1/devices/{device_id}/commands", headers=headers)
    assert resp.status_code == 200

if __name__ == "__main__":
    pytest.main([__file__, "-v"])
