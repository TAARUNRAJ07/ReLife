# ReLife Hardware Data Acquisition (DAQ) & Instrumentation Specification

> **NOTICE: DRAFT FOR HUMAN ENGINEER REVIEW**  
> Every schematic, pin assignment, shunt calculation, logic interface, and sensor topology in this document is a draft and requires physical verification with a multimeter and oscilloscope by a qualified engineer before energizing.

---

## 1. DAQ Subsystem Overview & Architectural Safety

The ReLife Data Acquisition (DAQ) system measures terminal electrical parameters, thermal gradients, and cell states without compromising the hardware safety layer.

### 1.1 Non-Negotiable Electrical Rules
1. **Zero Direct Battery Voltage on MCU Pins (Rule 4):** Battery potentials ($2.5\text{ V} - 16.8\text{ V}$) must NEVER connect directly to ESP32 GPIOs. All voltage sensing is performed through dedicated, isolated instrumentation ICs (INA226) or buffered divider networks.
2. **Zero ADC2 Pin Usage (Rule 4):** Because Wi-Fi telemetry operates continuously, all ESP32 ADC2 pins (GPIO 0, 2, 4, 12, 13, 14, 15, 25, 26, 27) are strictly excluded from analog reading to avoid Wi-Fi driver hardware locks.
3. **Safety Isolation (Rule 1):** The DAQ subsystem is read-only. Actuation output lines are restricted to a single optocoupled interlock line (`RELAY_CTRL`).

---

## 2. Sensor Sizing & Mathematical Derivations

> **DRAFT FOR HUMAN ENGINEER REVIEW**

### 2.1 Pack Voltage & Current Sensing: Texas Instruments INA226
The Texas Instruments **INA226** is a 16-bit high-precision current, voltage, and power monitor communicating over I2C.

#### INA226 Datasheet Specifications (TI INA226 Datasheet SBOS540):
- **Full-Scale Differential Shunt Voltage Range:** $V_{\text{shunt\_fs}} = \pm 81.92\text{ mV}$
- **Shunt Voltage Resolution (LSB):** $2.5\,\mu\text{V}$ per bit ($16\text{-bit}$ signed)
- **Bus Voltage Input Range ($V_{\text{BUS}}$):** $0\text{ V}$ to $36.0\text{ V}$
- **Bus Voltage Resolution (LSB):** $1.25\text{ mV}$ per bit
- **Operating Supply Voltage ($V_{\text{S}}$):** $2.7\text{ V}$ to $5.5\text{ V}$ (tied to ESP32 $3.3\text{ V}$ rail)
- **I2C Bus Compatibility:** Standard (100 kHz) and Fast (400 kHz) modes at $3.3\text{ V}$ CMOS logic.

#### Shunt Resistance Calculation:
The shunt resistance $R_{\text{shunt}}$ must be chosen so that the maximum possible operating current $I_{\text{max}}$ produces a voltage drop within the INA226 input envelope ($\pm 81.92\text{ mV}$):

- **Inputs:**
  - Standard Test Discharge Current: $I_{\text{nom}} = 1.00\text{ A}$
  - Peak Operating Current Envelope: $I_{\text{max}} = 5.00\text{ A}$ (covers 1S up to 4S scaled testing)
  - INA226 Maximum Shunt Voltage: $V_{\text{in\_max}} = 81.92\text{ mV}$

- **Formula & Maximum Resistance:**
  $$R_{\text{shunt\_max}} = \frac{V_{\text{in\_max}}}{I_{\text{max}}} = \frac{81.92\text{ mV}}{5.00\text{ A}} = 16.384\text{ m}\Omega$$

- **Selected Standard Value:**
  $$R_{\text{shunt}} = \mathbf{10.0\text{ m}\Omega}\quad (0.010\,\Omega,\ \pm 0.5\%,\ \text{4-terminal Kelvin metal element})$$

- **Verification at Operating Points:**
  - At $I_{\text{nom}} = 1.00\text{ A}$: $V_{\text{shunt}} = 1.00\text{ A} \times 0.010\,\Omega = 10.0\text{ mV}$
  - At $I_{\text{peak}} = 2.00\text{ A}$: $V_{\text{shunt}} = 2.00\text{ A} \times 0.010\,\Omega = 20.0\text{ mV}$
  - At $I_{\text{max}} = 5.00\text{ A}$: $V_{\text{shunt}} = 5.00\text{ A} \times 0.010\,\Omega = 50.0\text{ mV}\quad (61.0\%\text{ of full scale, providing safe headroom})$

