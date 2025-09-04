@echo off
echo Setting up Windows development environment for Jetson Self-Balancing Robot...

REM Install Python 3.11
echo Installing Python 3.11...
winget install Python.Python.3.11

REM Install Git
echo Installing Git...
winget install Git.Git

REM Install VS Code
echo Installing VS Code...
winget install Microsoft.VisualStudioCode

REM Install Arduino IDE
echo Installing Arduino IDE...
winget install Arduino.ArduinoIDE

REM Install PuTTY for serial communication
echo Installing PuTTY...
winget install PuTTY.PuTTY

REM Wait for installations to complete
echo Waiting for installations to complete...
timeout /t 30

REM Install Python packages
echo Installing Python packages...
pip install pyserial colorama python-can cantools

echo.
echo Manual installations needed:
echo 1. Download Teensyduino from https://www.pjrc.com/teensy/td_download.html
echo 2. Download VESC Tool from https://vesc-project.com/vesc_tool
echo 3. Install VS Code extensions:
echo    - Arduino (Microsoft)
echo    - PlatformIO IDE
echo    - Python (Microsoft)
echo.
echo Setup complete!
pause
