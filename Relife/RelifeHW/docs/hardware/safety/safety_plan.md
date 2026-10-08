# ReLife Hardware Safety Plan (Phase HS)

> **NOTICE: DRAFT FOR HUMAN ENGINEER REVIEW**  
> This safety plan is a draft engineering document. All hazard controls, abort thresholds, PPE guidelines, and physical containment protocols MUST be reviewed, physically verified, and approved by a qualified electrical/safety engineer before connecting or energizing any battery cells.

---

## 1. Safety Architecture & Separation of Concerns (Rule 1)

Safety in the ReLife platform is built on strict physical and functional layer separation. Under no circumstances may software or network systems be relied upon to prevent hazardous battery conditions.

```
+-------------------------------------------------------------------------+
|                        LAYER 1: HARDWARE SAFETY                         |
|  - Autonomous Hardware BMS (Overvoltage, Undervoltage, Overtemp cutoffs)|
|  - Primary In-Line DC Fuse (Fast-blow, short-circuit protection)        |
|  - Physical Emergency Stop (E-Stop) Switch (Direct coil interrupter)    |
|  * OPERATES 100% INDEPENDENTLY WITH ZERO SOFTWARE OR MCU RUNNING *       |
+-------------------------------------------------------------------------+
                                    |
+-------------------------------------------------------------------------+
|                    LAYER 2: EMBEDDED TELEMETRY & RIG                    |
|  - ESP32 Microcontroller: Sensor sampling, SD journal, TLS telemetry   |
|  - Local Interlock Function (SafetyInterlock::evaluateAndDrive)         |
|  - Hardware Watchdog Timer (3.0s auto-trip on firmware freeze)          |
|  * READ-ONLY TELEMETRY / LOCAL ACTUATION STRICTLY INTERLOCKED *         |
+-------------------------------------------------------------------------+
                                    |
+-------------------------------------------------------------------------+
|                    LAYER 3: CLOUD & AI DECISION SUPPORT                 |
|  - Health diagnostics, State of Health (SOH) estimation, second-life ML|
|  * ZERO CONTROL AUTHORITY OVER HARDWARE ACTUATION OR RELAYS *           |
+-------------------------------------------------------------------------+
```

### 1.1 Strict Boundary Rules
1. **BMS = Autonomous Safety Authority:** The battery pack BMS, primary fuse, and hardwired E-Stop form the unconditional safety boundary. They must prevent cell damage, fire, or thermal runaway even if the ESP32 is powered off, hanging, or destroyed.
2. **ESP32 = Measurement & Local Gatekeeper:** The ESP32 measures voltages, currents, temperatures, and states. Remote network commands can NEVER close a relay, bypass a local cutoff, or override safety limits.
3. **AI / Software = Decision Support Only:** Cloud/backend systems provide analytical insights and diagnostic scores. They possess ZERO control authority over power cutoffs.

---

## 2. Hazard Identification & Engineering Controls

| Hazard | Description / Failure Mode | Engineering Controls & Mitigation | Secondary Redundancy |
| :--- | :--- | :--- | :--- |
| **Overcharge** | Excessive voltage applied to cell during charging leading to lithium plating, electrolyte decomposition, and thermal runaway. | - Dedicated CC-CV charger with hard-set maximum terminal voltage of `<<FROM_DATASHEET: max_pack_charge_voltage_v>>` V.<br>- Hardware BMS overvoltage cutoff at `<<FROM_DATASHEET: cell_overvoltage_cutoff_v>>` V per cell. | - ESP32 local interlock trips load/charge relay if any cell reaches `<<FROM_DATASHEET: cell_overvoltage_warning_v>>` V.<br>- Constant human supervision during charging. |
| **Over-discharge** | Discharging below minimum cell potential causing copper dissolution and internal short circuits. | - BMS undervoltage protection cutoff at `<<FROM_DATASHEET: cell_undervoltage_cutoff_v>>` V per cell.<br>- Programmable electronic load voltage cutoff set to `<<FROM_DATASHEET: pack_undervoltage_cutoff_v>>` V. | - ESP32 firmware opens discharge contactor at `<<FROM_DATASHEET: cell_undervoltage_warning_v>>` V per cell. |
| **Short Circuit** | External bridging of positive and negative terminals generating extreme currents ($I > 100\text{ A}$). | - Primary fast-acting DC fuse rated at `<<FROM_DATASHEET: primary_fuse_rating_a>>` A placed directly at pack positive terminal.<br>- Fully insulated test leads and shrouded 4mm banana / XT90 connectors. | - BMS electronic short-circuit protection ($< 500\,\mu\text{s}$ response). |
| **Thermal Runaway** | Exothermic reaction chain driven by internal defect, overtemperature, or severe overcharge. | - BMS overtemperature cutoff sensor set to `<<FROM_DATASHEET: max_charge_temp_c>>` °C (charge) / `<<FROM_DATASHEET: max_discharge_temp_c>>` °C (discharge).<br>- Rig multi-point temperature probes on all cell bodies and current shunt. | - Testing performed inside fire-resistant LiPo safe charging box / steel enclosure.<br>- Sand bucket and Class D / Dry Powder fire extinguisher stationed adjacent to bench. |
| **Venting / Toxic Gas & Fire** | Ejection of flammable organic solvent electrolytes (DMC, EMC, EC) and toxic HF gases. | - Dedicated testing location with active mechanical fume extraction or open-air cross-ventilation.<br>- Physical isolation from flammable materials ($> 3\text{ m}$ clearance). | - Smoke and gas detection alarm mounted directly above test rig. |
| **Electric Shock** | Contact with energized terminals. (Though 4S is low voltage $< 20\text{ V}$, arcs and secondary burns can occur). | - All exposed conductor points covered with non-conductive Kapton / silicone insulation.<br>- 3.3V logic isolation from battery high-current loops. | - Star-grounding topology with single-point chassis bonding. |
| **Mechanical Damage** | Cell puncturing, crushing, drop impact, or terminal stress leading to internal separator rupture. | - Rigid pack mechanical retention frame / clamp.<br>- Strain-relieved wiring harness and heavy-gauge silicone leads. | - Routine visual inspection for swelling, denting, or corrosion before every test cycle. |

