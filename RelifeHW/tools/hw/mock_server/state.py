import os
import json
import time
import random
from typing import Dict, List, Set, Tuple, Optional, Any
from datetime import datetime, timezone

class FailureConfig:
    def __init__(self):
        self.fixed_latency_ms: float = 0.0
        self.random_500_rate: float = 0.0
        self.force_401: bool = False
        self.force_429: bool = False
        self.retry_after_s: int = 5
        self.drop_response_after_store: bool = False
        self.reject_next_n: int = 0

    def to_dict(self) -> Dict[str, Any]:
        return {
            "fixed_latency_ms": self.fixed_latency_ms,
            "random_500_rate": self.random_500_rate,
            "force_401": self.force_401,
            "force_429": self.force_429,
            "retry_after_s": self.retry_after_s,
            "drop_response_after_store": self.drop_response_after_store,
            "reject_next_n": self.reject_next_n,
        }

class MockServerState:
    def __init__(self, storage_dir: str = "data", valid_device_key: str = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"):
        self.storage_dir = storage_dir
        os.makedirs(self.storage_dir, exist_ok=True)
        self.jsonl_path = os.path.join(self.storage_dir, "telemetry_store.jsonl")
        self.valid_device_key = valid_device_key
        
        # Memory indexes
        # stored_records: set of (device_id, boot_id, seq)
        self.stored_records: Set[Tuple[str, str, int]] = set()
        # boot_sequences: dict of (device_id, boot_id) -> set of stored seqs
        self.boot_sequences: Dict[Tuple[str, str], Set[int]] = {}
        # heartbeats: dict of device_id -> latest heartbeat dict
        self.heartbeats: Dict[str, Dict[str, Any]] = {}
        # commands: dict of device_id -> list of command dicts
        self.commands: Dict[str, List[Dict[str, Any]]] = {}
        # command_history: dict of command_id -> ack dict
        self.command_history: Dict[str, Dict[str, Any]] = {}
        
        self.failure_config = FailureConfig()
        
        # Load existing store if present
        self._load_existing_store()

    def _load_existing_store(self):
        if os.path.exists(self.jsonl_path):
            with open(self.jsonl_path, "r", encoding="utf-8") as f:
                for line in f:
                    line = line.strip()
                    if not line:
                        continue
                    try:
                        record = json.loads(line)
                        dev_id = record.get("device_id")
                        boot_id = record.get("boot_id")
                        seq = record.get("seq")
                        if dev_id and boot_id and seq is not None:
                            key = (dev_id, boot_id, int(seq))
                            self.stored_records.add(key)
                            b_key = (dev_id, boot_id)
                            if b_key not in self.boot_sequences:
                                self.boot_sequences[b_key] = set()
                            self.boot_sequences[b_key].add(int(seq))
                    except Exception:
                        pass

    def calculate_ack_up_to_seq(self, device_id: str, boot_id: str) -> int:
        """Returns the highest consecutive sequence number stored starting from seq 1."""
        seqs = self.boot_sequences.get((device_id, boot_id), set())
        if not seqs:
            return 0
        curr = 1
        while curr in seqs:
            curr += 1
        return curr - 1

    def store_telemetry_batch(self, payload: Dict[str, Any]) -> Tuple[int, int, int, List[Dict[str, Any]]]:
        """
        Stores telemetry records idempotently.
        Returns: (ack_up_to_seq, received_count, duplicates, rejected)
        """
        device_id = payload["device_id"]
        boot_id = payload["boot_id"]
        records = payload.get("records", [])
        
        received_count = len(records)
        duplicates = 0
        rejected: List[Dict[str, Any]] = []
        new_records_to_write = []

        b_key = (device_id, boot_id)
        if b_key not in self.boot_sequences:
            self.boot_sequences[b_key] = set()

        for rec in records:
            seq = rec["seq"]
            # Check failure injection for rejection
            if self.failure_config.reject_next_n > 0:
                self.failure_config.reject_next_n -= 1
                rejected.append({"seq": seq, "reason": "Simulated rejection via failure injection"})
                continue

            rec_key = (device_id, boot_id, seq)
            if rec_key in self.stored_records:
                duplicates += 1
            else:
                self.stored_records.add(rec_key)
                self.boot_sequences[b_key].add(seq)
                record_entry = {
                    "device_id": device_id,
                    "boot_id": boot_id,
                    "source": payload.get("source", "live"),
                    "stored_at": datetime.now(timezone.utc).isoformat(),
                    **rec
                }
                new_records_to_write.append(record_entry)

        if new_records_to_write:
            with open(self.jsonl_path, "a", encoding="utf-8") as f:
                for entry in new_records_to_write:
                    f.write(json.dumps(entry) + "\n")

        ack_seq = self.calculate_ack_up_to_seq(device_id, boot_id)
        return ack_seq, received_count, duplicates, rejected

    def store_heartbeat(self, device_id: str, payload: Dict[str, Any]):
        self.heartbeats[device_id] = {
            "received_at": datetime.now(timezone.utc).isoformat(),
            **payload
        }

    def enqueue_command(self, device_id: str, command: str, params: Optional[Dict[str, Any]] = None) -> str:
        if device_id not in self.commands:
            self.commands[device_id] = []
        
        cmd_id = f"CMD-{int(time.time()*1000)}-{random.randint(100, 999)}"
        cmd_entry = {
            "command_id": cmd_id,
            "command": command,
            "params": params or {},
            "created_at": datetime.now(timezone.utc).isoformat()
        }
        self.commands[device_id].append(cmd_entry)
        return cmd_id

    def get_pending_commands(self, device_id: str) -> List[Dict[str, Any]]:
        # Polling returns the current queued commands and clears queue or leaves them until acked
        # Per standard polling pattern, return pending commands
        cmds = self.commands.get(device_id, [])
        self.commands[device_id] = []
        return cmds

    def acknowledge_command(self, command_id: str, status: str, error: Optional[str] = None):
        self.command_history[command_id] = {
            "status": status,
            "error": error,
            "acknowledged_at": datetime.now(timezone.utc).isoformat()
        }

    def reset_state(self):
        self.stored_records.clear()
        self.boot_sequences.clear()
        self.heartbeats.clear()
        self.commands.clear()
        self.command_history.clear()
        if os.path.exists(self.jsonl_path):
            try:
                os.remove(self.jsonl_path)
            except Exception:
                pass
