# GUI Monitoring Options for Real-Time Robot Data

**Goal:** Monitor IMU values, tuning parameters, IMU Hz, comms Hz, and other significant data in real-time with a GUI.

---

## Existing Tools (Ready to Use)

### 1. **SerialPlot** (Recommended - Check First)
**What it is:** Free, open-source real-time serial data plotting tool  
**Platform:** Windows, Linux, macOS  
**Website:** https://github.com/hawkmp/SerialPlot  
**Features:**
- Real-time plotting of multiple data channels
- Parses CSV or space-separated data from serial
- Customizable graphs, colors, scales
- Can save data to CSV
- Lightweight and fast

**How to Use:**
1. Download from GitHub releases
2. Connect to your Teensy serial port
3. Configure data format (your robot outputs: `R:0.00,P:0.00,Y:0.00,...`)
4. Add channels for Roll, Pitch, Yaw, Velocity, Current, etc.

**Pros:**
- ✅ Ready to use immediately
- ✅ No coding required
- ✅ Real-time plotting
- ✅ Free and open-source

**Cons:**
- ❌ Can't adjust tuning parameters (read-only)
- ❌ May need to parse your data format

**Try This First!** It's the quickest solution.

---

### 2. **Arduino Serial Plotter** (Built-in, Limited)
**What it is:** Built into Arduino IDE  
**Platform:** All platforms  
**Features:**
- Basic real-time plotting
- Only plots numeric values
- Limited customization

**Limitations:**
- ❌ Can't show multiple data types (text + numbers)
- ❌ Can't adjust parameters
- ❌ Limited to Arduino IDE

**Verdict:** Too limited for your needs.

---

### 3. **VESC Tool** (VESC-Specific)
**What it is:** Official VESC configuration and monitoring tool  
**Platform:** Windows, Linux, macOS, iOS  
**Website:** https://github.com/vedderb/vesc_tool  
**Features:**
- Real-time VESC telemetry
- Parameter adjustment
- Data logging

**Limitations:**
- ❌ Only monitors VESC, not IMU or other data
- ❌ Doesn't show your custom tuning parameters
- ❌ Complex setup

**Verdict:** Not suitable for your needs (only VESC data).

---

## Custom Development Options

### Option A: Python + Tkinter + Matplotlib (Recommended for Custom)
**Why:** You already have Python experience (`log_serial.py`), and this is the most flexible.

**Libraries Needed:**
- `pyserial` - Serial communication (already have)
- `tkinter` - GUI framework (built into Python)
- `matplotlib` - Real-time plotting
- `numpy` - Data processing

**Features You Can Build:**
- ✅ Real-time IMU values (Roll, Pitch, Yaw) with graphs
- ✅ Tuning parameters display (Kp, Ki, Kd, etc.)
- ✅ Communication rates (IMU Hz, VESC Hz)
- ✅ Adjust tuning parameters via GUI (send commands to Teensy)
- ✅ Data logging
- ✅ Communication status indicators

**Estimated Development Time:** 4-8 hours for basic version

**Pros:**
- ✅ Fully customizable
- ✅ Can adjust parameters via GUI
- ✅ Real-time plotting
- ✅ Cross-platform

**Cons:**
- ❌ Requires development time
- ❌ Need to maintain code

---

### Option B: Python + PyQt5 (More Professional)
**Why:** More modern, professional-looking GUI than Tkinter

**Libraries Needed:**
- `pyserial` - Serial communication
- `PyQt5` - GUI framework
- `pyqtgraph` - Fast real-time plotting (better than matplotlib for real-time)

**Features:** Same as Option A, but with better UI

**Pros:**
- ✅ Professional appearance
- ✅ Better performance for real-time plotting
- ✅ More widgets/controls available

**Cons:**
- ❌ Larger dependencies
- ❌ More complex to learn
- ❌ Longer development time

---

### Option C: Web-Based Dashboard (Advanced)
**Why:** Access from any device, modern interface

**Technology Stack:**
- Python backend (Flask/FastAPI) - Serial communication
- Web frontend (HTML/JavaScript) - Dashboard UI
- WebSocket - Real-time data streaming
- Chart.js or Plotly.js - Real-time plotting

**Features:**
- ✅ Access from phone, tablet, laptop
- ✅ Modern web interface
- ✅ Real-time updates
- ✅ Can run on Jetson and access remotely

**Pros:**
- ✅ Very flexible
- ✅ Modern interface
- ✅ Remote access

**Cons:**
- ❌ Most complex to build
- ❌ Requires web server setup
- ❌ Longer development time

---

## Recommendation

### **Try SerialPlot First (15 minutes)**
1. Download SerialPlot: https://github.com/hawkmp/SerialPlot/releases
2. Install and run
3. Connect to your Teensy serial port
4. Configure to parse your data format
5. Add channels for Roll, Pitch, Yaw, Velocity, Current, etc.

**If SerialPlot works for you:** Great! You're done.

**If SerialPlot doesn't meet your needs:** Build a custom Python GUI (Option A).

---

## Quick Start: Custom Python GUI (If Needed)

If you want to build a custom solution, here's a basic structure:

```python
# robot_monitor_gui.py - Basic structure
import serial
import tkinter as tk
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import matplotlib.pyplot as plt
import re

class RobotMonitorGUI:
    def __init__(self):
        self.ser = serial.Serial('/dev/ttyACM0', 2000000)
        self.root = tk.Tk()
        self.setup_gui()
        self.update_data()
        
    def setup_gui(self):
        # Create plots for IMU data
        # Create labels for tuning values
        # Create buttons for parameter adjustment
        pass
        
    def update_data(self):
        # Read serial data
        # Parse: R:0.00,P:0.00,Y:0.00,...
        # Update plots and labels
        # Schedule next update
        self.root.after(10, self.update_data)
        
    def send_command(self, cmd):
        # Send command to Teensy (e.g., 'p' to decrease Kp)
        self.ser.write(cmd.encode())
```

**Would you like me to build this custom GUI for you?**

---

## Comparison Table

| Tool | Setup Time | Customization | Parameter Control | Real-time Plotting | Cost |
|------|-----------|---------------|-------------------|-------------------|------|
| **SerialPlot** | 15 min | Medium | ❌ No | ✅ Yes | Free |
| **Arduino Plotter** | 0 min | ❌ Low | ❌ No | ⚠️ Basic | Free |
| **VESC Tool** | 30 min | ❌ Low | ✅ Yes (VESC only) | ✅ Yes (VESC only) | Free |
| **Custom Python** | 4-8 hours | ✅ Full | ✅ Yes | ✅ Yes | Free |
| **PyQt5 GUI** | 8-12 hours | ✅ Full | ✅ Yes | ✅ Yes | Free |
| **Web Dashboard** | 12-20 hours | ✅ Full | ✅ Yes | ✅ Yes | Free |

---

## Next Steps

1. **Try SerialPlot first** - Download and test (15 minutes)
2. **If SerialPlot works:** You're done!
3. **If you need more:** Let me know and I'll build a custom Python GUI tailored to your exact needs

---

## Questions to Consider

- Do you need to **adjust parameters** via GUI, or just **monitor**?
- Do you need **real-time plotting** or just **numeric displays**?
- Do you need to **save/log data**?
- Do you need **remote access** (from phone/tablet)?

Based on your answers, I can recommend the best solution or build a custom one for you.





