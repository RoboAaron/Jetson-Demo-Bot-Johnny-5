#!/bin/bash
# Quick start script for Robot Tuning GUI

cd "$(dirname "$0")"

echo "Starting Robot Tuning GUI..."
echo ""

# Check if Python 3 is available
if ! command -v python3 &> /dev/null; then
    echo "Error: python3 not found. Please install Python 3."
    exit 1
fi

# Check if required packages are installed
echo "Checking dependencies..."
python3 -c "import serial" 2>/dev/null || {
    echo "Installing pyserial..."
    pip3 install pyserial
}

python3 -c "import matplotlib" 2>/dev/null || {
    echo "Installing matplotlib..."
    pip3 install matplotlib
}

python3 -c "import tkinter" 2>/dev/null || {
    echo "Error: tkinter not found."
    echo "On Ubuntu/Debian, install with: sudo apt-get install python3-tk"
    exit 1
}

echo "Starting GUI..."
python3 robot_tuning_gui.py


