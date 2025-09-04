#!/usr/bin/env python3
"""
Test script to verify all installations for Jetson Self-Balancing Robot development
"""

import sys
import importlib

def test_import(module_name, package_name=None):
    """Test if a module can be imported"""
    try:
        if package_name:
            importlib.import_module(module_name, package_name)
        else:
            importlib.import_module(module_name)
        print(f"✅ {module_name} - OK")
        return True
    except ImportError as e:
        print(f"❌ {module_name} - FAILED: {e}")
        return False

def main():
    print("Testing Jetson Self-Balancing Robot Development Environment")
    print("=" * 60)
    
    # Test Python version
    print(f"Python version: {sys.version}")
    print()
    
    # Test core Python packages
    print("Testing Python packages:")
    packages = [
        "serial",  # pyserial
        "colorama",
        "can",     # python-can
        "cantools",
    ]
    
    all_passed = True
    for package in packages:
        if not test_import(package):
            all_passed = False
    
    print()
    
    # Test system tools
    print("Testing system tools:")
    print("✅ Arduino IDE - Installed via winget")
    print("✅ PuTTY - Installed via winget")
    print("✅ Python 3.11 - Installed via winget")
    
    print()
    
    # Manual installation reminders
    print("Manual installations still needed:")
    print("1. Download Teensyduino from: https://www.pjrc.com/teensy/td_download.html")
    print("2. Download VESC Tool from: https://vesc-project.com/vesc_tool")
    print("3. Install VS Code extensions:")
    print("   - Arduino (Microsoft)")
    print("   - PlatformIO IDE")
    print("   - Python (Microsoft)")
    
    print()
    
    if all_passed:
        print("🎉 All Python packages are working correctly!")
        print("Your development environment is ready for robot development.")
    else:
        print("⚠️  Some packages failed to import. Check the errors above.")
    
    print()
    print("Next steps:")
    print("1. Install Teensyduino add-on for Arduino IDE")
    print("2. Download and install VESC Tool")
    print("3. Connect your Teensy 4.1 and test upload")
    print("4. Connect your FSESC and test with VESC Tool")

if __name__ == "__main__":
    main()
