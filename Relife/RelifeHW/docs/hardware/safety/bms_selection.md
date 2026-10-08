# Smart BMS Selection & Comparison Matrix

> **NOTICE: DRAFT FOR HUMAN ENGINEER REVIEW**  
> This evaluation matrix compares commercial smart Battery Management Systems (BMS) with digital telemetry interfaces. All electrical specifications, logic levels, and communication protocols MUST be physically bench-tested and verified with an oscilloscope or logic analyzer before interfacing with the ESP32.

---

## 1. Executive Summary & Architectural Motivation

In the ReLife hardware test rig, utilizing a **Smart BMS with digital serial telemetry (UART/RS485/CAN)** fulfills two core requirements simultaneously:
1. **Autonomous Hardware Safety Barrier (Rule 1):** The BMS provides independent, hardware-level cell overvoltage, undervoltage, overcurrent, and overtemperature cutoffs that function unconditionally without MCU software.
2. **Elimination of Custom Multi-Channel Analog Front-End (AFE):** By streaming calibrated individual cell voltages and temperatures digitally over UART, the ESP32 eliminates the need for complex, discrete high-voltage differential resistor dividers, multiplexers, or flying-capacitor circuits.

---

## 2. Comparison Matrix: Candidate Smart BMS Models

The following three commercial Smart BMS models were evaluated for low-voltage (4S) battery testing:

