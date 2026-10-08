# ReLife 1S Battery Cell & Protection Specification

> **NOTICE: DRAFT FOR HUMAN ENGINEER REVIEW**  
> All parameters, electrical thresholds, and operating envelopes in this document are extracted for a **1S single Lithium-Ion cell** (Nominal: 3.7 V, Capacity: 2000 mAh). Unspecified manufacturer limits are marked with `<<FROM_DATASHEET: item>>` or `<<MISSING>>`.

---

## 1. Battery Cell Specifications (1S Configuration)

- **Cell Type:** Single Lithium-Ion cylindrical/pouch cell (1S)
- **Cell Markings / Rating:** 3.7 V, 2000 mAh (2.0 Ah)
- **Cell Mechanical Form:** Bare cell (no integrated protection module)
- **Nominal Chemistry:** Lithium-Ion (NMC / LiCoO2 typical for 3.7 V nominal)

| Parameter | Value / Formula | Conditions | Datasheet Source / Status |
| :--- | :--- | :--- | :--- |
| **Rated Capacity ($C_{\text{nom}}$)** | $2000\text{ mAh}$ ($2.00\text{ Ah}$) | Standard discharge rate | Cell label: 2000 mA |
| **Nominal Cell Voltage** | $3.70\text{ V}$ | Open Circuit Voltage at ~50% SOC | Cell label: 3.7 V |
| **Full Charge Voltage ($V_{\text{max}}$)** | $4.20\text{ V}$ | CC-CV charging termination | Standard 3.7V Li-ion specification |
| **Discharge Cutoff Voltage ($V_{\text{min}}$)** | $2.75\text{ V}$ to $3.00\text{ V}$ | Continuous load cutoff | `<<FROM_DATASHEET: cell_discharge_cutoff_v>>` (Draft: 3.00 V) |
| **Standard Charge Current ($0.5C$)** | $1.00\text{ A}$ | $0.5 \times C_{\text{nom}} = 0.5 \times 2.0\text{ Ah}$ | Calculated |
| **Gentle Charge Current ($0.2C$)** | $0.40\text{ A}$ | $0.2 \times C_{\text{nom}} = 0.2 \times 2.0\text{ Ah}$ | Calculated (Recommended for retired cells) |
| **Charge Cutoff Current ($I_{\text{end}}$)** | $0.05\text{ A}$ to $0.10\text{ A}$ | $C/40$ to $C/20$ taper current | Standard CC-CV taper |
| **Standard Discharge Test Current ($0.5C$)**| $1.00\text{ A}$ | Constant Current (Positive convention) | Characterization test rate |
| **Max Continuous Discharge Current** | `<<FROM_DATASHEET: max_cont_discharge_a>>` A | Continuous discharge limit | Typically $1C = 2.0\text{ A}$ for energy cells; higher for power cells |
| **Operating Temp (Charge)** | $0\text{ °C}$ to $45\text{ °C}$ | Cold charging strictly prohibited ($< 0\text{ °C}$) | Standard Li-ion safety envelope |
| **Operating Temp (Discharge)** | $-10\text{ °C}$ to $55\text{ °C}$ | Maximum continuous skin temperature | Standard Li-ion safety envelope |

---

## 2. Mandatory External Safety Hardware (Bare Cell Requirement)

> [!CAUTION]
> **BARE CELL SAFETY HAZARD (Rule 1):**  
> Because this cell is a **bare cell** with no integrated internal protection board, it MUST be wired to an **external autonomous 1S Protection Circuit Module (PCM / 1S BMS)** before being placed on the test rig.  
> The ESP32 is a measurement device and CANNOT be the sole barrier against overcharge, over-discharge, or short-circuit.

### Required External Hardware Safety Components:
1. **1S Hardware Protection Board (PCM / 1S BMS):**
   - IC: Dedicated 1S battery protection (e.g., DW01A + FS8205A dual N-channel MOSFET, or equivalent).
   - Overcharge Cutoff: $4.28\text{ V} \pm 0.05\text{ V}$ (autonomous hardware disconnect).
   - Over-discharge Cutoff: $2.40\text{ V}$ to $2.80\text{ V}$ (autonomous load disconnect).
   - Overcurrent / Short Circuit: $< 3.0\text{ A}$ to $5.0\text{ A}$ ($< 10\text{ ms}$ response).
