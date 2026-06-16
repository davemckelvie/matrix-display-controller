# uHat PCB — Schematic Checklist

**MCU:** STM32G431CBU6 (UFQFPN-48, 7×7 mm, 0.5 mm pitch)  
**Board revision:** board-v4  
**Firmware branch:** `board-v4` in `github.com/davemckelvie/matrix-display-controller.git`

---

## 1. Mandatory MCU connections

| Signal | MCU pin | Notes |
|---|---|---|
| VDD (digital) | 9, 24, 36, 48 | 3.3 V. 100 nF per pin + 4.7 µF bulk |
| VSS (digital GND) | 10, 23, 35, 47 | |
| VDDA (analog) | 13 | 3.3 V via ferrite bead, 100 nF + 1 µF |
| VSSA (analog GND) | 14 | |
| VDD_USB | 37 | Tie to 3.3 V via ferrite bead (even if USB unused) |
| NRST | 7 | 10 kΩ pull-up to 3.3 V + 100 nF to GND |
| BOOT0 | 28 | 10 kΩ pull-down to GND (boot from Flash) |
| VCAP | 22 | 2.2 µF to GND (per DS) |

---

## 2. Debug interface (SWD)

| Signal | MCU pin | Notes |
|---|---|---|
| SWDIO | PA13 (pin 34) | 10 kΩ pull-up recommended |
| SWCLK | PA14 (pin 33) | 10 kΩ pull-down recommended |
| SWO (optional) | PB3 (pin 39) | Only if using SWO trace |

> **Do not** route PA13 and PA14 to anything else — SWD is needed for firmware flashing and debugging.

---

## 3. Clock source

**Option A — HSE external crystal (recommended for accuracy)**

| Signal | MCU pin | Notes |
|---|---|---|
| OSC_IN | PD0 / PF0 (pin 5) | 8 MHz crystal + 2× 15 pF caps |
| OSC_OUT | PD1 / PF1 (pin 6) | |

> **Note:** On UFQFPN-48, the HSE pins alternate between PD0/PD1 and PF0/PF1 depending on the package variant. Check the STM32G431CBU6 datasheet pinout — they are **PD0 (pin 5) and PD1 (pin 6)**. Wire an 8 MHz crystal.

**Option B — HSI internal oscillator**

If the uHat doesn't need accurate clocking, the default 16 MHz HSI is sufficient. No HSE crystal needed; leave pins 5–6 unconnected (or route as GPIO if available on your package variant).

---

## 4. HUB75 display connector (13 signals)

All connections are **3.3 V logic** (the HUB75 panel is 5 V tolerant on inputs for most panels; confirm your panel spec).

### Row-select address lines

| Signal | MCU pin | Firmware define |
|---|---|---|
| A (row LSB) | **PC0** (pin 8) | `PIN_A` |
| B | **PC7** (pin 16) | `PIN_B` |
| C | **PC6** (pin 15) | `PIN_C` |
| D (row MSB) | **PB15** (pin 31) | `PIN_D` |

### Latch / OE / Clock

| Signal | MCU pin | Firmware define |
|---|---|---|
| OE (output enable) | **PB12** (pin 30) | `PIN_OE` |
| STB / LAT (strobe/latch) | **PB13** (pin 29) | `PIN_STB` |
| CLK (shift clock) | **PB14** (pin 32) | `PIN_CLK` |

### Color data — top half (rows 0–15)

| Signal | MCU pin | Firmware define |
|---|---|---|
| R1 (red) | **PB3** (pin 39) | `PIN_R1` |
| G1 (green) | **PD2** (pin 28) | `PIN_G1` |
| B1 (blue) | **PC3** (pin 12) | `PIN_B1` |

### Color data — bottom half (rows 16–31)

| Signal | MCU pin | Firmware define |
|---|---|---|
| R2 (red) | **PC1** (pin 9) | `PIN_R2` |
| G2 (green) | **PC2** (pin 10) | `PIN_G2` |
| B2 (blue) | **PA15** (pin 19) | `PIN_B2` |

### CRITICAL — Pin swap bug (D vs CLK)

The **original PCB** incorrectly swapped:
- D (row select) → PB14
- CLK → PB15

**Firmware (correct):** `PIN_D = PB_15`, `PIN_CLK = PB_14`  
**Schematic must match the firmware:**

| Wire | PCB output | MCU pin |
|---|---|---|
| D (row select) | HUB75 connector | **PB15** (pin 31) |
| CLK (shift clock) | HUB75 connector | **PB14** (pin 32) |

> ⚠️ This is the single most likely wiring error. Triple-check these two connections against the pin table above.

---

## 5. SPI slave interface (from host MCU / Raspberry Pi)

