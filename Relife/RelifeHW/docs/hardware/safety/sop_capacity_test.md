# Standard Operating Procedure (SOP): 4S Battery Pack Capacity & Characterization Test

> **NOTICE: DRAFT FOR HUMAN ENGINEER REVIEW**  
> This SOP is a draft engineering document. All test currents, cutoff voltages, rest durations, and safety limits MUST be reviewed, configured from cell manufacturer datasheets, and approved by a qualified engineer prior to execution.

---

## 1. Objective & Scope

This procedure details the step-by-step execution for measuring the actual usable discharge capacity ($\text{Ah}$) and characterization curve of a retired 4S low-voltage battery module using the ReLife hardware test rig.

### 1.1 Test Profile Overview
```
[Pre-Checks] -> [Rest 1: 15 min] -> [CC-CV Charge] -> [Rest 2: 30 min] -> [CC Discharge] -> [Rest 3 / Cooldown: 15 min] -> [Data Export]
```

---

## 2. Test Parameters (Configured for 1S 3.7V 2000mAh Cell)

| Parameter | Formula / Convention | Configured Test Value | Notes / Status |
| :--- | :--- | :--- | :--- |
| **Nominal Cell Capacity ($C_{\text{nom}}$)** | Cell rating label | **$2.00\text{ Ah}$** ($2000\text{ mAh}$) | Baseline rated capacity |
| **Gentle Charge Current ($0.2C$)** | $I_{\text{chg\_gentle}} = 0.2 \times C_{\text{nom}}$ | **$0.40\text{ A}$** | Recommended for retired cells |
| **Standard Charge Current ($0.5C$)** | $I_{\text{chg\_std}} = 0.5 \times C_{\text{nom}}$ | **$1.00\text{ A}$** | Standard CC charge rate |
| **Charge Cutoff Voltage (CV Target)** | Upper terminal voltage | **$4.20\text{ V}$** | End of CC-CV charging |
| **Charge Cutoff Current** | $I_{\text{cutoff}} \approx 0.025C$ | **$0.05\text{ A}$** ($50\text{ mA}$) | End of charge taper threshold |
| **Discharge Test Current ($I_{\text{dis}}$)** | $0.5C$ constant current (Positive convention) | **$1.00\text{ A}$** | Characterization discharge rate |
| **Cell Undervoltage Cutoff ($V_{\text{min}}$)** | Safe lower discharge limit | **$3.00\text{ V}$** | Rig interlock contactor cutoff |
| **Max Skin Temperature ($T_{\text{max}}$)** | Upper thermal trip threshold | **$50.0\text{ °C}$** | Charge abort: 45°C; Discharge: 50°C |
| **Max Test Duration ($t_{\text{max}}$)** | $t_{\text{max}} = 1.3 \times \frac{C_{\text{nom}}}{I_{\text{dis}}} \times 3600$ | **$9360\text{ s}$** ($156\text{ min}$) | Safety watchdog timeout |
| **Primary Fast DC Fuse** | Fast-blow fuse at $B+$ terminal | **$3.0\text{ A}$** | Short-circuit hardware barrier |

---

## 3. Step-by-Step Operating Procedure

### Phase 1: Pre-Checks & Initial Rig Setup
1. **Physical & Visual Inspection:**
   - Inspect the 4S pack for structural integrity, swelling, cell venting marks, or terminal corrosion.
   - Verify that all cell tap balance leads and high-current silicone leads (10–12 AWG) are securely seated in the terminal blocks.
2. **Primary Fuse & Hardware BMS Check:**
   - Verify that the fast-acting primary fuse (`<<FROM_DATASHEET: primary_fuse_rating_a>>` A) is installed in the positive battery lead.
   - Ensure the hardware BMS is active and balance leads are fully engaged.
3. **Rig Connection & Grounding:**
   - Connect the pack positive ($B+$) and pack negative ($B-$) to the rig input terminals.
   - Verify single-point star ground between the current shunt, electronic load, and instrumentation.
4. **Instrumentation & Telemetry Initialization:**
   - Power on the ESP32 test rig via the isolated USB supply.
   - Launch local telemetry monitor / logging:
     ```bash
     python tools/hw/mockctl.py start-log --device-id ESP32-RIG-001 --interval-ms 1000
     ```
   - Verify that all 4 cell voltages, pack voltage, current, and 4 temperatures report `VALID` quality flags.
5. **E-Stop Verification:**
   - Manually depress the physical E-Stop switch; verify that the load contactor disconnects immediately and ESP32 reports `TRIPPED_ESTOP`. Reset E-Stop to armed state.

---

### Phase 2: Initial Rest Period (Equilibrium)
1. Keep the battery at open circuit ($I = 0\text{ A}$) for **15 minutes**.
2. Measure and record initial Open Circuit Voltages (OCV) for all 4 cells:
   - Cell 1: _____ V, Cell 2: _____ V, Cell 3: _____ V, Cell 4: _____ V.
3. Verify that the initial cell imbalance $\Delta V = V_{\text{max}} - V_{\text{min}} < <<FROM_DATASHEET: max_initial_cell_delta_v>>\text{ V}$.

---

### Phase 3: Controlled CC-CV Charging
> [!IMPORTANT]
> **Supervision Required:** Do NOT leave the charging rig unattended.
1. Configure the DC power supply / charger:
   - Voltage limit: `<<FROM_DATASHEET: max_pack_charge_voltage_v>>` V.
   - Constant current limit: `<<FROM_DATASHEET: charge_current_a>>` A.
