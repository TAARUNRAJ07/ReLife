# ReLife Test Rig Wiring & Electrical Interconnect Specification

> **NOTICE: DRAFT FOR HUMAN ENGINEER REVIEW**  
> Every electrical connection, wire gauge, fuse rating, ground topology, and contactor circuit in this document is a draft and requires physical review and verification by a qualified electrical engineer prior to assembly or energization.

---

## 1. Electrical Architecture & Required Chain

> **DRAFT FOR HUMAN ENGINEER REVIEW**

The high-current power circuit follows a strict, non-negotiable physical topology. All components are connected in direct serial sequence from the battery terminals to the load/charger bus:

```
[Battery Positive (B+)]
         │
         ▼
[1. Primary Fast DC Fuse (3A / 32V DC, < 50mm from B+)]
         │
         ▼
[2. Manual DC Isolation Switch (Lockable disconnect)]
         │
         ▼
[3. Emergency Stop (E-Stop) Contact (Physical circuit break)]
         │
         ▼
[4. Safety Interlock Relay / DC Contactor (Controlled exclusively by ESP32 Interlock)]
         │
         ▼
[Load / Charger Positive Bus (P+)] ───► [Programmable Load / CC-CV Charger (+)]
                                                      │
[Load / Charger Negative Bus (P-)] ◄─── [Programmable Load / CC-CV Charger (-)]
         │
         ▼
[5. Precision Current Shunt (10 mΩ, 4-terminal Kelvin sensing)]
         │
         ▼
[6. Battery Management System (BMS / PCM Low-Side Solid-State Switch)]
         │
         ▼
[Battery Negative (B-)] ◄─── [STAR GROUND REFERENCE POINT]
```

---

## 2. Component Sizing & Engineering Calculations

> **DRAFT FOR HUMAN ENGINEER REVIEW**

### 2.1 Primary DC Fuse Sizing & Interrupt Rating
The primary fuse protects against catastrophic short-circuit conditions, insulation breakdown, and external contact failures.

- **Design Inputs:**
  - Rated Cell Capacity: $C_{\text{nom}} = 2.00\text{ Ah}$
  - Standard Characterization Discharge Current: $I_{\text{dis}} = 1.00\text{ A}$ ($0.5C$)
  - Maximum Allowable Continuous Operating Current: $I_{\text{max\_cont}} = 2.00\text{ A}$ ($1.0C$)
  - Maximum Open Circuit Voltage: $V_{\text{max}} = 4.20\text{ V}$ (1S) / $16.80\text{ V}$ (4S)
  - Estimated Internal Resistance: $R_{\text{cell\_internal}} \approx 35\text{ m}\Omega$
  - Test Lead Resistance: $R_{\text{leads}} \approx 25\text{ m}\Omega$

- **Continuous Current Rating Selection:**
  The fuse rating must exceed the maximum continuous operating current with a standard $125\%$ NEC continuous load factor to prevent nuisance tripping during prolonged discharge tests:
  $$I_{\text{fuse\_min}} = I_{\text{max\_cont}} \times 1.25 = 2.00\text{ A} \times 1.25 = 2.50\text{ A}$$
  **Selected Fuse Continuous Rating:** **$3.0\text{ A}$ Fast-Acting DC Blade / Cartridge Fuse** (Littelfuse 0297003 or equivalent).

- **Voltage Rating Selection:**
  DC arc extinguishing is fundamentally more demanding than AC because DC has no natural zero-crossing current point. The fuse MUST carry an explicit DC voltage rating:
  $$V_{\text{fuse\_DC}} \ge 1.5 \times V_{\text{max}} = 1.5 \times 4.20\text{ V} = 6.30\text{ V}$$
  **Selected Voltage Rating:** **$32\text{ V DC}$ (or $58\text{ V DC}$)** automotive/industrial DC rating.

- **Interrupting Capacity (AIC - Amperes Interrupting Capacity):**
  Prospective short-circuit current across zero-resistance fault:
  $$I_{\text{fault\_prospective}} = \frac{V_{\text{max}}}{R_{\text{cell\_internal}} + R_{\text{leads}}} = \frac{4.20\text{ V}}{0.035\,\Omega + 0.025\,\Omega} = \frac{4.20\text{ V}}{0.060\,\Omega} = 70.0\text{ A}$$
  For multi-cell packs, prospective fault current can exceed $300\text{ A}$. The fuse must safely interrupt without body rupture or sustained arc:
  $$I_{\text{AIC}} \ge 1000\text{ A DC}\quad (\text{Littelfuse Mini-Blade is rated for } 1000\text{ A @ } 32\text{ V DC})$$

