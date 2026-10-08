# ReLife Hardware MVP Scope & Architecture Specification

> **NOTICE: DRAFT FOR HUMAN ENGINEER REVIEW**  
> Every electrical design, calculation, limit, and architectural specification in this document is a draft and requires physical validation and sign-off by a qualified human engineer before live battery energization.

---

## 1. System Overview & Hardware MVP

The **ReLife Hardware Test Rig (Phase HS)** is an instrumented low-voltage evaluation platform designed to characterize retired lithium-ion battery packs for second-life decision support. 

### 1.1 In-Scope Hardware MVP
- **Battery Pack:** 4S (4-series) low-voltage lithium-ion / LFP battery module.
  - Cell chemistry: `<<FROM_DATASHEET: cell_chemistry>>`
  - Nominal cell model: `<<FROM_DATASHEET: cell_model>>`
  - Pack nominal capacity: `<<FROM_DATASHEET: nominal_capacity_ah>>` Ah
- **Instrumentation & Measurement Rig:**
  - Precision 4-channel cell voltage monitoring front-end (isolated from raw ESP32 GPIOs).
  - Pack terminal voltage divider network.
  - High-precision low-side or high-side shunt current sensor (convention: positive = discharge, negative = charge).
  - Multi-point temperature monitoring (cell body average, BMS FET heatsink, current shunt, ambient air).
  - Physical Emergency Stop (E-Stop) sense line.
- **ESP32 Telemetry & Communication Subsystem:**
  - Real-time FreeRTOS sampling task with fixed deterministic tick rate.
  - Autonomous hardware watchdog timer (3.0 second timeout).
  - Non-volatile flash/SD journal ensuring at-least-once delivery.
  - Wi-Fi telemetry client communicating with ReLife backend using `contracts/v1`.

---

## 2. Explicitly Out of Scope

To guarantee strict safety boundaries during Phase HS, the following features and topologies are strictly excluded:
1. **High-Voltage Packs (>60 V DC / Traction Packs):** Out of scope. The MVP only operates on low-voltage (4S, nominal ~12.8 V–16.8 V) configurations.
2. **Automated / Closed-Loop Charger Control:** Out of scope unless explicitly reviewed and signed off by a senior safety engineer. Charging must never be initiated autonomously by remote network commands.
3. **Over-The-Air (OTA) Firmware Execution:** Remote flashing of firmware binaries is disabled. All firmware updates require physical USB-UART connection.

---

## 3. Deliverables to the Software Team

The hardware engineering team delivers the following artifacts to the software platform team:

| Deliverable | Location | Description |
| :--- | :--- | :--- |
| **Battery Limits Profile** | `contracts/v1/battery_limits.yaml` | Safe operating limits (voltage cutoffs, maximum charge/discharge currents, thermal derating thresholds). |
| **Interface Contract Schema** | `contracts/v1/telemetry.schema.json` | Formal JSON schema for telemetry ingestion. |
| **OpenAPI Device Spec** | `contracts/v1/openapi-device.yaml` | Specification of the 4 device endpoints. |
| **Golden Reference Logs** | `data/golden/` | Calibrated, real/simulated JSONL test logs with known capacity and impedance ground truths. |
| **Sensor Calibration Tables** | `docs/calibration/` | Calibration matrices, polynomial constants, and versioning metadata (`calibration_version`). |

---

## 4. Non-Claims & Safety Separation (Rule 1)

### 4.1 Explicit Non-Claims
- **NOT Certified:** This test rig is an uncertified hackathon prototype / research test bench. It is NOT certified under UN 38.3, UL 1973, IEC 62619, or CE/FCC standards.
- **NOT a BMS Replacement:** The ESP32 is strictly a measurement and telemetry node. It does NOT replace the battery's dedicated hardware Battery Management System (BMS).

### 4.2 Autonomous Safety Architecture
The hardware safety layer operates independently of any firmware or network state:
1. **BMS + Primary Fuse + Physical E-Stop:** Maintain primary autonomous safety cutoff authority. If firmware crashes, hangs, or network fails, the BMS and fuse protect against thermal runaway or short circuits.
2. **Sole Power Interlock Function:** All relays, load switches, and power actuators are controlled exclusively through a single local safety interlock function (`SafetyInterlock::evaluateAndDrive()`). Network commands can never bypass local interlocks.

---

## 5. Hardware Interface & Telemetry Contract Summary

All telemetry records adhere strictly to `contracts/v1` conventions:
- **Discharge Current:** Positive convention ($I > 0$).
- **Units:** Volts (V), Amperes (A), Degrees Celsius (°C), Ampere-hours (Ah), Seconds (s).
- **Timestamps:** UTC ISO-8601 string (`YYYY-MM-DDTHH:MM:SS.sssZ`).
- **Battery Identifier:** Formatted as `^RL-BAT-[0-9]{4}$` (e.g., `RL-BAT-0042`).
- **Data Quality:** Every sensor read produces `{value, ok, error_code}`. Failed readings produce `null` values with explicit quality flags (never 0).
