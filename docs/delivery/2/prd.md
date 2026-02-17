# PBI-2: BNO085 SPI Migration

## Overview
Migrate BNO085 IMU from I2C to SPI interface to achieve 99% reliability with high-rate sensor updates (400-1000Hz) and low latency (<0.5ms) required for rock-solid balancing.

## Problem Statement
**ORIGINAL**: Current I2C interface at 100kHz limits IMU update rate and introduces latency/jitter that prevents achieving 99% reliability. SPI interface is required for commercial hoverboard/Segway-level performance.

**REVISED (2026-01-26)**: After extensive testing, SPI migration was **ABANDONED** due to hardware limitations. The Teyleten Robot GY-BNO085 breakout board does not properly support SPI communication - the ADO pin (MISO) is not routed to the BNO085 chip's MISO pin, instead being strapped/resistored for I2C address selection. Additionally, I2C optimization (400kHz bus, 400Hz IMU updates) achieved excellent performance (97.7-98.8% IMU communication success, ±0.5° roll stability), proving that optimized I2C is sufficient for balancing without requiring SPI.

## User Stories
- As a developer, I want SPI communication with BNO085 IMU, so that I can achieve 400-1000Hz update rates with <0.5ms latency.
- As a developer, I want to resolve PS3 controller conflict with SPI IMU, so that both systems can operate simultaneously.

## Technical Approach
- Rewire BNO085: PS0→3.3V, PS1→GND, connect SCK/MOSI/MISO/CS pins to Teensy 4.1.
- Update firmware to use `Adafruit_BNO08x.begin_SPI()` instead of I2C.
- Configure SPI at 1-3MHz with proper CS and interrupt pins.
- **PS3 Controller Conflict Resolution**: PS3 controller handled by Jetson (not Teensy), so no library conflict. This is a temporary solution until ESP32 WROOM module arrives for direct Bluetooth PS3 control on Teensy if desired.
- Test and validate improved sensor performance.

## Physical Wiring Changes

### Current I2C Wiring (to be removed):
- **SDA** (Yellow wire) → Currently on Teensy Pin 18 (disconnect)
- **SCL** (Blue wire) → Currently on Teensy Pin 19 (disconnect)

### New SPI Wiring (Teyleten Robot GY-BNO085 Board):
| Board Pin Label | Wire Color | Teensy 4.1 Pin | Pin Name | SPI Function | Notes |
|-----------------|------------|----------------|----------|--------------|-------|
| **VCC** | Red | Pin 24 | 3.3V | Power | Keep existing (3.3V only!) |
| **GND** | Black | Pin 23 | GND | Ground | Keep existing |
| **SCL** | Blue | **Pin 13** | SCK | SPI Clock | Shared pin (SCL in I2C, SCK in SPI) |
| **SDA** | Yellow | **Pin 11** | MOSI | SPI Master Out | Shared pin (SDA in I2C, MOSI in SPI) |
| **ADO** | Green | **Pin 12** | MISO | SPI Master In | Shared pin (ADO/ADR in I2C, MISO in SPI) |
| **CS** | White | **Pin 10** | CS | Chip Select | SPI mode only |
| **INT** | Orange | **Pin 9** | INT | Interrupt | Keep existing (optional but recommended) |
| **RST** | - | - | - | Reset | Not used (can leave unconnected) |
| **PS1** | - | - | GND | Mode Select | Jumper to GND (SPI mode) |
| **PSO** (PS0) | - | - | 3.3V | Mode Select | Jumper to 3.3V (SPI mode) - Note: labeled "PSO" on board |

**Note**: Teyleten Robot board uses **shared pins** - SCL/SDA/ADO serve different functions in I2C vs SPI mode.

### Mode Selection Jumpers (on Teyleten Robot GY-BNO085 board):
- **PSO** (PS0) → **3.3V** (SPI mode - **REQUIRED**) - *Note: Board is labeled "PSO" but this is PS0*
- **PS1** → **GND** (SPI mode - **REQUIRED**)

**Important**: These jumpers are **required** for SPI mode. If PS0/PS1 are left floating, BNO085 defaults to I2C mode at power-on. The mode is detected during initialization, so the jumpers must be in place before powering on.

### Visual Wiring Diagram (Teyleten Robot GY-BNO085)