---

## 3. Test Location, Facilities & PPE

### 3.1 Facility Requirements
- **Location:** Dedicated, non-combustible workbench (concrete, steel, or ceramic surface).
- **Clearance:** Minimum 3 meters clearance in all directions from combustible materials, paper, cardboard, solvents, or chemicals.
- **Ventilation:** Active mechanical fume exhaust hood or well-ventilated outdoor sheltered area.
- **Fire Safety Equipment Station:**
  - Standard Class D metal fire extinguisher or ABC Dry Chemical extinguisher located within 2 meters.
  - Dedicated metal container filled with clean, dry industrial sand (minimum 10 liters) with a heavy-duty metal scoop.
  - Heavy-duty thermal fire blanket (minimum $1.2\text{ m} \times 1.8\text{ m}$).
  - Standalone optical smoke detector positioned directly above the test bench.

### 3.2 Required Personal Protective Equipment (PPE)
Every person within the test cell perimeter must wear:
1. **Eye & Face Protection:** ANSI Z87.1 approved impact-resistant safety glasses with side shields. Full-face shield required during initial pack connection and first energization.
2. **Hand Protection:** Cut-resistant insulated electrical safety gloves (minimum Class 00, 500V rated) and heavy heat-resistant leather over-gloves.
3. **Body Protection:** 100% natural fiber (cotton / wool) or flame-retardant (FR) lab coat. Synthetic fabrics (polyester, nylon) are strictly forbidden due to melt hazards.
4. **Footwear:** Closed-toe leather safety shoes.

---

## 4. Operational Safety Policies

### 4.1 Prohibition on Unattended Testing
> [!CAUTION]
> **NO UNATTENDED CHARGING OR DISCHARGING IS PERMITTED.**  
> A qualified, designated test operator must be physically present at the test bench throughout the entirety of any charging, capacity discharge, or pulse test cycle. If the operator must leave the area, the test MUST be paused or aborted, and the contactor opened.

