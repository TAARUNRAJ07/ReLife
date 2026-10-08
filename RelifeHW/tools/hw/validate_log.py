#!/usr/bin/env python3
"""
ReLife Telemetry Log Validator (validate_log.py)
Validates a JSONL telemetry log against contracts/v1/telemetry.schema.json.
Calculates valid percentage, sequence gaps, duplicates, time drift, sampling jitter (std of dt), and quality flags.
"""
import sys
import os
import json
import math
import argparse
from datetime import datetime, timezone
from typing import Dict, List, Any, Tuple
import jsonschema

DEFAULT_SCHEMA_PATH = os.path.join(
    os.path.dirname(__file__), "..", "..", "contracts", "v1", "telemetry.schema.json"
)

def parse_iso8601(ts_str: str) -> datetime:
    # Handle Zulu / UTC offset
    if ts_str.endswith("Z"):
        ts_str = ts_str[:-1] + "+00:00"
    return datetime.fromisoformat(ts_str)

def main():
    parser = argparse.ArgumentParser(description="ReLife Telemetry Log Validator")
    parser.add_argument("log_file", help="Path to JSONL telemetry log file")
    parser.add_argument("--schema", default=DEFAULT_SCHEMA_PATH, help="Path to telemetry JSON schema")
    args = parser.parse_args()

    if not os.path.exists(args.log_file):
        print(f"[ERROR] Log file not found: {args.log_file}")
        sys.exit(1)

    schema_path = os.path.abspath(args.schema)
    if not os.path.exists(schema_path):
        print(f"[ERROR] Schema file not found: {schema_path}")
        sys.exit(1)

    with open(schema_path, "r", encoding="utf-8") as f:
        full_schema = json.load(f)

    # Resolve TelemetryRecord sub-schema for standalone record validation
    record_schema = full_schema.get("definitions", {}).get("TelemetryRecord", full_schema)

    total_lines = 0
    valid_records = 0
    invalid_records = 0
    validation_errors = []

    # Per (device_id, boot_id) tracking
    streams: Dict[Tuple[str, str], List[Dict[str, Any]]] = {}
    quality_flag_counts: Dict[str, int] = {}

    with open(args.log_file, "r", encoding="utf-8") as f:
        for line_num, line in enumerate(f, start=1):
            line = line.strip()
            if not line:
                continue
            total_lines += 1
            try:
                data = json.loads(line)
            except Exception as e:
                invalid_records += 1
                validation_errors.append((line_num, f"Malformed JSON: {e}"))
                continue

            # Support batch format, individual record format, and stored JSONL logs
            records_to_check = []
            if "records" in data and isinstance(data["records"], list):
                dev_id = data.get("device_id", "unknown")
                b_id = data.get("boot_id", "unknown")
                for r in data["records"]:
                    records_to_check.append((dev_id, b_id, r))
            else:
                dev_id = data.get("device_id", "default")
                b_id = data.get("boot_id", "default")
                clean_rec = {k: v for k, v in data.items() if k not in ("stored_at", "source", "device_id", "boot_id")}
                records_to_check.append((dev_id, b_id, clean_rec))

            for dev_id, b_id, rec in records_to_check:
                try:
                    jsonschema.validate(instance=rec, schema=record_schema)
                    valid_records += 1
                except jsonschema.ValidationError as err:
                    invalid_records += 1
                    validation_errors.append((line_num, f"Schema error at {list(err.path)}: {err.message}"))
                    continue

                # Quality flags
                flags = rec.get("quality_flags", [])
                for flg in flags:
                    quality_flag_counts[flg] = quality_flag_counts.get(flg, 0) + 1

                # Track stream data
                stream_key = (dev_id, b_id)
                if stream_key not in streams:
                    streams[stream_key] = []
                streams[stream_key].append(rec)

    print("==================================================")
    print("           RELIFE TELEMETRY VALIDATION REPORT     ")
    print("==================================================")
    print(f"Log File: {args.log_file}")
    print(f"Total Lines / Records Processed: {total_lines}")
    print(f"Valid Records:   {valid_records}")
    print(f"Invalid Records: {invalid_records}")

    valid_pct = (valid_records / (valid_records + invalid_records) * 100.0) if (valid_records + invalid_records) > 0 else 0.0
    print(f"Validation Pass Rate: {valid_pct:.2f}%")

    if validation_errors:
        print("\n--- Validation Errors (First 5) ---")
        for l_num, err_msg in validation_errors[:5]:
            print(f"  Line {l_num}: {err_msg}")
        if len(validation_errors) > 5:
            print(f"  ... and {len(validation_errors) - 5} more errors.")

    print("\n--- Quality Flag Counts ---")
    if quality_flag_counts:
        for flg, count in sorted(quality_flag_counts.items(), key=lambda x: x[1], reverse=True):
            print(f"  {flg}: {count}")
    else:
        print("  (No quality flags recorded)")

    print("\n--- Stream Integrity & Timing Statistics ---")
    for (dev_id, b_id), recs in streams.items():
        print(f"\n[Stream: Device={dev_id}, Boot={b_id}] ({len(recs)} records)")
        seqs = [r.get("seq") for r in recs if r.get("seq") is not None]
        
        # Check duplicates
        seen_seqs = set()
        duplicates = []
        for s in seqs:
            if s in seen_seqs:
                duplicates.append(s)
            else:
                seen_seqs.add(s)

        # Check gaps
        seqs_sorted = sorted(seen_seqs)
        gaps = []
        if seqs_sorted:
            min_s, max_s = seqs_sorted[0], seqs_sorted[-1]
            for exp in range(min_s, max_s + 1):
                if exp not in seen_seqs:
                    gaps.append(exp)

        print(f"  Sequence Range: {seqs_sorted[0] if seqs_sorted else 'N/A'} -> {seqs_sorted[-1] if seqs_sorted else 'N/A'}")
        print(f"  Duplicate Seq Count: {len(duplicates)} {f'(Examples: {duplicates[:3]})' if duplicates else ''}")
        print(f"  Sequence Gap Count:  {len(gaps)} {f'(Gaps: {gaps[:5]})' if gaps else ''}")

        # Compute timestamp deltas & sampling jitter
        timestamps = []
        for r in recs:
            ts_str = r.get("timestamp")
            if ts_str:
                try:
                    timestamps.append(parse_iso8601(ts_str))
                except Exception:
                    pass

        if len(timestamps) >= 2:
            deltas_s = []
            non_monotonic = 0
            for i in range(1, len(timestamps)):
                dt = (timestamps[i] - timestamps[i-1]).total_seconds()
                if dt < 0:
                    non_monotonic += 1
                deltas_s.append(dt)

            mean_dt = sum(deltas_s) / len(deltas_s)
            variance = sum((dt - mean_dt) ** 2 for dt in deltas_s) / len(deltas_s)
            std_dt = math.sqrt(variance)

            print(f"  Sampling Period Mean: {mean_dt:.4f} s ({(1.0/mean_dt):.2f} Hz)" if mean_dt > 0 else f"  Sampling Period Mean: {mean_dt:.4f} s")
            print(f"  Sampling Jitter (Std Dev): {std_dt*1000.0:.2f} ms")
            print(f"  Time Drift / Non-monotonic steps: {non_monotonic}")
        else:
            print("  Insufficient timestamp data for jitter calculation.")

    print("\n==================================================")
    if invalid_records > 0:
        sys.exit(1)

if __name__ == "__main__":
    main()