```
Teyleten Robot GY-BNO085 Board        Teensy 4.1
┌──────────────────────────┐         ┌──────────────────┐
│  [Pin Layout - Bottom]   │         │                  │
│  1  2  3  4  5  6  7  8 9 10       │                  │
│ VCC GND SCL SDA ADO CS INT RST PS1 PSO│              │
│                          │         │                  │
│  VCC (Pin 1, Red)  ──────┼────────┼► Pin 24 (3.3V)   │
│                          │         │                  │
│  GND (Pin 2, Black)──────┼────────┼► Pin 23 (GND)    │
│                          │         │                  │
│  SCL (Pin 3, Blue)───────┼────────┼► Pin 13 (SCK)    │
│                          │         │   SPI Clock      │
│  SDA (Pin 4, Yellow)─────┼────────┼► Pin 11 (MOSI)   │
│                          │         │   Master Out     │
│  ADO (Pin 5, Green)──────┼────────┼► Pin 12 (MISO)   │
│                          │         │   Master In      │
│  CS (Pin 6, White)───────┼────────┼► Pin 10 (CS)     │
│                          │         │   Chip Select    │
│  INT (Pin 7, Orange)─────┼────────┼► Pin 9 (INT)     │
│                          │         │   Interrupt      │
│  RST (Pin 8)             │         │   (Not used)      │
│                          │         │                  │
│  PS1 (Pin 9)      ───────┼───► GND (Jumper)          │
│  PSO/PS0 (Pin 10) ───────┼───► 3.3V (Jumper)         │
│                          │         │                  │
└──────────────────────────┘         └──────────────────┘

Board Pin Order (Bottom Edge, Left to Right):
┌─────────────────────────────────────────────────────┐
│ 1    2    3    4    5    6    7    8    9    10    │
│ VCC  GND  SCL  SDA  ADO  CS   INT  RST  PS1  PSO   │
│ (Red)(Blk)(Blu)(Yel)(Grn)(Wht)(Or) (NC) (GND)(3.3V)│
└─────────────────────────────────────────────────────┘

Pin Layout on Teensy 4.1 (Left Side - Digital I/O):
┌─────────────────────────────────────────────┐
│  ...  │  9  │ 10  │ 11  │ 12  │ 13  │ ...   │
│       │ INT │ CS  │MOSI │MISO │ SCK │       │
│       │(Or) │(Wh) │(Yl) │(Gr) │(Bl) │       │
└─────────────────────────────────────────────┘

Pin Layout on Teensy 4.1 (Right Side - Power):
┌─────────────────────────┐
│  ...  │ 23  │ 24  │ ... │
│       │ GND │ 3.3V│     │
│       │(Blk)│(Red)│     │
└─────────────────────────┘

Legend:
  Red   = VCC (Power)
  Black = GND (Ground)
  Blue  = SCK (SPI Clock)
  Yellow= MOSI (Master Out Slave In)
  Green = MISO (Master In Slave Out)
  White = CS (Chip Select)
  Orange= INT (Interrupt)
```

### Wiring Steps (Teyleten Robot GY-BNO085):
1. **Keep existing connections**: 
   - VCC (Pin 1, Red) → Pin 24 (3.3V)
   - GND (Pin 2, Black) → Pin 23 (GND)
   - INT (Pin 7, Orange) → Pin 9 (INT)

2. **Disconnect I2C wires**: 
   - Remove SDA (Pin 4, Yellow) from Teensy Pin 18
   - Remove SCL (Pin 3, Blue) from Teensy Pin 19

3. **Reconnect shared pins for SPI** (same physical wires, different function):
   - SCL (Pin 3, Blue wire) → Pin 13 (SCK) - *Same wire, now used as SPI Clock*
   - SDA (Pin 4, Yellow wire) → Pin 11 (MOSI) - *Same wire, now used as SPI Master Out*
   - ADO (Pin 5, Green wire) → Pin 12 (MISO) - *New wire, used as SPI Master In*

4. **Connect SPI-specific pins**:
   - CS (Pin 6, White wire) → Pin 10 (Chip Select) - *New connection*

5. **Add mode select jumpers**:
   - PSO (Pin 10, PS0) → 3.3V (Jumper) - *Note: Board labeled "PSO" but is PS0*
   - PS1 (Pin 9) → GND (Jumper)

6. **Leave unconnected**: RST (Pin 8) - Not needed