### 4.2 Two-Person Rule for First Power-Up
Before any battery pack is connected to the test rig or energized for the first time:
- A **minimum of two persons** (the primary test operator and a designated safety peer reviewer) must be present.
- Both persons must independently review and sign off on the [Pre-Power-Up Verification Checklist](#8-pre-power-up-verification-checklist).
- The secondary person must hold the physical E-Stop switch or be positioned with immediate access to the main disconnect.

### 4.3 Strict Prohibition on Dismantling EV High-Voltage Packs
> [!WARNING]
> **PROHIBITION:** The ReLife hardware test rig is strictly designed for modular 4S low-voltage battery packs ($< 25\text{ V}$).  
> Under NO circumstances may personnel attempt to dismantle, tap, or probe high-voltage electric vehicle (EV) traction battery packs ($> 60\text{ V}\text{ DC}$) without specialized high-voltage certified training, isolated tooling, and dedicated industrial high-voltage facilities.

---

## 5. Abort Conditions & Emergency Procedures

### 5.1 Immediate Test Abort Conditions
A test must be immediately aborted (E-Stop pressed, load disconnected) if any of the following occur:
1. **Any cell voltage exceeds `<<FROM_DATASHEET: cell_overvoltage_cutoff_v>>` V** during charging.
2. **Any cell voltage drops below `<<FROM_DATASHEET: cell_undervoltage_cutoff_v>>` V** during discharge.
3. **Any temperature sensor reads $> <<FROM_DATASHEET: max_abort_temperature_c>>` °C** or rises faster than $2.0\text{ °C/minute}$.
4. **Cell voltage imbalance exceeds `<<FROM_DATASHEET: max_cell_delta_v>>` V** between the highest and lowest cell.
5. **Physical abnormalities observed:** Cell swelling/pouch puffing, hissing sound, popping, chemical odor, visible smoke, or terminal discoloration.
6. **Loss of sensor telemetry:** ESP32 reports `QUALITY_FLAG_SENSOR_FAULT` or ADC saturation.

### 5.2 Emergency Response Procedure (Fire or Venting)
If smoke, sparks, venting, or flames occur:
1. **STEP 1: HIT EMERGENCY STOP.** Immediately slam the physical E-Stop button and disconnect AC mains power to all bench equipment.
2. **STEP 2: EVACUATE & ISOLATE.** Alert all room occupants and step back to a safe distance ($> 5\text{ m}$).
3. **STEP 3: DEPLOY FIRE MITIGATION.**
   - For small smoke/early runaway: Smother with dry sand or thermal fire blanket using insulated tongs/gloves.
   - For open flame: Discharge Class D / ABC dry powder extinguisher directly at the base of the fire.
   - *Never use water on exposed lithium-metal or uncontained high-energy cells.*
4. **STEP 4: ACTIVATE BUILDING ALARM & VENTILATION.** Turn on emergency exhaust, pull building fire alarm, and contact emergency services.

---

## 6. Pack Storage, Handling & Transport

- **Storage State of Charge (SOC):** All retired cells and packs in storage must be maintained at nominal storage voltage (`<<FROM_DATASHEET: nominal_storage_voltage_v>>` V/cell, approx. 30%–50% SOC). Never store packs fully charged (100%) or deeply discharged (0%).
- **Storage Enclosure:** Store in heavy-gauge grounded steel battery cabinets or explosion-resistant LiPo containers.
- **Terminal Insulation:** Every stored cell or pack must have its terminals insulated with non-conductive terminal caps or heavy Kapton tape.
- **Transport:** When moving packs between workstations, use dedicated non-conductive, padded transport containers. Never carry cells loose in pockets, bags, or with conductive tools.

---

## 7. Incident Log Template

In the event of an electrical trip, unexpected heating, sensor failure, or emergency abort, the operator must complete the following incident record:

```markdown
### ReLife Safety Incident & Anomaly Report

- **Date / Time (UTC):** ____________________
- **Battery Pack ID:** RL-BAT-______
- **Operator Name:** ____________________
- **Second Safety Reviewer:** ____________________
- **Active Test Phase:** [ ] Charge  [ ] Rest  [ ] CC Discharge  [ ] Pulse Test  [ ] Idle
- **Anomaly Description:**
  __________________________________________________________________________
  __________________________________________________________________________

- **Telemetry at Time of Incident:**
  - Pack Voltage: ________ V
  - Cell Voltages: [Cell 1: ___ V, Cell 2: ___ V, Cell 3: ___ V, Cell 4: ___ V]
  - Pack Current: ________ A
  - Temperatures: [Cell Avg: ___ °C, BMS: ___ °C, Shunt: ___ °C, Ambient: ___ °C]
  - Interlock State: ____________________

- **Action Taken:**
  [ ] E-Stop Pressed  [ ] Manual Load Disconnect  [ ] Sand/Blanket Deployed  [ ] Fire Extinguisher Used

- **Root Cause Assessment:**
  __________________________________________________________________________

- **Corrective Actions Required Before Resuming Work:**
  1. ______________________________________________________________________
  2. ______________________________________________________________________

- **Sign-Off:**
  - Operator Signature: ______________________ Date: ____________
  - Safety Lead Signature: ____________________ Date: ____________
```

---

## 8. Pre-Power-Up Verification Checklist

Before connecting battery leads or energizing the test rig, both engineers must complete and sign this checklist:

| Check Item | Requirement | Verified (Op) | Verified (Peer) |
| :--- | :--- | :---: | :---: |
| **1. Visual Pack Inspection** | No swelling, denting, punctures, leakage, or corrosion on cell bodies. | [ ] | [ ] |
| **2. Primary Fuse Integrity** | Fast-acting DC fuse (`<<FROM_DATASHEET: primary_fuse_rating_a>>` A) installed in-line at battery positive terminal. | [ ] | [ ] |
| **3. Hardware BMS Active** | Dedicated BMS connected, balance leads seated, autonomous cutoff functioning. | [ ] | [ ] |
| **4. Physical E-Stop Test** | E-Stop mechanically tested; pressing E-Stop physically breaks load/charge contactor coil power. | [ ] | [ ] |
| **5. Polarity & Insulation** | Terminal polarity verified with calibrated DMM; all non-terminated leads insulated. | [ ] | [ ] |
| **6. Grounding Topology** | Single-point star ground verified. No ground loops between ESP32, shunt, and load. | [ ] | [ ] |
| **7. Voltage Isolation** | Confirmed: Battery and cell taps NEVER connect directly to ESP32 MCU pins. | [ ] | [ ] |
| **8. Fire Safety Station** | Sand bucket, fire blanket, and extinguisher verified present within 2m. | [ ] | [ ] |
| **9. PPE Check** | Both engineers wearing safety glasses, flame-retardant lab coats, and safety shoes. | [ ] | [ ] |
| **10. Watchdog & Interlock** | Firmware watchdog verified active; local interlock software loaded. | [ ] | [ ] |

**Primary Operator Sign-Off:**  
Name: __________________________ Signature: __________________________ Date: ______________

**Second Safety Reviewer Sign-Off:**  
Name: __________________________ Signature: __________________________ Date: ______________