| Feature / Criteria | Candidate 1: JBD (Jiabaida) Smart BMS | Candidate 2: Daly Smart BMS | Candidate 3: ANT BMS (Smart 4S–8S) |
| :--- | :--- | :--- | :--- |
| **Exact Model** | **JBD-SP04S034** (or JBD-SP04S020) | **Daly Smart 4S 12V 30A/40A** | **ANT-BMS 4S–8S 30A** |
| **Supported Topologies**| 3S–4S Li-ion / LFP (configurable) | 4S Li-ion / LFP (factory fixed) | 4S–8S Li-ion / LFP (configurable) |
| **Rated Continuous Current**| 30 A (peak 60 A) | 30 A / 40 A (peak 100 A) | 30 A (peak 60 A) |
| **Current Margin vs Rig ($1.0\text{ A}$–$10\text{ A}$)** | $> 300\%$ margin (minimal FET $R_{\text{DS(on)}}$ thermal dissipation) | $> 300\%$ margin | $> 300\%$ margin |
| **Cell OVP (Overvoltage)** | $4.25\text{ V} \pm 0.025\text{ V}$ (programmable) | $4.25\text{ V} \pm 0.05\text{ V}$ (programmable) | $4.25\text{ V} \pm 0.05\text{ V}$ (programmable) |
| **Cell UVP (Undervoltage)**| $2.80\text{ V} \pm 0.05\text{ V}$ (programmable) | $2.70\text{ V} \pm 0.05\text{ V}$ (programmable) | $2.80\text{ V} \pm 0.05\text{ V}$ (programmable) |
| **Short-Circuit Protection**| $100\,\mu\text{s} - 400\,\mu\text{s}$ hardware latch | $< 500\,\mu\text{s}$ hardware latch | $< 300\,\mu\text{s}$ hardware latch |
| **Temperature Sensor Inputs** | **3 inputs:** 2 external NTC probes + 1 internal power FET sensor | **2 inputs:** 1 external NTC probe + 1 internal sensor | **3 inputs:** 2 external NTC probes + 1 internal sensor |
| **Balancing Method & Rate** | Passive bleeding: $35\text{ mA} - 50\text{ mA}$ | Passive bleeding: $30\text{ mA} \pm 5\text{ mA}$ | Passive / Active hybrid: $50\text{ mA} - 100\text{ mA}$ |
| **Digital Communication** | **UART (TTL)**, Bluetooth BLE, RS485 (optional) | **UART (TTL)**, RS485, CANbus, Bluetooth | **UART (TTL)**, Bluetooth BLE |
| **Logic Level (UART)** | **3.3 V Logic** (direct interface to ESP32 GPIO) | **Mixed 3.3V / 5.0V** (Requires level-shifting verification) | **3.3 V Logic** |
| **Protocol Documentation** | **Public & Open-Source:** Comprehensive JBD/Xiaoxiang binary register specification published | **Vendor Documented:** Daly 13-byte hex frame specification published | **Reverse-Engineered:** Binary packet protocol (140-byte status frame) |
| **Source & Reference Links** | [Overkill Solar JBD Protocol Spec](https://github.com/FurTrader/Overkill-Solar-BMS-Firmware) / [JBD Official](https://jiabaida-bms.com/) | [Daly BMS Official](https://www.dalybms.com/) / [Daly UART Protocol](https://github.com/robdobsn/DalyBMS) | [ANT BMS Documentation](https://github.com/syssi/esphome-ant-bms) |

---

## 3. Communication Protocol Deep Dive

### 3.1 JBD (Jiabaida / Xiaoxiang) Protocol
- **Physical Layer:** UART, 9600 baud, 8 data bits, 1 stop bit, no parity (8N1).
- **Logic Level:** Standard 3.3V CMOS logic (directly compatible with ESP32 GPIO16/17 UART2 without level shifters).
- **Framing Structure:**
  - Start Byte: `0xDD`
  - Command Type: `0xA5` (Read) / `0x5A` (Write)
  - Register Address:
    - `0x03`: Read Basic System Info (Total pack voltage, current, residual capacity, SOC, protection status bits, cycle count, temperatures).
    - `0x04`: Read Individual Cell Voltages (Returns array of 2-byte mV integers per cell: $V_1, V_2, V_3, V_4$).
    - `0x05`: Read Hardware Version & Device Name.
  - Length Byte: Payload length $N$
  - Checksum: Two-byte checksum calculated as `0x10000 - Sum(Bytes)`.
  - End Byte: `0x77`
- **Driver Ecosystem:** Mature open-source drivers exist across Arduino, ESP-IDF, Python, and Linux.

### 3.2 Daly Smart Protocol
- **Physical Layer:** UART / RS485, 9600 baud, 8N1.
- **Logic Level:** Varies by hardware revision; some Daly units supply 5V on the communication header or output 5V TTL signals, creating an overvoltage risk for raw ESP32 3.3V inputs without an inline resistor divider or bidirectional level shifter.
- **Framing Structure:** Fixed 13-byte binary command/response packets:
  - Start Byte: `0xA5`
  - Host Address: `0x40`
  - Command ID: `0x90` (V/I/SOC), `0x91` (Max/Min cell voltage), `0x95` (Cell voltages frame), `0x96` (Temperatures).
  - Data Length: `0x08` (always 8 bytes payload)
  - Checksum: 1-byte low 8 bits sum.
- **Drawback:** Daly units require polling individual frames sequentially (one request packet per 3 cells), requiring multi-turn query state machines.

### 3.3 ANT BMS Protocol
- **Physical Layer:** UART, 19200 baud, 8N1.
- **Framing Structure:** Streams large 140-byte binary status blocks upon request or periodically.
- **Drawback:** Protocol is largely community-reverse engineered without formal vendor documentation. Firmware revisions frequently alter register offsets.

---

## 4. Engineering Recommendation

### Primary Recommendation: **JBD-SP04S034 (Jiabaida / Xiaoxiang 4S 30A)**

#### Technical Justification:
1. **Fully Documented Open Protocol:** The JBD protocol is completely documented and publicly verifiable. Querying register `0x04` returns all individual cell voltages in a single transaction with a robust 16-bit checksum.
2. **Native 3.3 V Logic Compatibility:** The UART connector on JBD boards operates at 3.3V logic, allowing direct, clean connection to ESP32 hardware UART pins without requiring active level-shifter ICs.
3. **Multi-Point Temperature Sensors:** Comes equipped with **two external NTC probes** (for mounting directly on the battery cell bodies and current shunt) plus an internal temperature sensor monitoring the BMS power MOSFETs.
4. **Current Rating Margin:** Rated for $30\text{ A}$ continuous. For our $1.0\text{ A}$–$2.0\text{ A}$ capacity test load, the BMS power MOSFETs will operate near ambient temperature ($< 0.1\text{ W}$ dissipation), preventing thermal drift.
5. **Configurability:** Allows programmable OVP/UVP thresholds, allowing us to align the autonomous BMS cutoff with the specific cell chemistry.

---

## 5. Risk Analysis & Integration Warnings

> [!CAUTION]
> **SAFETY & INTEGRATION RISKS:**
> 1. **Ground Loop & Floating Potentials:**  
>    The BMS switches the **negative terminal ($B-$ to $P-$)** using low-side N-channel MOSFETs. When the BMS disconnects under a fault, $P-$ floats up towards $B+$.  
>    **RULE:** The ESP32 logic ground MUST be bonded strictly to the battery side ground ($B-$), NOT the switched load ground ($P-$). Optocouplers or digital isolators (e.g., ADuM1201 or PC817) are strongly recommended on the UART RX/TX lines to prevent ground bounce.
> 2. **Minimum Cell Count for 1S Testing:**  
>    Multi-cell smart BMSs (JBD, Daly, ANT) are designed for a minimum of **3S or 4S** series connections (total pack voltage $\ge 9.0\text{ V}$). They CANNOT operate or power their internal microcontroller from a single 1S cell ($3.7\text{ V}$).  
>    - If testing **1S single cells**, an external hardware 1S PCM (e.g. DW01A) is used for safety cutoffs, and digital telemetry is measured via an I2C ADC (e.g., ADS1115 / INA226).  
>    - If testing **4S packs**, the JBD-SP04S034 connects directly across all 4 cells.
> 3. **Logic Level Verification:**  
>    Before plugging the BMS UART header into the ESP32, verify with a multimeter that the idle UART TX line measures $\le 3.30\text{ V}$. If $5.0\text{ V}$ is detected, install a $1.2\text{ k}\Omega : 2.0\text{ k}\Omega$ voltage divider.