**Total wires**: 7 (VCC, GND, SCL→SCK, SDA→MOSI, ADO→MISO, CS, INT)

**Board Specifications**: See `docs/hardware/BNO085_Teyleten_Robot_Specs.md` for complete specifications.

**Reference**: See `SPI_WIRING_GUIDE.md` for complete wiring diagram and troubleshooting.

## UX/UI Considerations
N/A (firmware-focused; debugging via serial monitor).

## Acceptance Criteria
- SPI communication established at 1-3MHz.
- IMU update rate ≥400Hz (target 1000Hz).
- Latency <0.5ms per sensor reading.
- PS3 controller works simultaneously via Jetson (no library conflicts on Teensy).
- All CoS from backlog met.

## Dependencies
- PBI 1 (Teensy Core Firmware) - must have working balance controller first.
- PS3 controller handled by Jetson (temporary solution - avoids conflict).
- SPI wiring guide (see `SPI_WIRING_GUIDE.md`).
- Board specifications (see `docs/hardware/BNO085_Teyleten_Robot_Specs.md`).

## Hardware Specifications (Teyleten Robot GY-BNO085)
- **Board Model**: Teyleten Robot GY-BNO085
- **SPI Speed**: Up to 3MHz (board supports)
- **I2C Speed**: Up to 400kHz (default mode)
- **Power**: 3.3V only (5V will damage!)
- **Pin Sharing**: SCL/SDA/ADO pins shared between I2C and SPI modes
- **Mode Selection**: PS0 (labeled "PSO") and PS1 control interface mode
- **Complete Specs**: See `docs/hardware/BNO085_Teyleten_Robot_Specs.md`

## Lessons Learned & Findings

### SPI Communication Failure (2025-01)
**Symptoms Observed**:
- MISO line consistently returns `0xFF` (floating/high)
- INT pin never goes LOW after CS wake-up signal
- Adafruit library initialization fails at all SPI speeds (500kHz - 3MHz)
- All 4 SPI modes tested (MODE0, MODE1, MODE2, MODE3) - all fail identically

**Root Cause Identified**:
- **Board Routing Issue**: ADO pin resistance measurements (ADO to GND = 68kΩ, ADO to VCC = 4.6kΩ) indicate ADO is strapped/resistored for I2C address selection, NOT routed to BNO085's MISO pin
- **Board Type**: Teyleten Robot GY-BNO085 is a budget clone board designed primarily for I2C, not SPI
- **Comparison**: Adafruit and SparkFun BNO085 boards include proper SPI routing and pull-up resistors, which is why they work with SPI

**Evidence**:
- Same chip works perfectly via I2C (proving chip is functional)
- All wiring verified correct (continuity, voltages, PS0/PS1 jumpers)
- CS pull-up resistor attempts failed (not the issue)
- See `SPI_TROUBLESHOOTING_SUMMARY.md` and `SPI_VS_I2C_ANALYSIS.md` for complete analysis

### I2C Optimization Success (2025-01)
**Achieved Performance**:
- **I2C Bus Speed**: 400kHz (Fast Mode) - 4x faster than original 100kHz
- **IMU Update Rate**: 400Hz - 4x faster than original 100Hz
- **IMU Communication Success**: 97.7-98.8% success rate
- **Roll Stability**: ±0.5° (excellent)
- **PID Update Rate**: 500Hz
- **Conclusion**: Optimized I2C provides sufficient performance for balancing without requiring SPI

**Key Insight**: The bottleneck wasn't I2C itself, but unoptimized I2C configuration. I2C at 400kHz-1MHz can handle 200-1000Hz update rates needed for self-balancing.

## Open Questions
- ~~Optimal SPI frequency (1MHz vs 3MHz) for reliability vs speed tradeoff.~~ **RESOLVED**: SPI not possible on current board
- Future: When ESP32 WROOM arrives, evaluate direct Bluetooth PS3 on Teensy vs keeping Jetson-based solution.

## Related Tasks
See [Tasks for PBI 2](./tasks.md) (to be created)

## Implementation Status

### Physical Wiring - ✅ COMPLETE
- **Date Completed**: 2025-01-XX
- **Status**: All SPI wiring connections completed per specifications above
- **Mode Selection Jumpers**: PS0→3.3V and PS1→GND jumpers installed
- **Next Step**: Upload SPI firmware and test communication

**Parent Backlog**: [Backlog.md](../backlog.md)


