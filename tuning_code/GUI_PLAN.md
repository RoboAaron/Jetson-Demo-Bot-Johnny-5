# Python GUI Development Plan

## Goal
Build a real-time monitoring and tuning GUI for the self-balancing robot within 1 hour.

## Architecture (Best Practices)

### 1. **Threading Model**
- **Main Thread**: GUI (tkinter) - handles UI updates
- **Serial Thread**: Reads serial data continuously (non-blocking)
- **Plot Thread**: Updates matplotlib plots (thread-safe)

### 2. **Serial Communication**
- Use `pyserial` with timeout to prevent blocking
- Parse data format: `R:0.00,P:0.00,Y:0.00,Pos:0.00,VelSet:0.00,VelAct:0.00,Curr:0.00,Bal:OK,Log:OFF`
- Parse stats: `📊 I2C Stats: Success=...` and `📊 VESC Stats: Success=...`
- Handle connection errors gracefully

### 3. **GUI Layout**
```
┌─────────────────────────────────────────────────────────┐
│  Connection: [Device Dropdown] [Connect] [Disconnect]    │
├─────────────────────────────────────────────────────────┤
│  IMU Data (Real-time)          │  Tuning Parameters     │
│  Roll: 0.00°                  │  Angle PID:            │
│  Pitch: 0.00°                  │    Kp: [▼] 8.0 [▲]     │
│  Yaw: 0.00°                    │    Ki: [▼] 0.2 [▲]     │
│  Balance: OK                   │    Kd: [▼] 0.4 [▲]      │
│                                │  Velocity PID:         │
│  Communication Rates:          │    Kp: [▼] 0.4 [▲]     │
│  IMU: 400 Hz (99%)             │    Ki: [▼] 0.15 [▲]     │
│  VESC: 67 Hz (98%)             │    Kd: [▼] 0.15 [▲]    │
│                                │  Motor Settings:       │
│  [Real-time Plot]              │    Max Current: [▼] [▲]│
│                                │    Setpoint: [▼] [▲]    │
├─────────────────────────────────────────────────────────┤
│  [Show All Tuning Values] [Reset Position] [Help]       │
└─────────────────────────────────────────────────────────┘
```

### 4. **Features**
- ✅ Auto-detect serial devices
- ✅ Real-time IMU values (Roll, Pitch, Yaw)
- ✅ Communication rate monitoring (IMU Hz, VESC Hz, success %)
- ✅ Tuning parameter adjustment (all PID gains)
- ✅ Real-time plotting (Roll, Pitch, Current)
- ✅ Connection status indicator
- ✅ Error handling and recovery

### 5. **Implementation Steps**
1. Create directory structure
2. Build serial communication module
3. Build GUI framework
4. Add real-time data display
5. Add parameter controls
6. Add plotting
7. Test and refine

## Time Estimate: 45-60 minutes


