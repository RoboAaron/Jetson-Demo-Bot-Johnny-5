# Hardware Recovery & Wiring Guide — Johnny-5 Self-Balancing Robot

**Date:** 2026-04-08
**Reason:** Teensy 4.1 destroyed during wire/connector rearrangement.
**FSESC status:** Functional (verified via VESC Tool over USB).
**IMU status:** Unknown — test after replacement Teensy arrives.

---

## 1. Root-Cause Analysis

The UART wiring was correct and operated reliably for months:

| Teensy Pin | Function | FSESC Side |
|------------|----------|------------|
| Pin 0 (RX1) | Serial1 RX | Left VESC TX |
| Pin 1 (TX1) | Serial1 TX | Left VESC RX |
| Pin 7 (RX2) | Serial2 RX | Right VESC TX |
| Pin 8 (TX2) | Serial2 TX | Right VESC RX |

VESC 6 UART is 3.3 V logic (STM32F4 MCU). No level shifter is required for Teensy 4.1 (also 3.3 V I/O).

**Most likely failure mode during rewiring:**

1. **Momentary 5 V contact** — The FSESC6.7 has a 5 V @ 1 A BEC output on its JST connector, adjacent to the UART pins. Brushing a signal wire against the 5 V pin during rearrangement would push 5 V into a Teensy 3.3 V GPIO, exceeding the absolute-maximum rating and causing permanent latch-up damage.
2. **Hot-plug transient** — Connecting or disconnecting wires while the 10S LiPo battery was live can cause inductive spikes on the shared ground or signal lines.
3. **Reverse current / back-feed** — If the FSESC was powered (battery ON) while the Teensy was unpowered, the VESC UART TX line could source current into the unpowered Teensy 3.3 V rail through the GPIO clamping diodes, causing latch-up.

---

## 2. Shopping List

### Must-Buy (replacement)