- **Measurement Resolution:**
  $$\text{Current LSB} = \frac{V_{\text{shunt\_LSB}}}{R_{\text{shunt}}} = \frac{2.50\,\mu\text{V}}{0.010\,\Omega} = \mathbf{0.250\text{ mA}}\quad (250\,\mu\text{A per bit})$$

- **Power Dissipation & Shunt Wattage Selection:**
  Continuous power dissipation at $I_{\text{peak}} = 2.00\text{ A}$:
  $$P_{\text{diss\_cont}} = I_{\text{peak}}^2 \times R_{\text{shunt}} = (2.00\text{ A})^2 \times 0.010\,\Omega = 0.040\text{ W} = \mathbf{40\text{ mW}}$$
  Maximum fault power dissipation at $I_{\text{max}} = 5.00\text{ A}$:
  $$P_{\text{diss\_max}} = (5.00\text{ A})^2 \times 0.010\,\Omega = 0.250\text{ W} = \mathbf{250\text{ mW}}$$
  Applying an $8\times$ engineering safety margin to keep component temperature rise $< 5\text{ °C}$ (minimizing thermal drift $\Delta R_{\text{TCR}}$):
  $$P_{\text{rated}} \ge 8 \times P_{\text{diss\_max}} = 8 \times 0.250\text{ W} = \mathbf{2.0\text{ W}}$$
  **Selected Shunt Component:** **Vishay Dale WSL2512R0100FEA** ($10\text{ m}\Omega$, $2\text{ W}$, $1\%$, $75\text{ ppm/°C}$, 4-terminal Kelvin surface mount).

---

### 2.2 Multi-Point Temperature Monitoring: Dallas DS18B20
Thermal monitoring uses **Maxim/Dallas DS18B20** digital temperature sensors on an active 1-Wire bus.

- **Bus Topology:** Single 1-Wire bus pulled up to $3.3\text{ V}$ through a precision **$4.7\text{ k}\Omega \pm 1\%$** metal-film pull-up resistor.
- **Powering Method:** Dedicated $3.3\text{ V}$ power connection (3-wire mode: VDD, GND, DATA). *Parasitic power mode is strictly prohibited* to prevent conversion drops during multi-probe simultaneous temperature sampling.
- **Sensor Placement & Role:**
  1. **Sensor 1 (Cell Body):** Mechanically taped to the center of the battery cell casing with thermally conductive Kapton tape.
  2. **Sensor 2 (Current Shunt):** Thermally bonded directly to the $10\text{ m}\Omega$ shunt resistor body to monitor sensing burden heating.
  3. **Sensor 3 (BMS / PCM FETs):** Placed directly on the power MOSFET heatsink tab of the protection module.
  4. **Sensor 4 (Ambient Reference):** Suspended in free air within the enclosure perimeter to calculate $\Delta T = T_{\text{cell}} - T_{\text{ambient}}$.
- **Measurement Envelope:** $-55\text{ °C}$ to $+125\text{ °C}$ ($\pm 0.5\text{ °C}$ accuracy from $-10\text{ °C}$ to $+85\text{ °C}$).

---

### 2.3 Smart BMS UART Digital Telemetry & Voltage Cross-Check
The ESP32 reads calibrated individual cell voltages ($V_1, V_2, V_3, V_4$), State of Charge (SOC), and hardware alarm flags via hardware UART (`UART2`).

#### Logic Level Compatibility & Interfacing:
- **JBD-SP04S034:** Operates natively at **$3.3\text{ V}$ TTL logic**. Connects directly to ESP32 GPIO 16 (RX2) and GPIO 17 (TX2).
- **Daly Smart BMS / 5V Models:** If testing a Daly BMS or any unit that outputs $5.0\text{ V}$ TTL logic, an inline **bidirectional logic level shifter** (e.g., TI TXS0108E or BSS138 FET circuit) is mandatory. Connecting $5\text{ V}$ logic directly to ESP32 GPIOs will cause latch-up and silicon degradation.