---

### 2.2 Precision Current Shunt Resistor Sizing
The current shunt provides primary analog current feedback to the telemetry and safety interlock engine.

- **Design Inputs:**
  - Nominal Discharge Current: $I_{\text{nom}} = 1.00\text{ A}$
  - Peak Operating Current: $I_{\text{peak}} = 2.00\text{ A}$
  - Target Voltage Drop Range: $20\text{ mV} - 50\text{ mV}$ (balances signal-to-noise ratio vs burden voltage)

- **Shunt Resistance Value:**
  $$R_{\text{shunt}} = \frac{V_{\text{drop\_target}}}{I_{\text{peak}}} = \frac{20\text{ mV}}{2.00\text{ A}} = 0.010\,\Omega = \mathbf{10\text{ m}\Omega}$$
  *(For ultra-high resolution at $1.0\text{ A}$, a $20\text{ m}\Omega$ or $50\text{ m}\Omega$ shunt produces $50\text{ mV}$ drop).*

- **Power Dissipation & Thermal Derating:**
  At maximum continuous current ($2.0\text{ A}$):
  $$P_{\text{diss}} = I_{\text{peak}}^2 \times R_{\text{shunt}} = (2.00\text{ A})^2 \times 0.010\,\Omega = 0.040\text{ W} = \mathbf{40\text{ mW}}$$
  At maximum fault envelope ($5.0\text{ A}$ before fuse blowout):
  $$P_{\text{fault}} = (5.00\text{ A})^2 \times 0.010\,\Omega = 0.250\text{ W} = \mathbf{250\text{ mW}}$$
  Applying a conservative $4\times$ engineering safety margin to eliminate thermal coefficient resistance drift ($\Delta R_{\text{thermal}}$):
  $$P_{\text{rated\_min}} \ge 4 \times P_{\text{fault}} = 4 \times 0.250\text{ W} = \mathbf{1.0\text{ W}}$$
  **Selected Shunt Component:** **$10\text{ m}\Omega$, $2\text{ W}$ (or $3\text{ W}$)**, 4-terminal Kelvin surface mount or chassis-mount metal strip shunt, $\pm 0.5\%$ tolerance, temperature coefficient $\le 50\text{ ppm/°C}$.

---

### 2.3 Wire Gauge & Voltage Drop Calculation
High-current lines connect the battery, fuse, contactor, and electronic load.

- **Design Inputs:**
  - Maximum Continuous Current: $I_{\text{max}} = 2.00\text{ A}$ (rig design envelope: $5.00\text{ A}$)
  - Total One-Way Conductor Length: $L = 0.50\text{ m}$ (total loop length $L_{\text{loop}} = 1.00\text{ m}$)
  - Conductor Material: Annealed stranded copper with flexible silicone insulation (200°C rating)

- **Wire Selection:**
  **16 AWG** ($1.31\text{ mm}^2$) flexible silicone wire.
  *(Chassis wiring ampacity for 16 AWG at 200°C rating is $15.0\text{ A}$, providing $> 300\%$ margin over our $5\text{ A}$ maximum operating envelope).*

- **Loop Resistance & Voltage Drop:**
  Resistance of stranded copper 16 AWG at 20°C: $R_{\text{wire}} \approx 13.2\text{ m}\Omega/\text{m}$.
  $$R_{\text{loop}} = 1.00\text{ m} \times 0.0132\,\Omega/\text{m} = 0.0132\,\Omega$$
  At $2.00\text{ A}$ test current:
  $$\Delta V = I \times R_{\text{loop}} = 2.00\text{ A} \times 0.0132\,\Omega = \mathbf{0.0264\text{ V}} = \mathbf{26.4\text{ mV}}$$
  Percentage voltage drop relative to 3.7V nominal:
  $$\% \Delta V = \frac{0.0264\text{ V}}{3.70\text{ V}} \times 100 = \mathbf{0.71\%} \quad (\ll 1.0\%\text{ target})$$