| Item | Approx. Price | Source | Notes |
|------|---------------|--------|-------|
| PJRC Teensy 4.1 | $32–38 | [pjrc.com](https://www.pjrc.com/store/teensy41.html) / Amazon | Identical replacement |
| Teyleten Robot GY-BNO085 IMU | ~$20 | Amazon | Only if existing IMU is dead (test first with new Teensy) |

### Protection Components (prevent recurrence)

| Item | Qty | Approx. Price | Purpose |
|------|-----|---------------|---------|
| TVS diode array, 3.3 V clamp (e.g., PRTR5V0U2X or TPD4E05U06) | 2 | $2–4 | Clamp overvoltage on UART RX lines (pins 0, 7) and I2C lines (pins 18, 19) before they reach Teensy GPIOs |
| Ceramic capacitor 100 nF (0.1 µF), 0805 or through-hole | 4 | $1 (pack) | Decoupling on Teensy 3.3 V rail, VESC COMM side, and IMU VCC |
| Electrolytic capacitor 10 µF, 10 V+ | 2 | $1 (pack) | Bulk decoupling on Teensy 3.3 V and VIN |
| Ferrite bead, 600 Ω @ 100 MHz, ≥ 500 mA (e.g., BLM18PG601SN1) | 6 | $3 (pack of 10) | Inline on Serial1 TX/RX, Serial2 TX/RX, I2C SDA/SCL |
| JST-PH 2.0 mm pre-crimped pigtail cables | 2 sets | $5–10 | Mate to FSESC COMM connectors; replace DuPont jumpers with secure connections |
| Silicone hookup wire, 24–26 AWG (signal) | 1 spool | $6 | Clean signal runs |
| Heat-shrink tubing, assorted | 1 kit | $5 | Insulate solder joints and protect against accidental shorts |

### Recommended Tools

| Item | Notes |
|------|-------|
| Digital multimeter | Verify pin voltages BEFORE connecting to Teensy |
| Anti-static wrist strap | Prevent ESD damage during assembly |

### Already Owned (from inventory)

- Treedix breakout board for Teensy 4.1
- JST PH connector set (BOJOUL, 24 sets assorted)
- Silicone wire 16/14/12/10 AWG (for power, not signal)
- Heat-shrink bullet connectors

---

## 3. Power-On / Power-Off Procedures

### 3.1 Pre-Power Checklist (every time)

- [ ] All signal wires are secure in connectors — no loose DuPont pins
- [ ] No bare conductor tips visible — all joints insulated with heat-shrink
- [ ] FSESC JST connectors fully seated — give each a gentle tug to confirm
- [ ] Teensy seated flat in breakout board — no pins bridging underneath
- [ ] Battery switch is OFF and XT90-S is disconnected
- [ ] Multimeter spot-check: measure between FSESC 5 V BEC pin and each signal wire — should read open (infinite resistance)

### 3.2 Power-ON Sequence

```
Step  Action                                          What to verify
────  ──────────────────────────────────────────────  ────────────────────────────────
 1    Battery switch OFF, XT90-S disconnected          No voltage anywhere
 2    Connect Teensy USB to computer                   Power LED on Teensy lights up
 3    Open serial monitor (2 000 000 baud)             Boot banner prints
 4    Confirm IMU init message                         "IMU initialized at 0x4B" (or 0x4A)
 5    Connect XT90-S anti-spark connector              Brief spark is normal (capacitor charge)
 6    Turn battery switch ON                           FSESC power LED lights up
 7    Verify VESC comm in serial output                VESC success rate > 95%
 8    System is ready                                  Roll/pitch/yaw values streaming
```

**Why this order matters:** The Teensy must be powered (USB) before the FSESC, so its GPIO pins are in a defined state when the VESC UART TX lines go active. Powering the FSESC first would drive 3.3 V UART signals into unpowered Teensy GPIOs, risking latch-up.

### 3.3 Power-OFF Sequence

```
Step  Action                                          What to verify
────  ──────────────────────────────────────────────  ────────────────────────────────
 1    Press 'o' in serial monitor                      Motors disabled, "Motor output: OFF"
 2    Turn battery switch OFF                          FSESC power LED goes dark
 3    Wait 2 seconds                                   Let FSESC bulk capacitors discharge
 4    Disconnect XT90-S                                Physical isolation of battery
 5    Disconnect Teensy USB                            Teensy power LED off
```

### 3.4 NEVER-DO Rules

| Rule | Reason |
|------|--------|
| Never plug/unplug signal wires while battery is ON | Transient spikes from inductive coupling can exceed GPIO voltage ratings |
| Never power FSESC before Teensy is on USB | VESC UART TX back-feeds into unpowered Teensy GPIOs → latch-up |
| Never use loose DuPont jumper wires for FSESC connections | They pull out during handling, brush adjacent pins (especially 5 V BEC) |
| Never touch the FSESC 5 V BEC pin to anything on the Teensy | Teensy I/O is 3.3 V only; 5 V will destroy it |
| Never hot-swap the IMU I2C connector with power on | I2C bus contention can damage both the IMU and the Teensy |

---

## 4. Complete Wiring Schematic

### 4.1 System Block Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          POWER DISTRIBUTION                                 │
│                                                                             │
│  ┌──────────┐    ┌─────────┐    ┌─────────┐    ┌────────────────────────┐  │
│  │ 2x 5S    │    │ Nilight │    │ XT90-S  │    │ Flipsky Dual Mini      │  │
│  │ LiPo     ├───►│ Battery ├───►│ Anti-   ├───►│ FSESC6.7 Pro           │  │
│  │ (10S     │    │ Switch  │    │ Spark   │    │                        │  │
│  │ ~37V nom)│    │         │    │         │    │ Left VESC  Right VESC  │  │
│  └──────────┘    └─────────┘    └─────────┘    └───┬───────────┬────────┘  │
│                                                     │           │           │
│                                                     │Motor      │Motor      │
│                                                     │Phases     │Phases     │
│                                                     ▼           ▼           │
│                                               ┌──────────┐┌──────────┐     │
│                                               │ Left Hub ││ Right Hub│     │
│                                               │ Motor    ││ Motor    │     │
│                                               │ (36V)    ││ (36V)    │     │
│                                               └──────────┘└──────────┘     │
│                                                                             │
│  ┌──────────┐    ┌─────────────────────┐    ┌──────────────┐               │
│  │ Computer │    │ Teensy 4.1          │    │ BNO085 IMU   │               │
│  │ / Jetson ├───►│ (USB = 5V power     ├───►│ (3.3V from   │               │
│  │          │USB │  + serial data)     │3.3V│  Teensy)     │               │
│  └──────────┘    └─────────────────────┘    └──────────────┘               │
│                                                                             │
│  Ground bus: Teensy GND ──── FSESC GND ──── BNO085 GND                     │
│              (single-point star ground, 16 AWG or thicker wire)             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 4.2 Teensy 4.1 Pin Assignment Table

| Teensy Pin | Function | Direction | Connects To | Wire Color (suggested) | Notes |
|------------|----------|-----------|-------------|------------------------|-------|
| Pin 0 | Serial1 RX | IN | Left VESC TX | White | Through ferrite bead + TVS clamp |
| Pin 1 | Serial1 TX | OUT | Left VESC RX | Green | Through ferrite bead |
| Pin 7 | Serial2 RX | IN | Right VESC TX | Yellow | Through ferrite bead + TVS clamp |
| Pin 8 | Serial2 TX | OUT | Right VESC RX | Blue | Through ferrite bead |
| Pin 13 | LED_BUILTIN | OUT | (onboard LED) | — | Heartbeat indicator, no external wiring |
| Pin 18 | I2C SDA | BIDIR | BNO085 SDA | Orange | Through ferrite bead; 3.3 V only |
| Pin 19 | I2C SCL | BIDIR | BNO085 SCL | Brown | Through ferrite bead; 3.3 V only |
| 3.3V | Power out | OUT | BNO085 VCC | Red | 250 mA max from Teensy regulator |
| GND | Ground | — | BNO085 GND + FSESC GND | Black | Star ground bus |
| USB | Power + data | BIDIR | Computer / Jetson | — | 2 Mbaud serial + 5 V power source |

Pins not listed are unused by the current firmware.

### 4.3 FSESC6.7 Dual Mini — COMM Connector Identification

The FSESC6.7 Dual Mini has JST-PH 2.0 mm connectors for each motor channel. Pin order varies by board revision and is **not consistently documented** by Flipsky. You **must** identify pins with a multimeter before connecting.

**Procedure to identify each COMM connector's pins:**

```
Equipment needed: Multimeter, FSESC powered via battery (Teensy NOT connected)

1. Set multimeter to DC voltage mode.

2. Connect multimeter black probe to a known FSESC GND
   (e.g., battery negative terminal or motor phase connector shield).

3. Probe each JST pin on the COMM connector with the red probe:

   Expected readings:
   ┌───────────────────────────────────────────────────────┐
   │  ~0.0 V          →  GND                              │
   │  ~5.0 V (steady) →  5V BEC output  ⚠ DO NOT CONNECT │
   │  ~3.3 V (steady or toggling) → UART TX (from VESC)   │
   │  ~0.0 V or floating           → UART RX (into VESC)  │
   │  ~0.0 V or floating           → ADC / PPM input      │
   └───────────────────────────────────────────────────────┘

4. Label each pin with tape or marker.
5. Repeat for the second motor channel's COMM connector.
```

**Critical: The 5 V BEC pin must NEVER connect to any Teensy pin.** If your JST pigtail has a wire landing on the 5 V pin, cut and insulate it with heat-shrink.

### 4.4 Detailed Signal Wiring Diagram

```
                        FSESC6.7 Dual Mini
                   ┌──────────────────────────┐
                   │  LEFT VESC    RIGHT VESC  │
                   │  COMM Port    COMM Port   │
                   │  ┌──────┐    ┌──────┐    │
                   │  │ TX ──┼────┼─TX ──┼─┐  │
                   │  │ RX ──┼─┐  │ RX ──┼┐│  │
                   │  │ GND──┼┐│  │ GND──┼││  │
                   │  │ 5V X │││  │ 5V X │││  │  X = DO NOT CONNECT
                   │  └──────┘││  └──────┘││  │
                   └──────────┼┼──────────┼┼──┘
                              ││          ││
        ┌─ Ferrite ─ TVS ────┘│          ││
        │  ┌─ Ferrite ────────┘          ││
        │  │           ┌─ Ferrite ─ TVS ─┘│
        │  │           │  ┌─ Ferrite ──────┘
        │  │           │  │
        ▼  ▼           ▼  ▼
   ┌────────────────────────────────────────┐
   │            TEENSY 4.1                  │
   │  (on Treedix breakout board)           │
   │                                        │
   │  Pin 0 (RX1) ◄── Left VESC TX         │
   │  Pin 1 (TX1) ──► Left VESC RX         │
   │  Pin 7 (RX2) ◄── Right VESC TX        │
   │  Pin 8 (TX2) ──► Right VESC RX        │
   │                                        │
   │  Pin 18 (SDA) ◄──► BNO085 SDA ─ FB ─┐│
   │  Pin 19 (SCL) ◄──► BNO085 SCL ─ FB ─┤│
   │  3.3V ────────────► BNO085 VCC       ││
   │  GND ─┬───────────► BNO085 GND       ││
   │       │                               ││
   │  USB ◄──► Computer / Jetson           ││
   └───────┼───────────────────────────────┘│
           │                                │
           │  ┌────────────────────────┐    │
           │  │      BNO085 IMU        │    │
           │  │  (GY-BNO085 breakout)  │    │
           │  │                        │    │
           │  │  VCC ◄── 3.3V         │    │
           │  │  GND ◄── GND          │    │
           │  │  SDA ◄──►─────────────┼────┘  (through ferrite bead)
           │  │  SCL ◄──►─────────────┘        (through ferrite bead)
           │  │  PS0  = floating (or GND) for I2C mode
           │  │  PS1  = floating (or GND) for I2C mode
           │  └────────────────────────┘
           │
           ▼
   ════════════════════
    SHARED GROUND BUS
    (16 AWG, star topology)
   ════════════════════
           │
     ┌─────┴─────┐
     ▼           ▼
   Teensy     FSESC
   GND        GND
```

### 4.5 Protection Circuit Detail

Place these components inline between the FSESC connectors and the Teensy breakout board. Solder them onto a small piece of perfboard or protoboard mounted near the Teensy.

```
FSESC UART TX ──── [Ferrite Bead] ──┬── Teensy RX Pin
                                     │
                                [TVS Diode]
                                     │
                                    GND

FSESC UART RX ◄─── [Ferrite Bead] ── Teensy TX Pin
(TX pins need ferrite bead only; TVS clamp is on RX pins where external
 signals enter the Teensy)
```

**TVS diode placement (PRTR5V0U2X):**

The PRTR5V0U2X is a dual-channel TVS in SOT-23-6. One package covers two signal lines. You need two packages:
- Package 1: Teensy Pin 0 (RX1) and Pin 7 (RX2) — VESC UART receive lines
- Package 2: Teensy Pin 18 (SDA) and Pin 19 (SCL) — I2C bus

The TVS clamps any spike above ~3.6 V to ground, protecting the Teensy GPIO.

**Ferrite bead placement:**

One ferrite bead inline on each of these six signal wires:
1. Pin 0 (RX1) — Left VESC TX
2. Pin 1 (TX1) — Left VESC RX
3. Pin 7 (RX2) — Right VESC TX
4. Pin 8 (TX2) — Right VESC RX
5. Pin 18 (SDA) — BNO085 SDA
6. Pin 19 (SCL) — BNO085 SCL

**Decoupling capacitor placement:**
- 100 nF ceramic across Teensy 3.3 V and GND (as close to the pin as possible)
- 10 µF electrolytic across Teensy VIN and GND
- 100 nF ceramic across BNO085 VCC and GND (at the IMU breakout board)

### 4.6 FSESC6.7 VESC Tool Configuration Reference

Both motor channels must be configured identically via VESC Tool over USB:

| Parameter | Value | Notes |
|-----------|-------|-------|
| Motor Type | FOC | Field Oriented Control |
| Motor Poles | 30 | Gyroor 6.5" hoverboard hub motors |
| Max Battery Current | ±50 A | FSESC6.7 Pro rated limit |
| Max Motor Current | ±50 A | |
| Max Battery Voltage | 42 V | 10S LiPo full charge |
| Min Battery Voltage | 30 V | 10S LiPo cutoff |
| App Mode | UART | Not ADC, not PPM |
| UART Baud Rate | 115200 | Must match firmware `Serial1.begin(115200)` |
| CAN ID | 1 (left), 2 (right) | For future CAN use; not active in current firmware |
| CAN Baud Rate | 500000 | |

---

## 5. IMU Testing Procedure (after Teensy replacement)

Before wiring the VESCs, test the IMU alone:

1. Wire only the BNO085 to the new Teensy (SDA, SCL, 3.3V, GND).
2. Connect Teensy via USB, open serial monitor at 2 000 000 baud.
3. Expected output: `IMU initialized at 0x4B` (or `0x4A`).
4. If the IMU fails to initialize at both addresses:
   - Verify PS0 and PS1 are floating or tied to GND (I2C mode).
   - Try powering the IMU from an external 3.3 V supply.
   - If still dead, the IMU was damaged alongside the Teensy — order replacement.
5. Once IMU is confirmed working, proceed to wire the VESCs.

---

## 6. Firmware Communication Parameters (reference)

These are the baud rates and protocols configured in `teensy_balance_cascaded/teensy_balance_cascaded.ino`:

| Interface | Protocol | Speed | Library |
|-----------|----------|-------|---------|
| USB Serial | Serial (CDC) | 2 000 000 baud | Built-in |
| Left VESC | UART (Serial1) | 115 200 baud | VescUart |
| Right VESC | UART (Serial2) | 115 200 baud | VescUart |
| BNO085 IMU | I2C (Wire) | 400 kHz | Adafruit_BNO08x |
| IMU report rate | — | 400 Hz (2500 µs interval) | — |
| Angle PID ISR | IntervalTimer | 500 Hz (2000 µs interval) | — |

---

## 7. Assembly Order (recommended)

1. **Inspect the new Teensy** — verify no bent pins, seat it in the Treedix breakout board.
2. **Solder protection components** — build the TVS + ferrite bead protoboard.
3. **Wire IMU only** — SDA, SCL, 3.3 V, GND through ferrite beads. Test (Section 5).
4. **Wire shared ground** — single 16 AWG wire from Teensy GND to FSESC GND.
5. **Identify FSESC COMM pins** — use multimeter procedure (Section 4.3). Label them.
6. **Prepare JST-PH pigtails** — cut the 5 V BEC wire, insulate the cut end.
7. **Wire left VESC UART** — through ferrite beads and TVS. Verify with multimeter that no wire touches 5 V.
8. **Wire right VESC UART** — same procedure.
9. **Final inspection** — run the pre-power checklist (Section 3.1).
10. **Power on** — follow the power-on sequence (Section 3.2).