#### Voltage Redundancy & Sanity Cross-Check:
To guarantee sensor integrity against wire faults, the firmware executes a continuous mathematical cross-check between the independent analog INA226 bus reading and the sum of BMS cell voltages:

$$\Delta V_{\text{cross\_check}} = \left| V_{\text{BUS\_INA226}} - \sum_{i=1}^{N} V_{\text{cell\_BMS}, i} \right|$$

- **Plausibility Threshold:** $\Delta V_{\text{cross\_check}} \le \mathbf{100\text{ mV}}$ (or $\le 2.0\%$ of pack voltage).
- **Failure Response:** If $\Delta V_{\text{cross\_check}} > 100\text{ mV}$ for $> 2$ consecutive sampling periods:
  1. Firmware immediately flags `QUALITY_FLAG_VOLTAGE_DIV_NOISE`.
  2. Local safety interlock triggers `INTERLOCK_TRIP_SENSOR_FAULT`.
  3. Load contactor is opened instantly, halting test execution.

---

### 2.4 Local Display & Non-Volatile Storage
- **Local Visual Display:** **SSD1306 0.96" OLED** ($128 \times 64$ pixels, monochrome) operating on the shared $3.3\text{ V}$ I2C bus (Address: `0x3C`). Displays real-time cell voltage, discharge current, skin temperature, interlock status, and Wi-Fi link state.
- **Local Journal Storage:** **SPI MicroSD Card Module** formatted in FAT32. Communicates over the dedicated hardware VSPI bus. Stores circular telemetry records locally to guarantee at-least-once delivery during network drops.

---

## 3. Comprehensive ESP32 Wiring & Pin Mapping Table

> **DRAFT FOR HUMAN ENGINEER REVIEW**

All pin assignments respect hardware strapping requirements, avoid ADC2 restrictions, and ensure 3.3V logic compliance:

| ESP32 Pin | Connected Device | Device Signal | ESP32 Function | Voltage Level | Electrical Constraints / Circuit Details |
| :--- | :--- | :--- | :--- | :---: | :--- |
| **GPIO 21** | INA226, SSD1306 | SDA | I2C Data | 3.3 V | Hardware I2C bus; $4.7\text{ k}\Omega$ pull-up to 3.3V |
| **GPIO 22** | INA226, SSD1306 | SCL | I2C Clock | 3.3 V | Hardware I2C bus; $4.7\text{ k}\Omega$ pull-up to 3.3V |
| **GPIO 16** | Smart BMS / PCM | RXD | UART2 RX | 3.3 V | Connects to BMS TX (via level shifter if 5V) |
| **GPIO 17** | Smart BMS / PCM | TXD | UART2 TX | 3.3 V | Connects to BMS RX (3.3V logic) |
| **GPIO 4** | DS18B20 Probes | DQ | 1-Wire Bus | 3.3 V | Dedicated 1-Wire; $4.7\text{ k}\Omega$ pull-up to 3.3V |
| **GPIO 23** | MicroSD Card | MOSI | VSPI MOSI | 3.3 V | Hardware SPI Data Out |
| **GPIO 19** | MicroSD Card | MISO | VSPI MISO | 3.3 V | Hardware SPI Data In |
| **GPIO 18** | MicroSD Card | SCK | VSPI SCK | 3.3 V | Hardware SPI Clock |
| **GPIO 5** | MicroSD Card | CS | VSPI CS | 3.3 V | Chip Select (Active Low); internal pull-up |
| **GPIO 27** | Relay Contactor Driver| IN | GPIO Output | 3.3 V | Controls optocoupled relay input (Active HIGH) |
| **GPIO 34** | E-Stop Switch | SENSE | GPIO Input | 3.3 V | Input-only (ADC1 bank). HIGH=Armed, LOW=E-Stop pressed |
| **GPIO 35** | INA226 Sensor | ALERT | GPIO Input | 3.3 V | Input-only (ADC1 bank). Active LOW overcurrent alert |
| **3.3V (Pin 1)**| Sensors / OLED / SD | VCC | Power Rail | 3.3 V DC | ESP32 LDO output; total DAQ load $< 120\text{ mA}$ |
| **GND (Pin 14)**| Star Ground Point | GND | Logic Ground | 0.0 V Ref | Bonded directly to Battery Negative ($B-$) |
| **5V / VIN** | USB Isolator | VBUS | Power Input | 5.0 V DC | Sourced from ADuM3160 USB Galvanic Isolator |