| Signal | MCU pin | Firmware define |
|---|---|---|
| SPI1_MOSI (host→uHat) | **PA7** (pin 18) | `MOD_SPI_SLAVE_MOSI` |
| SPI1_MISO (uHat→host) | **PA6** (pin 17) | `MOD_SPI_SLAVE_MISO` |
| SPI1_SCK (clock) | **PA5** (pin 15) | `MOD_SPI_SLAVE_SCK` |
| SPI1_SS (chip select) | **PA4** (pin 14) | `MOD_SPI_SLAVE_SS` |

> **Level-shift needed:** If the host is 5 V (e.g., original Raspberry Pi), add a 3.3 V → 5 V tolerant level shifter or voltage divider on MOSI, SCK, SS. MISO is uHat output (3.3 V) — acceptable to most 5 V hosts.

---

## 6. UART console (optional)

| Signal | MCU pin | Firmware |
|---|---|---|
| USART1_TX | **PA9** (pin 21) | `BufferedSerial pc(PA_9, PA_10)` |
| USART1_RX | **PA10** (pin 22) | |

Used for debug console output at 9600 baud. Optional for production — can leave unpopulated.

---

## 7. Status LED

| Signal | MCU pin | Notes |
|---|---|---|
| LED | **PC13** (pin 7) | Active-high (`ledPin = 1`). Series resistor (e.g., 330 Ω) to LED anode, LED cathode to GND. |

> **Note:** PC13 is pin 7 on UFQFPN-48 — same package pin as NRST on some other STM32 variants. On G431CBU6, pin 7 **is** PC13, not NRST. NRST is a separate pin. Double-check your datasheet pin 7 assignment.

---

## 8. Pins that are **NOT** bonded on UFQFPN-48

These pins exist in the silicon but have **no physical pad** on the 48-pin package. **Do not route traces to them** — they cannot be used.

| Port pin | Use in board-v3 | Now handled by |
|---|---|---|
| PC_8 | Row-select A | → PC_0 (pin 8) |
| PC_10 | Color R2 | → PC_1 (pin 9) |
| PC_11 | Color G2 | → PC_2 (pin 10) |
| PC_12 | Color B1 | → PC_3 (pin 12) |

---

## 9. Unused / available GPIO

The following pins are **not used** in this firmware revision. They can be left NC, used for future expansion, or repurposed as GPIO on the PCB:

| Port pin | MCU pin | Notes |
|---|---|---|
| PA0 | 11 | ADC/TIM/GPIO |
| PA1 | 12 | ADC/TIM/GPIO |
| PA2 | 13 | ADC/TIM/GPIO |
| PA3 | 14 | ADC/TIM/GPIO |
| PA8 | 20 | MCO/TIM/GPIO |
| PA11 | 24 | USB_DM / GPIO |
| PA12 | 25 | USB_DP / GPIO |
| PB0 | 35 | ADC/TIM/GPIO |
| PB1 | 36 | ADC/TIM/GPIO |
| PB2 | 37 | GPIO |
| PB4 | 40 | GPIO / SPI1_NSS alt |
| PB5 | 41 | GPIO / SPI1_MOSI alt |
| PB6 | 42 | GPIO / I2C1_SCL |
| PB7 | 43 | GPIO / I2C1_SDA |
| PB8 | 44 | GPIO / TIM |
| PB9 | 45 | GPIO / TIM |
| PB10 | 46 | GPIO / UART3_TX |
| PB11 | 47 | GPIO / UART3_RX |
| PC4 | 13 | ADC/GPIO |
| PC5 | 14 | ADC/GPIO |
| PC14 | 6 | OSC32_IN / GPIO (if no LSE) |

---

## 10. Checklist — before sending for fab

- [ ] All **4 VDD + 4 VSS** pins have decoupling caps
- [ ] **VDDA** has ferrite bead + 100 nF + 1 µF
- [ ] **VDD_USB** connected to 3.3 V via ferrite
- [ ] **VCAP** has 2.2 µF to GND
- [ ] **NRST** has 10 kΩ pull-up + 100 nF to GND
- [ ] **BOOT0** pulled to GND (10 kΩ)
- [ ] **SWD** (PA13, PA14) routed to a 5-pin header (VDD, SWDIO, SWCLK, GND, NRST)
- [ ] **Crystal** (if HSE used): 8 MHz, load caps appropriate for your crystal's CL
- [ ] **HUB75 pinout** matches firmware exactly (especially D vs CLK — see §4)
- [ ] **SPI level-shift**: if host is 5 V, level shifters on MOSI/SCK/SS
- [ ] No traces to **PC8/PC10/PC11/PC12** (not bonded)
- [ ] All **GND** pins tied together with a solid ground plane