2. **Primary In-Line DC Fuse:**
   - Fast-acting DC fuse rated at **$3.0\text{ A}$** placed directly at cell positive terminal ($B+$).
   - Formula: $I_{\text{test}} \times 1.5 = 1.0\text{ A} \times 1.5 \approx 1.5\text{ A} \rightarrow 3.0\text{ A}$ fuse provides robust short-circuit protection while allowing $0.5C$ ($1.0\text{ A}$) test currents.
3. **Emergency Stop (E-Stop) Switch:**
   - Directly breaks the coil power to the rig load/charge disconnect relay.

---

## 3. Conflict & Margin Analysis: Cell Rating vs. Protection Thresholds

| Protection Parameter | Cell Safe Envelope | 1S PCM / Hardware Cutoff | Software Rig Warning Interlock | Status / Margin |
| :--- | :--- | :--- | :--- | :--- |
| **Overcharge Voltage** | $4.20\text{ V}$ (Max) | $4.28\text{ V} \pm 0.05\text{ V}$ (PCM trip) | $4.22\text{ V}$ (Contactor opened by ESP32) | SAFE: 3-tier defense (Charger limit $4.20\text{ V} \rightarrow$ Rig $4.22\text{ V} \rightarrow$ PCM $4.28\text{ V}$) |
| **Over-discharge Voltage**| $3.00\text{ V}$ (Cutoff) | $2.50\text{ V} \pm 0.10\text{ V}$ (PCM trip) | $3.00\text{ V}$ (Load opened by ESP32) | SAFE: Rig terminates test at $3.00\text{ V}$; PCM protects if load fails |
| **Discharge Current** | $2.00\text{ A}$ ($1C$ rating) | $3.5\text{ A} - 5.0\text{ A}$ (PCM DOCP) | $1.20\text{ A}$ (Overcurrent interlock) | SAFE: Rig trips above $1.20\text{ A}$; 3A fuse melts on short circuit |
| **Charge Temperature** | $0\text{ °C}$ to $45\text{ °C}$ | Thermal switch / probe | $45.0\text{ °C}$ (Rig charge abort) | SAFE: Cold charging blocked below $5\text{ °C}$ |
| **Discharge Temperature** | $-10\text{ °C}$ to $55\text{ °C}$ | Thermal switch / probe | $50.0\text{ °C}$ (Rig discharge abort) | SAFE: Rig trips $5\text{ °C}$ below maximum cell skin limit |

---

## 4. 1S Hardware Rig Interface & Grounding Topology

- **ESP32 Power:** Powered independently via isolated 5V USB (never from the 1S cell).
- **Voltage Measurement:** Single-ended voltage divider ($2:1$ divider using precision 0.1% $10\text{ k}\Omega$ resistors):
  $$V_{\text{ADC}} = V_{\text{cell}} \times \frac{10\text{ k}\Omega}{10\text{ k}\Omega + 10\text{ k}\Omega} = \frac{V_{\text{cell}}}{2}$$
  - At $V_{\text{cell}} = 4.20\text{ V}$, $V_{\text{ADC}} = 2.10\text{ V}$ (optimally centered in ESP32 ADC1 linear 0–2.5V region).
  - Connected exclusively to ESP32 **ADC1** (e.g., GPIO34 / GPIO35), avoiding ADC2.
- **Current Shunt:** $10\text{ m}\Omega$ or $50\text{ m}\Omega$ low-side current shunt.
- **Contract Payload Compatibility (`contracts/v1`):**
  - `pack_voltage_v`: $V_{\text{cell}}$ ($2.75\text{ V}$–$4.20\text{ V}$)
  - `cell_voltages_v`: `[V_cell, null, null, null]` (4-element array with cell 1 populated, cells 2–4 null).
  - `current_a`: Positive for discharge ($+1.0\text{ A}$), negative for charge ($-0.4\text{ A}$).