### Strictly Excluded ESP32 Pins:
- **GPIO 6, 7, 8, 9, 10, 11:** Connected internally to the SPI flash memory. Excluded.
- **GPIO 0, 2, 12, 15:** Strapping pins. Excluded from external instrumentation to prevent boot failures.
- **GPIO 25, 26, 12, 13, 14, 15, 2, 4, 0:** Excluded from any analog readings due to ADC2 / Wi-Fi peripheral lock.

---

## 4. Bill of Materials (BOM) Additions for DAQ

The following specialized components are added to [`bom.csv`](file:///d:/RMKEC/Year_III/Sem_V/Hackathon/Revolt/RelifeHW/bom.csv):

| RefDes | Component Description | Manufacturer / MPN | Key Specifications | Quantity |
| :--- | :--- | :--- | :--- | :---: |
| **U1** | Digital Voltage/Current Sensor IC | TI INA226AIDGSR | 16-bit I2C, $\pm 81.92\text{ mV}$ shunt range, $36\text{ V}$ bus, MSOP-10 | 1 |
| **R_SHUNT**| Precision Current Shunt Resistor | Vishay WSL2512R0100FEA | $10\text{ m}\Omega$, $2\text{ W}$, $1\%$, 4-terminal Kelvin SMD | 1 |
| **DISP1**| OLED Graphical Display Module | Solomon Systech SSD1306 | 0.96", $128 \times 64$ pixels, I2C, 3.3V | 1 |
| **SD1** | MicroSD Card Storage Module | Generic / Adafruit 254 | SPI interface, 3.3V level buffer, push-push socket | 1 |
| **TH1-TH4**| Digital 1-Wire Temperature Sensors | Dallas DS18B20 | Waterproof probe, 1-Wire, $\pm 0.5\text{ °C}$, 3.3V | 4 |
| **U3** | Bidirectional Logic Level Shifter | TI TXS0108EPWR | 8-channel auto-direction sensing, 3.3V $\leftrightarrow$ 5.0V | 1 |
| **R_PU1-3**| Bus Pull-Up Resistors | Yageo RC0805FR-074K7L | $4.7\text{ k}\Omega$, $1/8\text{ W}$, $1\%$, SMD 0805 (for I2C and 1-Wire) | 3 |
| **C_DEC1-4**| Decoupling Ceramic Capacitors | Murata GRM188R71H104KA93D | $0.1\,\mu\text{F}$ ($100\text{ nF}$), 50V, X7R (at IC VDD pins) | 4 |

---

## 5. Failure Mode, Risk Analysis & Engineering Warnings

> [!CAUTION]
> **CRITICAL ELECTRICAL RISKS:**
> 1. **High Common-Mode Voltage Risk on INA226 Bus ($V_{\text{BUS}}$):**  
>    The INA226 $V_{\text{BUS}}$ line is rated for a maximum of $36.0\text{ V}$. For 1S ($4.2\text{ V}$) and 4S ($16.8\text{ V}$) packs, this is completely safe. However, never connect this DAQ circuit to battery packs exceeding $36\text{ V}$ (e.g. 10S/13S 48V e-bike packs) without an external voltage divider, or the INA226 will fail catastrophically.
> 2. **Loss of 1-Wire Pull-Up Resistor:**  
>    If the $4.7\text{ k}\Omega$ pull-up resistor on the 1-Wire bus opens or disconnects, the bus will float high or low, causing all DS18B20 readings to fail.  
>    **Firmware Fail-Safe:** The firmware checks sensor validity. If reading fails, it flags `QUALITY_FLAG_SENSOR_FAULT` and immediately trips the interlock contactor.
> 3. **Ground Loop Through Debug Cable:**  
>    Connecting an un-isolated USB cable from a mains-powered PC to the ESP32 while the battery is under test can bridge PC earth ground to battery ground, bypassing the current shunt.  
>    **Mitigation:** The **ADuM3160 USB Galvanic Isolator** is mandatory whenever a PC is physically connected.
