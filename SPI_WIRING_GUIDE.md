# BNO085 SPI Wiring Guide - Teensy 4.1

**Date**: January 2025  
**Branch**: `feature/spi-migration`  
**Status**: ✅ Wiring Complete (2025-01-XX)

---

## Quick Summary

You're currently wired for **I2C**. To switch to **SPI**, you need to **reconnect 4 wires**.

### Wire Changes Required:
- ❌ **Disconnect**: SDA (Pin 18) and SCL (Pin 19)
- ✅ **Add**: SCK, MOSI, MISO (3 new wires)
- ✅ **Add**: Mode select jumpers on PS0/PS1

**Total wires**: 7 (vs 5 for I2C)

---

## Complete Wiring Table (Teyleten Robot GY-BNO085)

| Board Pin | Label | Wire Color | Teensy 4.1 Pin | Pin Name | SPI Function | Notes |
|-----------|-------|------------|----------------|----------|--------------|-------|
| 1 | **VCC** | Red | 3.3V Rail | 3.3V | Power | 3.3V only! |
| 2 | **GND** | Black | GND Rail | GND | Ground | Common ground |
| 3 | **SCL** | Blue | **Pin 13** | SCK | SPI Clock | Shared pin (SCL→SCK) |
| 4 | **SDA** | Purple | **Pin 11** | MOSI | SPI Master Out | Shared pin (SDA→MOSI) |
| 5 | **ADO** | Orange | **Pin 12** | MISO | SPI Master In | Shared pin (ADO→MISO) |
| 6 | **CS** | White | **Pin 10** | CS | Chip Select | SPI mode only |
| 7 | **INT** | Green | **Pin 9** | INT | Interrupt | Required for SPI |
| 8 | **RST** | Yellow | **Pin 14** | RST | Reset | Required for reliable SPI |
| 9 | **PS1** | - | **3.3V** | Mode Select | Mode Select | **Jumper to 3.3V** |
| 10 | **PSO** (PS0) | - | **3.3V** | Mode Select | Mode Select | **Jumper to 3.3V** |

**Note**: Teyleten Robot board uses **shared pins** - SCL/SDA/ADO serve different functions in I2C vs SPI mode.

### Mode Selection Jumpers (CRITICAL for SPI):
- **PSO** (Pin 10, PS0) → **3.3V** (HIGH) - *Note: Board labeled "PSO" but this is PS0*
- **PS1** (Pin 9) → **3.3V** (HIGH) - *BOTH must be HIGH for SPI!*

⚡ **IMPORTANT**: Previous documentation incorrectly stated PS1→GND. Per Adafruit and BNO085 datasheet, **BOTH PS0 and PS1 must be HIGH (3.3V) for SPI mode!**

### Required Connections:
- ✅ **RST** (Reset pin) - Connected to Pin 14 for reliable initialization
- ✅ **INT** (Interrupt) - Required for SPI communication

---

## Wiring Instructions

### STEP 1: Keep Existing Power Connections
✅ **These stay connected:**
- VCC (Red) → Pin 24 (3.3V)
- GND (Black) → Pin 23 (GND)

### STEP 2: Disconnect I2C Wires
❌ **Remove these connections:**
- SDA → Was on Pin 18 (no longer needed)
- SCL → Was on Pin 19 (no longer needed)

### STEP 3: Reconnect Shared Pins for SPI
✅ **Reconnect existing wires to new pins** (same wires, different function):
- SCL (Pin 3, Blue wire) → Pin 13 (SCK) - *Was on Pin 19, now SPI Clock*
- SDA (Pin 4, Yellow wire) → Pin 11 (MOSI) - *Was on Pin 18, now SPI Master Out*
- ADO (Pin 5, Green wire) → Pin 12 (MISO) - *New connection, SPI Master In*

### STEP 4: Connect SPI-Specific Pin
✅ **Connect new wire:**
- CS (Pin 6, White wire) → Pin 10 (Chip Select) - *New connection for SPI*

### STEP 5: Keep INT Wire
✅ **This stays connected:**
- INT (Pin 7, Orange) → Pin 9 (Interrupt)

### STEP 6: Connect Reset Pin
✅ **Connect new wire:**
- RST (Pin 8, Yellow wire) → Pin 14 (Reset) - *Required for reliable SPI init*

### STEP 7: Add Mode Select Jumpers
✅ **On the Teyleten Robot GY-BNO085 board:**
- PSO (Pin 10, PS0) → **3.3V** (enables SPI mode) - *Note: Board labeled "PSO" but is PS0*
- PS1 (Pin 9) → **3.3V** (enables SPI mode) - *BOTH must be HIGH!*

⚡ **WARNING**: Both PS0 and PS1 must be connected to 3.3V for SPI mode. Previous documentation incorrectly stated PS1→GND.

---

## Visual Wiring Diagram

