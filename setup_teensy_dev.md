# Teensy 4.1 Development Setup

*Complete guide for setting up Teensy 4.1 development environment for the Jetson Self-Balancing Robot*

## 1. Install Arduino IDE

### Download and Install
- Go to: https://www.arduino.cc/en/software
- Download Arduino IDE 2.3.2 or later
- Install with default settings
- Launch Arduino IDE to verify installation

### Verify Installation
- Open Arduino IDE
- Go to Help → About Arduino IDE
- Confirm version 2.3.2 or later

## 2. Install Teensyduino

### Download Teensyduino
- Go to: https://www.pjrc.com/teensy/td_download.html
- Select your Arduino IDE version (2.3.2)
- Download the installer

### Install Teensyduino
- Run the Teensyduino installer
- Select your Arduino IDE installation directory
- **Important**: Install ALL libraries and tools
- Complete the installation

### Verify Installation
- Open Arduino IDE
- Go to Tools → Board → Teensy 4.1
- You should see "Teensy 4.1" in the board list

## 3. Install Required Libraries

### Via Arduino IDE Library Manager
Open Arduino IDE → Tools → Manage Libraries and install:

#### Essential Libraries
- **Adafruit BNO08x** (by Adafruit) - BNO085 IMU sensor library
  ⚠️  NOTE: The robot uses a **BNO085**, NOT a BNO055. These are different chips.
  The correct library is **Adafruit BNO08x** (`Adafruit_BNO08x.h`).
  The SparkFun BNO08x library conflicts with Teensy's USB stack — do not use it.
  See `firmware/FIRMWARE_DESIGN.md` §3 for the full decision record.
- **VescUart** - VESC serial UART communication (motor controller interface)
- **PID** (by Brett Beauregard, `PID_v1.h`) - PID controller library
- **Wire** (built-in) - I2C communication
- **EEPROM** (built-in) - Configuration storage

#### Optional Libraries
- **Servo** (built-in) - If using servo motors
- **SoftwareSerial** (built-in) - Additional serial ports
- **Time** (built-in) - Time functions

### Install via Library Manager
1. Open Arduino IDE
2. Go to Tools → Manage Libraries
3. Search for each library name
4. Click "Install" for each library

## 4. Configure Board Settings

### Board Configuration
- **Board**: Teensy 4.1
- **USB Type**: Serial
- **CPU Speed**: 600 MHz (recommended for robot)
- **Optimize**: Faster (for better performance)

### Serial Port
- Connect Teensy via USB
- Go to Tools → Port
- Select the correct COM port (e.g., COM3, COM4)

## 5. Test Connection

### Upload Test Sketch
1. Open Arduino IDE
2. Go to File → Examples → 01.Basics → Blink
3. Select Tools → Board → Teensy 4.1
4. Select correct COM port
5. Click Upload (arrow button)
6. Teensy LED should blink

### Serial Monitor Test
1. Open Tools → Serial Monitor
2. Set baud rate to 115200
3. Upload this test code:

```cpp
void setup() {
  Serial.begin(115200);
  Serial.println("Teensy 4.1 Ready!");
}

void loop() {
  Serial.println("Hello from Teensy!");
  delay(1000);
}
```

## 6. Development Workflow

### Creating New Sketches
1. File → New
2. Save as: `robot_balance_controller.ino`
3. Select Teensy 4.1 board
4. Write your code
5. Upload to Teensy

### Debugging
- Use Serial Monitor for debugging
- Set baud rate to 115200
- Add `Serial.println()` statements for debugging
- Use Serial Plotter for real-time data visualization

### Library Management
- Add libraries via Tools → Manage Libraries
- Check library documentation for usage examples
- Update libraries regularly

## 7. Robot-Specific Configuration

### IMU Setup (BNO085)
The robot uses a **BNO085** with the **Adafruit BNO08x** library (`Adafruit_BNO08x.h`).
⚠️  Do NOT use `Adafruit_BNO055` (different chip) or `SparkFun BNO08x` (conflicts with
Teensy USB stack). See `firmware/FIRMWARE_DESIGN.md` for the full decision record.

```cpp
// Use Adafruit BNO08x library — the actual library used in this project
#include <Wire.h>
#include <Adafruit_BNO08x.h>

Adafruit_BNO08x bno08x(-1);  // -1 = no reset pin (I2C mode)
sh2_SensorValue_t sensorValue;

void setup() {
  Wire.begin();
  Wire.setClock(400000);  // 400kHz I2C
  if (!bno08x.begin_I2C()) {
    Serial.println("BNO085 not detected! Check wiring.");
    while (1);
  }
  // Enable rotation vector report at 400Hz (2500 µs)
  bno08x.enableReport(SH2_ROTATION_VECTOR, 2500);
  Serial.println("BNO085 OK");
}

void loop() {
  if (bno08x.getSensorEvent(&sensorValue)) {
    if (sensorValue.sensorId == SH2_ROTATION_VECTOR) {
      // Convert quaternion to roll/pitch/yaw — see active firmware for full conversion
      float qr = sensorValue.un.rotationVector.real;
      float qi = sensorValue.un.rotationVector.i;
      float qj = sensorValue.un.rotationVector.j;
      float qk = sensorValue.un.rotationVector.k;
      // roll = atan2(2*(qr*qi + qj*qk), 1 - 2*(qi*qi + qj*qj))  [radians → degrees]
    }
  }
}
```

### VESC Communication (UART)
The robot uses **VescUart** (UART serial), not CAN bus.

```cpp
#include <VescUart.h>

VescUart vescLeft;
VescUart vescRight;

void setup() {
  Serial1.begin(115200);  // Left VESC UART
  Serial2.begin(115200);  // Right VESC UART
  vescLeft.setSerialPort(&Serial1);
  vescRight.setSerialPort(&Serial2);
}

void loop() {
  // Read encoder feedback
  if (vescLeft.getVescValues()) {
    float erpm = vescLeft.data.rpm;
    float current = vescLeft.data.avgMotorCurrent;
  }
  // Send current command
  vescLeft.setCurrent(2.5);   // 2.5 Amps
}
```

### I2C Communication
```cpp
#include <Wire.h>

void setup() {
  Wire.begin();
  Serial.begin(115200);
}

void loop() {
  Wire.beginTransmission(0x68); // IMU address
  Wire.write(0x00); // Register address
  Wire.endTransmission();
}
```

## 8. Troubleshooting

### Common Issues

#### Teensy Not Detected
- Check USB cable (use data cable, not charge-only)
- Try different USB port
- Install Teensyduino drivers
- Restart Arduino IDE

#### Upload Fails
- Press reset button on Teensy
- Try different USB port
- Check COM port selection
- Verify board selection

#### Library Not Found
- Install library via Library Manager
- Check library name spelling
- Restart Arduino IDE
- Check library compatibility

#### Serial Monitor Issues
- Verify baud rate (115200)
- Check COM port selection
- Ensure code has `Serial.begin(115200)`
- Try different USB port

### Getting Help
- PJRC Forum: https://forum.pjrc.com/
- Arduino Reference: https://www.arduino.cc/reference/
- Teensy Documentation: https://www.pjrc.com/teensy/

---

*This setup guide ensures your Teensy 4.1 is ready for robot development with all necessary libraries and tools configured.*