---

### 2.4 Discharge Load & Power Dissipation Sizing
The discharge load dissipates the chemical energy stored in the cell during characterization.

- **Design Inputs:**
  - Cell Maximum Voltage: $V_{\text{max}} = 4.20\text{ V}$
  - Discharge Test Current: $I_{\text{dis}} = 1.00\text{ A}$ (or $2.00\text{ A}$)

- **Power Dissipation:**
  $$P_{\text{load\_nom}} = V_{\text{max}} \times I_{\text{dis}} = 4.20\text{ V} \times 1.00\text{ A} = \mathbf{4.20\text{ W}}$$
  $$P_{\text{load\_max}} = 4.20\text{ V} \times 2.00\text{ A} = \mathbf{8.40\text{ W}}$$

- **Load Selection Options:**
  1. **Option A (Active Programmable Electronic Load - Preferred):**
     - Programmable CC/CV/CP/CR DC Electronic Load rated $\ge 150\text{ W}$, $0-60\text{ V}$, $0-30\text{ A}$ (e.g., East Tester ET5410, Korad KEL103, or DL24P). Provides constant current independent of declining cell voltage.
  2. **Option B (Passive Power Resistor Bank):**
     - Sized for $1.0\text{ A}$ discharge:
       $$R_{\text{resistor}} = \frac{V_{\text{nom}}}{I_{\text{dis}}} = \frac{3.70\text{ V}}{1.00\text{ A}} = 3.70\,\Omega \quad (\text{Use } 3.9\,\Omega\text{ standard})$$
     - Rating required with $4\times$ derating for continuous bench cooling:
       $$P_{\text{resistor\_rated}} \ge 4 \times 8.4\text{ W} = \mathbf{33.6\text{ W}} \rightarrow \text{Use } \mathbf{50\text{ W Aluminum-Housed Wirewound Resistor}}$$ mounted to an external heatsink.

---

## 3. Grounding Scheme & Power Architecture

> **DRAFT FOR HUMAN ENGINEER REVIEW**

Grounding errors represent the single most common cause of microcontroller destruction and measurement offsets in battery test benches.

```
       [ISOLATED 5V USB ADAPTER]
                  │
                  ▼
       [ADuM3160 USB GALVANIC ISOLATOR]  <── Mandatory when connecting to PC/Laptop
                  │ (1500 Vrms Galvanic Isolation)
                  ▼
       [ESP32 Microcontroller Node]
                  │
             (Logic GND)
                  │
                  ▼
  ══════════════════════════════════════════════════════════════════════════════
  ★ SINGLE-POINT STAR GROUND (Directly bonded at Battery Negative Terminal B-)
  ══════════════════════════════════════════════════════════════════════════════
         ▲                           ▲                            ▲
         │                           │                            │
   [Battery B-]            [Shunt Kelvin In (-)]         [BMS Logic Reference]
```

### 3.1 Strict Grounding Rules
1. **Bonding at Battery Negative ($B-$):** The Star Ground reference point is physically located at the battery negative terminal before the current shunt.
2. **Never Ground to Switched Output ($P-$):** Because the BMS and interlock contactor use low-side switching, the load ground ($P-$) floats to $B+$ when cut off. The ESP32 logic ground must NEVER be tied to $P-$.
3. **ESP32 Powering & USB Galvanic Isolation:**
   - The ESP32 is powered via an **independent 5V USB power adapter**.
   - When connecting a laptop or PC for debugging, serial logging, or programming, an inline **ADuM3160 / ADuM4160 Full-Speed USB Galvanic Isolator** ($1500\text{ V}_{\text{RMS}}$ isolation) MUST be installed between the PC and ESP32. This prevents PC chassis ground loops through the battery test shunt.
4. **Differential Kelvin Sensing:** The voltage across the $10\text{ m}\Omega$ shunt is measured using dedicated twisted-pair Kelvin sense traces routed directly to the differential inputs of the ADC (ADS1115 / INA226), isolated from high-current return loops.

---

## 4. Cutoff Design & Safety Interlock Hierarchy

> **DRAFT FOR HUMAN ENGINEER REVIEW**

The test rig implements a multi-tiered safety cutoff hierarchy. Software performs gentle operational cutoffs, while hardware acts as an autonomous fail-safe:

