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
- **SparkFun BNO08x Arduino Library** (by SparkFun) - BNO085 IMU sensor library
  ⚠️  NOTE: The robot uses a **BNO085**, NOT a BNO055. These are different chips
  with incompatible libraries. Use the SparkFun BNO08x library, NOT Adafruit_BNO055.
  See `firmware/FIRMWARE_DESIGN.md` §2 for full comparison.
- **FlexCAN_T4** (by tonton81) - CAN bus for Teensy 4.x (replaces generic CAN library)
- **Wire** (built-in) - I2C communication
- **SPI** (built-in) - SPI communication
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
The robot uses a **BNO085** (SparkFun BNO08x library), not the BNO055.
The BNO085 uses a report-based interface — you request the data type you want
(rotation vector, accelerometer, etc.) rather than reading registers directly.

```cpp
// ⚠️  Use SparkFun BNO08x library — NOT Adafruit_BNO055
#include <Wire.h>
#include <SparkFun_BNO08x_Arduino_Library.h>

BNO08x imu;

void setup() {
  Wire.begin();
  if (!imu.begin(0x4A, Wire)) {   // 0x4A = default I2C address (SA0=GND)
    Serial.println("BNO085 not detected! Check wiring.");
    while (1);
  }
  // Request ARVR-stabilized rotation vector @ 200 Hz (5000 µs)
  imu.enableRotationVector(5000);
  Serial.println("BNO085 OK");
}

void loop() {
  if (imu.wasReset()) {
    imu.enableRotationVector(5000);  // Re-enable reports after reset
  }
  if (imu.getSensorEvent() &&
      imu.getSensorEventID() == SENSOR_REPORTID_ROTATION_VECTOR) {
    float pitch = imu.getRoll();   // axis mapping depends on mounting
    Serial.println(pitch);
  }
}
```

### CAN Bus Setup (FlexCAN_T4 for Teensy 4.x)
```cpp
// Use FlexCAN_T4 — NOT the generic "CAN" library (Thomas Barth)
// FlexCAN_T4 supports the Teensy 4.1's native CAN FD controller.
#include <FlexCAN_T4.h>

FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> can;

void setup() {
  can.begin();
  can.setBaudRate(500000);  // 500 kbps — matches FSESC setup
  Serial.begin(115200);
}

void loop() {
  CAN_message_t msg;
  if (can.read(msg)) {
    // Process incoming frame
    Serial.print("ID: "); Serial.println(msg.id, HEX);
  }
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