2. Initiate charging. Monitor cell voltages and temperatures continuously.
3. **Charge Termination Criteria:**
   - Stop when current tapers down to `<<FROM_DATASHEET: charge_cutoff_current_a>>` A, **OR**
   - Any cell reaches `<<FROM_DATASHEET: cell_overvoltage_cutoff_v>>` V, **OR**
   - Pack temperature exceeds `<<FROM_DATASHEET: max_charge_temp_c>>` °C.
4. Disconnect charger immediately upon charge completion.

---

### Phase 4: Post-Charge Rest Period (Stabilization)
1. Allow the pack to rest in an open-circuit state for **30 minutes**.
2. This rest period allows chemical equilibrium and thermal stabilization.
3. Record stabilized full-charge OCV values for all 4 cells.

---

### Phase 5: Constant-Current Capacity Discharge
1. Configure the DC Electronic Load:
   - Mode: Constant Current (CC).
   - Discharge Current: `<<FROM_DATASHEET: discharge_test_current_a>>` A.
   - Voltage Cutoff (Hardware Load): `<<FROM_DATASHEET: pack_undervoltage_cutoff_v>>` V.
2. Enqueue the capacity test on the rig:
   ```bash
   python tools/hw/mockctl.py start-test --test-type capacity --battery-id RL-BAT-0042 --cutoff-v 10.0
   ```
3. Engage the electronic load to begin discharge.
4. Continuous Monitoring:
   - Verify discharge current reads positive ($I > 0\text{ A}$).
   - Continuously monitor the lowest cell voltage ($V_{\text{min}}$) and highest temperature ($T_{\text{max}}$).

---

### Phase 6: Automatic & Manual Stop Conditions
The discharge test MUST immediately terminate when **ANY** of the following conditions is met:

1. **Lowest Cell Cutoff:** Any individual cell drops to $\le <<FROM_DATASHEET: cell_undervoltage_cutoff_v>>\text{ V}$ (triggers instant contactor open).
2. **Pack Voltage Cutoff:** Total pack voltage drops to $\le <<FROM_DATASHEET: pack_undervoltage_cutoff_v>>\text{ V}$.
3. **Over-Temperature Trip:** Any temperature sensor reads $\ge <<FROM_DATASHEET: max_discharge_temp_c>>\text{ °C}$ or rate of temperature rise $\frac{dT}{dt} > 2.0\text{ °C/min}$.
4. **Time Limit Exceeded:** Test duration exceeds maximum safety timeout $t_{\text{max}} = <<FROM_DATASHEET: max_test_duration_s>>\text{ s}$.
5. **Operator Emergency Abort:** Operator depresses physical E-Stop or issues:
   ```bash
   python tools/hw/mockctl.py stop-test --reason operator_abort
   ```

---

### Phase 7: Post-Test Cooldown & Stabilization
1. Ensure the discharge contactor is open ($I = 0.00\text{ A}$).
2. Allow the pack to cool and rest undisturbed for **15 minutes**.
3. Measure and record final resting OCV:
   - Cell 1: _____ V, Cell 2: _____ V, Cell 3: _____ V, Cell 4: _____ V.
4. If the pack is to be placed into storage, recharge to storage voltage (`<<FROM_DATASHEET: nominal_storage_voltage_v>>` V/cell, ~30–50% SOC).

---

## 4. Data Capture & Verification Checklist

Immediately after test completion, verify the integrity of the collected telemetry data:

```bash
# Validate recorded JSONL log against schema
python tools/hw/validate_log.py tools/hw/mock_server/data/telemetry_store.jsonl
```

Verify the following items before archiving the dataset:
- [ ] Total discharged capacity calculated: $C_{\text{actual}} = \int I(t)\,dt = \text{______ Ah}$.
- [ ] Health percentage calculated: $\text{SOH}_{\text{actual}} = \frac{C_{\text{actual}}}{C_{\text{nom}}} \times 100 = \text{______}\%$.
- [ ] Log validation pass rate is 100.0%.
- [ ] No sequence gaps (`seq` monotonically strictly increasing).
- [ ] Zero unexpected non-monotonic timestamp steps or clock jumps.
- [ ] Sampling jitter standard deviation $\sigma_{\Delta t} \le 5.0\text{ ms}$.
- [ ] Calibration version (`calibration_version`) recorded on every telemetry frame.

---

## 5. Human Pre-Power-Up & Execution Sign-Off

> [!CAUTION]
> Both the Primary Operator and the Peer Safety Reviewer must sign below before executing this SOP.

| Step | Verification | Operator Initials | Reviewer Initials |
| :--- | :--- | :---: | :---: |
| 1 | Visual inspection passed (no swelling, physical damage, or leaks). | _____ | _____ |
| 2 | Primary fuse installed and rated correctly. | _____ | _____ |
| 3 | Physical E-Stop test verified (breaks contactor circuit). | _____ | _____ |
| 4 | Voltage divider and shunt connections verified isolated from ESP32 pins. | _____ | _____ |
| 5 | Fire extinguisher, sand bucket, and fire blanket ready on bench. | _____ | _____ |
| 6 | Both engineers equipped with eye protection and FR lab coats. | _____ | _____ |

**Primary Test Operator:**  
Name: ________________________________  Signature: ________________________________  Date: ______________

**Peer Safety Reviewer:**  
Name: ________________________________  Signature: ________________________________  Date: ______________