| Tier | Subsystem | Action | Voltage Cutoff (1S) | Response Time | Authority |
| :---: | :--- | :--- | :---: | :---: | :--- |
| **Tier 1 (Soft)** | **ESP32 Firmware** | Opens Interlock Contactor via GPIO | **$3.00\text{ V}$** | $\sim 50\text{ ms}$ | Operational test end (clean logging finalization) |
| **Tier 2 (Hard)** | **Hardware 1S PCM / BMS** | Low-side MOSFET shutoff | **$2.50\text{ V} \pm 0.10\text{ V}$** | $< 10\text{ ms}$ | Autonomous hardware backstop (protects if ESP32 hangs) |
| **Tier 3 (Catastrophic)**| **Primary DC Fuse** | Physical thermal fuse element melt | N/A ($I > 3.0\text{ A}$) | $< 5\text{ ms}$ | Permanent short-circuit protection |

### 4.1 Strict Firmware Interlock Rule (Rule 1)
```cpp
// Firmware Interlock Invariant (Firmware Rule 1)
// All output pins driving the power contactor are strictly gated through this function:
InterlockState SafetyInterlock::evaluateAndDrive(const TelemetryRecord_t* record, bool test_active) {
    if (!record || !test_active) {
        openContactor();
        return INTERLOCK_SAFE_DISARMED;
    }
    // Check all physical safety boundaries locally:
    if (record->pack_voltage_v.value < 3.00f || record->pack_voltage_v.value > 4.22f) {
        openContactor(); // Soft cutoff triggered
        return (record->pack_voltage_v.value < 3.00f) ? INTERLOCK_TRIP_UNDERVOLTAGE : INTERLOCK_TRIP_OVERVOLTAGE;
    }
    if (record->current_a.value > 1.50f) {
        openContactor(); // Overcurrent trip
        return INTERLOCK_TRIP_OVERCURRENT;
    }
    if (record->temps_c[0].value > 45.0f || record->temps_c[1].value > 50.0f) {
        openContactor(); // Thermal trip
        return INTERLOCK_TRIP_OVERTEMP;
    }
    if (digitalRead(ESTOP_SENSE_PIN) == LOW) {
        openContactor(); // E-Stop pressed
        return INTERLOCK_TRIP_ESTOP;
    }
    if ((record->quality_flags & QUALITY_FLAG_SENSOR_FAULT) != 0) {
        openContactor(); // Sensor invalid
        return INTERLOCK_TRIP_SENSOR_FAULT;
    }

    // Only if ALL local safety conditions are true may contactor be closed:
    closeContactor();
    return INTERLOCK_ARMED_ACTIVE;
}
```
**Network inputs can NEVER directly close the contactor or override this interlock.**

---

## 5. Physical Construction, Enclosure & Protection Features

> **DRAFT FOR HUMAN ENGINEER REVIEW**

1. **Insulated Test Enclosure:**
   - The battery cell is housed inside a dedicated non-conductive, fire-resistant enclosure (e.g., steel explosion-resistant LiPo box with non-conductive ceramic/silicone liner).
2. **Polarity Protection & Connectors:**
   - **Keyed High-Current Connectors:** Amass **XT60 / XT30** polarized connectors for battery leads. Asymmetrical mechanical design prevents reverse insertion.
   - **Reverse Polarity Protection:** Shottky diode or reverse-blocking P-channel MOSFET gate circuit on the charger input bus.
3. **Mechanical Strain Relief:**
   - All high-current silicone cables anchored with nylon cable glands or P-clamps within $50\text{ mm}$ of entry into the rig enclosure to prevent mechanical stress on battery terminals.
4. **Emergency Stop (E-Stop):**
   - Industrial latching mushroom-head E-Stop switch (twist-to-release, rated $\ge 10\text{ A DC}$) wired in direct series with the contactor coil power supply. Depressing the switch physically cuts coil holding current, instantly opening the power contacts.
5. **Temperature Probe Placement:**
   - Probe 1 (NTC / DS18B20): Mechanically clamped to the center of the cell metal casing using thermally conductive Kapton tape.
   - Probe 2: Thermally bonded directly to the $10\text{ m}\Omega$ current shunt resistor body.