```
Teyleten Robot GY-BNO085 Board        Teensy 4.1
┌──────────────────────────┐         ┌──────────────────┐
│  [Pin Layout - Bottom]   │         │                  │
│  1  2  3  4  5  6  7  8 9 10       │                  │
│ VCC GND SCL SDA ADO CS INT RST PS1 PSO│              │
│                          │         │                  │
│  VCC (Pin 1, Red)  ──────┼────────┼► Pin 24 (3.3V)  │
│                          │         │                  │
│  GND (Pin 2, Black)──────┼────────┼► Pin 23 (GND)    │
│                          │         │                  │
│  SCL (Pin 3, Blue)───────┼────────┼► Pin 13 (SCK)    │
│                          │         │   SPI Clock      │
│  SDA (Pin 4, Purple)─────┼────────┼► Pin 11 (MOSI)  │
│                          │         │   Master Out     │
│  ADO (Pin 5, Green)──────┼────────┼► Pin 12 (MISO)  │
│                          │         │   Master In      │
│  CS (Pin 6, White)───────┼────────┼► Pin 10 (CS)    │
│                          │         │   Chip Select    │
│  INT (Pin 7, Orange)─────┼────────┼► Pin 9 (INT)    │
│                          │         │   Interrupt      │
│  RST (Pin 8)             │         │   (Not used)     │
│                          │         │                  │
│  PS1 (Pin 9)      ───────┼───► GND (Jumper)         │
│  PSO (Pin 10, PS0)───────┼───► 3.3V (Jumper)         │
│                          │         │                  │
└──────────────────────────┘         └──────────────────┘

Board Pin Order (Bottom Edge, Left to Right):
┌─────────────────────────────────────────────────────┐
│ 1    2    3    4    5    6    7    8    9    10    │
│ VCC  GND  SCL  SDA  ADO  CS   INT  RST  PS1  PSO    │
│ (Red)(Blk)(Blu)(Pur)(Or) (Wht)(Grn)(NC) (3.3V)(3.3V)│
└─────────────────────────────────────────────────────┘
```

---

## Pin Locations on Teensy 4.1

**Left Side (Digital I/O Pins):**
- Pin 9 (INT): Right side, near edge
- Pin 10 (CS): Next to Pin 9
- Pin 11 (MOSI): Next to Pin 10
- Pin 12 (MISO): Next to Pin 11
- Pin 13 (SCK): Next to Pin 12

**Right Side (Power Pins):**
- Pin 23 (GND): Right side, near edge
- Pin 24 (3.3V): Right side, next to GND

---

## SPI Configuration Details

### SPI Settings (in code):
- **Clock Speed**: 3 MHz (conservative, BNO085 supports up to 3 MHz)
- **Mode**: SPI_MODE0 (CPOL=0, CPHA=0)
- **Bit Order**: MSBFIRST

### Why These Settings?
- 3 MHz provides fast communication while staying within spec
- Much faster than I2C @ 100kHz (30x faster!)
- SPI_MODE0 is the most common and well-tested mode

---

## Troubleshooting

### Issue: "BNO085 SPI initialization failed"

**Check 1**: PS0/PS1 jumpers (Teyleten Robot board)
- PSO (Pin 10, PS0) must be connected to **3.3V** (not floating) - *Note: Board labeled "PSO"*
- PS1 (Pin 9) must be connected to **GND** (not floating)
- Without these, BNO085 won't enter SPI mode

**Check 2**: Wire connections
- Verify all 7 wires are connected securely
- Check for loose connections or shorts

**Check 3**: Power
- Ensure VCC is connected to 3.3V (not 5V!)
- BNO085 is 3.3V only, 5V will damage it

**Check 4**: CS Pull-up
- Some breakout boards need a 10kΩ pull-up on CS
- If yours doesn't have it built-in, add external pull-up

### Issue: "Communication errors"

**Solution**: Try slower SPI speed
- Change from 3 MHz to 1 MHz in code
- Long wires (>12") may need slower speed

---

## Performance Comparison

| Metric | I2C @ 100kHz | SPI @ 3MHz |
|--------|--------------|------------|
| **Speed** | ~10k bytes/s | ~375k bytes/s |
| **Latency** | 2-5 ms | <0.5 ms |
| **Jitter** | High (clock stretching) | Low (hardware sync) |
| **Reliability** | Good for 1-5Hz | Excellent for 1kHz+ |

**Expected Improvement**: 10-50x faster IMU communication

---

## Code Changes Required

### Before (I2C):
```cpp
#include <Wire.h>
#include <Adafruit_BNO08x.h>

Adafruit_BNO08x bno08x(-1);  // I2C mode

void setup() {
  Wire.begin();
  bno08x.begin_I2C(0x4A);
}
```

### After (SPI):
```cpp
#include <SPI.h>
#include <Adafruit_BNO08x.h>

#define BNO08X_CS 10
#define BNO08X_INT 9

Adafruit_BNO08x bno08x(-1);  // SPI mode

void setup() {
  SPI.begin();
  bno08x.begin_SPI(BNO08X_CS, BNO08X_INT, &SPI, 3000000);
}
```

---

## Next Steps

1. ✅ **Physical Wiring** (COMPLETE - 2025-01-XX)
2. ⏭️ **Update Code** (upload SPI firmware to Teensy)
3. ⏭️ **Test IMU** (verify SPI communication works)
4. ⏭️ **Tune Performance** (increase IMU update rate to 400-1000Hz)

---

**Wiring Complete!** Next: Upload SPI firmware and test communication.
