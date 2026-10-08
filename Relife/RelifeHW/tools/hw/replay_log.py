#!/usr/bin/env python3
"""
ReLife Telemetry Log Replayer (replay_log.py)
Posts a recorded JSONL log to a ReLife backend (live or mock) with source = "replay".
"""
import sys
import os
import json
import time
import argparse
import requests
from datetime import datetime, timezone
from typing import List, Dict, Any

DEFAULT_SERVER_URL = "http://127.0.0.1:8000"
DEFAULT_DEVICE_KEY = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"

def main():
    parser = argparse.ArgumentParser(description="ReLife Telemetry Log Replayer")
    parser.add_argument("log_file", help="Path to JSONL telemetry log file")
    parser.add_argument("--server", default=DEFAULT_SERVER_URL, help="Backend base URL")
    parser.add_argument("--device-key", default=DEFAULT_DEVICE_KEY, help="X-Device-Key authentication header")
    parser.add_argument("--device-id", help="Override device ID (default: use ID from log)")
    parser.add_argument("--boot-id", help="Override boot ID (default: use ID from log)")
    parser.add_argument("--batch-size", type=int, default=20, help="Number of records per HTTP POST batch")
    parser.add_argument("--delay", type=float, default=0.1, help="Delay in seconds between batch POSTs")
    args = parser.parse_args()

    if not os.path.exists(args.log_file):
        print(f"[ERROR] Log file not found: {args.log_file}")
        sys.exit(1)

    raw_records: List[Dict[str, Any]] = []
    file_dev_id = None
    file_boot_id = None

    with open(args.log_file, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                data = json.loads(line)
            except Exception:
                continue

            if "records" in data and isinstance(data["records"], list):
                if not file_dev_id:
                    file_dev_id = data.get("device_id")
                if not file_boot_id:
                    file_boot_id = data.get("boot_id")
                for r in data["records"]:
                    raw_records.append(r)
            else:
                if not file_dev_id:
                    file_dev_id = data.get("device_id")
                if not file_boot_id:
                    file_boot_id = data.get("boot_id")
                # Strip storage metadata if present
                clean_rec = {k: v for k, v in data.items() if k not in ("stored_at", "source", "device_id", "boot_id")}
                raw_records.append(clean_rec)

    target_dev_id = args.device_id or file_dev_id or "ESP32-RIG-REPLAY"
    target_boot_id = args.boot_id or file_boot_id or f"replay-{int(time.time())}"

    print(f"[INFO] Replaying {len(raw_records)} records to {args.server} for device '{target_dev_id}' (boot_id: '{target_boot_id}')...")

    headers = {
        "Content-Type": "application/json",
        "X-Device-Key": args.device_key
    }

    url = f"{args.server.rstrip('/')}/api/v1/devices/{target_dev_id}/telemetry"

    total_posted = 0
    total_duplicates = 0
    total_rejected = 0

    for i in range(0, len(raw_records), args.batch_size):
        chunk = raw_records[i : i + args.batch_size]
        payload = {
            "device_id": target_dev_id,
            "boot_id": target_boot_id,
            "sent_at": datetime.now(timezone.utc).isoformat(),
            "source": "replay",
            "records": chunk
        }

        try:
            resp = requests.post(url, json=payload, headers=headers, timeout=10)
            if resp.status_code == 200:
                res = resp.json()
                total_posted += res.get("received_count", len(chunk))
                total_duplicates += res.get("duplicates", 0)
                rej = res.get("rejected", [])
                total_rejected += len(rej)
                print(f"  Batch {i//args.batch_size + 1}: ACK up to seq {res.get('ack_up_to_seq')}, duplicates: {res.get('duplicates')}, rejected: {len(rej)}")
            else:
                print(f"[ERROR] Server responded with {resp.status_code}: {resp.text}")
        except Exception as e:
            print(f"[ERROR] HTTP request failed: {e}")

        if args.delay > 0:
            time.sleep(args.delay)

    print("\n--- Replay Summary ---")
    print(f"Total Records Transmitted: {len(raw_records)}")
    print(f"Total Processed by Server: {total_posted}")
    print(f"Duplicates Detected:       {total_duplicates}")
    print(f"Rejected Records:          {total_rejected}")
    print("[OK] Replay finished.")

if __name__ == "__main__":
    main()
