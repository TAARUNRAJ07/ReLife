#!/usr/bin/env python3
"""
ReLife Hardware Mock Control CLI (mockctl)
Allows test engineers to enqueue commands for the ESP32 rig and toggle failure injection modes.
"""
import sys
import argparse
import requests
import json
from typing import Dict, Any

DEFAULT_SERVER_URL = "http://127.0.0.1:8000"

def get_admin_url(server_url: str, endpoint: str) -> str:
    return f"{server_url.rstrip('/')}/_admin/{endpoint.lstrip('/')}"

def cmd_enqueue(args):
    url = get_admin_url(args.server, "command")
    params: Dict[str, Any] = {}
    if args.command == "START_TEST":
        params["test_type"] = args.test_type
        params["battery_id"] = args.battery_id
        if args.cutoff_v is not None:
            params["cutoff_voltage_v"] = args.cutoff_v
        if args.duration_s is not None:
            params["max_duration_s"] = args.duration_s
    elif args.command == "START_LOG":
        params["interval_ms"] = args.interval_ms
    elif args.command == "STOP_TEST":
        params["reason"] = args.reason or "operator_stop"
    elif args.command == "REQUEST_STATUS":
        params["detail_level"] = "full"

    payload = {
        "device_id": args.device_id,
        "command": args.command,
        "params": params
    }

    try:
        resp = requests.post(url, json=payload, timeout=5)
        resp.raise_for_status()
        data = resp.json()
        print(f"[OK] Enqueued command {data.get('command')} (ID: {data.get('command_id')}) for device {data.get('device_id')}")
    except Exception as e:
        print(f"[ERROR] Failed to enqueue command: {e}")
        sys.exit(1)

def cmd_failure(args):
    url = get_admin_url(args.server, "failure")
    payload = {}
    if args.latency is not None:
        payload["fixed_latency_ms"] = args.latency
    if args.rate_500 is not None:
        payload["random_500_rate"] = args.rate_500
    if args.force_401 is not None:
        payload["force_401"] = args.force_401
    if args.force_429 is not None:
        payload["force_429"] = args.force_429
    if args.drop_response is not None:
        payload["drop_response_after_store"] = args.drop_response
    if args.reject_n is not None:
        payload["reject_next_n"] = args.reject_n

    try:
        resp = requests.post(url, json=payload, timeout=5)
        resp.raise_for_status()
        data = resp.json()
        print("[OK] Updated failure configuration:")
        print(json.dumps(data.get("failure_config"), indent=2))
    except Exception as e:
        print(f"[ERROR] Failed to set failure modes: {e}")
        sys.exit(1)

def cmd_state(args):
    url = get_admin_url(args.server, "state")
    try:
        resp = requests.get(url, timeout=5)
        resp.raise_for_status()
        data = resp.json()
        print("[MOCK SERVER STATE]")
        print(json.dumps(data, indent=2))
    except Exception as e:
        print(f"[ERROR] Failed to fetch state: {e}")
        sys.exit(1)

def cmd_reset(args):
    url = get_admin_url(args.server, "reset")
    try:
        resp = requests.post(url, timeout=5)
        resp.raise_for_status()
        print("[OK] Mock server data and state reset successfully.")
    except Exception as e:
        print(f"[ERROR] Failed to reset server: {e}")
        sys.exit(1)

def main():
    parser = argparse.ArgumentParser(description="ReLife Hardware Mock Control CLI")
    parser.add_argument("--server", default=DEFAULT_SERVER_URL, help="Mock server base URL")
    subparsers = parser.add_subparsers(dest="subcommand", required=True)

    # Command: start-log
    p_log = subparsers.add_parser("start-log", help="Enqueue START_LOG command")
    p_log.add_argument("--device-id", default="ESP32-RIG-001", help="Target device ID")
    p_log.add_argument("--interval-ms", type=int, default=1000, help="Sampling interval in ms")
    p_log.set_defaults(command="START_LOG", func=cmd_enqueue)

    # Command: start-test
    p_test = subparsers.add_parser("start-test", help="Enqueue START_TEST command")
    p_test.add_argument("--device-id", default="ESP32-RIG-001", help="Target device ID")
    p_test.add_argument("--test-type", choices=["capacity", "pulse"], default="capacity", help="Test profile")
    p_test.add_argument("--battery-id", default="RL-BAT-0042", help="Battery identifier")
    p_test.add_argument("--cutoff-v", type=float, default=10.0, help="Discharge cutoff voltage")
    p_test.add_argument("--duration-s", type=int, default=3600, help="Maximum test duration")
    p_test.set_defaults(command="START_TEST", func=cmd_enqueue)

    # Command: stop-test
    p_stop = subparsers.add_parser("stop-test", help="Enqueue STOP_TEST command")
    p_stop.add_argument("--device-id", default="ESP32-RIG-001", help="Target device ID")
    p_stop.add_argument("--reason", default="user_abort", help="Reason for stopping")
    p_stop.set_defaults(command="STOP_TEST", func=cmd_enqueue)

    # Command: request-status
    p_status = subparsers.add_parser("request-status", help="Enqueue REQUEST_STATUS command")
    p_status.add_argument("--device-id", default="ESP32-RIG-001", help="Target device ID")
    p_status.set_defaults(command="REQUEST_STATUS", func=cmd_enqueue)

    # Command: failure
    p_fail = subparsers.add_parser("failure", help="Configure failure injection modes")
    p_fail.add_argument("--latency", type=float, help="Fixed latency in ms")
    p_fail.add_argument("--rate-500", type=float, help="Random 500 error probability (0.0 to 1.0)")
    p_fail.add_argument("--force-401", type=lambda v: v.lower() in ("true", "1", "yes"), help="Force 401 Unauthorized")
    p_fail.add_argument("--force-429", type=lambda v: v.lower() in ("true", "1", "yes"), help="Force 429 Rate Limit")
    p_fail.add_argument("--drop-response", type=lambda v: v.lower() in ("true", "1", "yes"), help="Drop response after saving")
    p_fail.add_argument("--reject-n", type=int, help="Reject the next N records")
    p_fail.set_defaults(func=cmd_failure)

    # Command: state
    p_state = subparsers.add_parser("state", help="Inspect mock server state")
    p_state.set_defaults(func=cmd_state)

    # Command: reset
    p_reset = subparsers.add_parser("reset", help="Reset all mock server state and stored data")
    p_reset.set_defaults(func=cmd_reset)

    args = parser.parse_args()
    args.func(args)

if __name__ == "__main__":
    main()
