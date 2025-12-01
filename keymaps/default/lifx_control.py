#!/usr/bin/env python3
"""
LIFX Smart Lamp Control for QMK TIDBIT Keyboard
Controls LIFX lamp brightness and power via HID RAW commands from encoder 3

Encoder 3 Controls (KC_P0):
  CW rotation  = Increase brightness
  CCW rotation = Decrease brightness
  Button press = Toggle lamp on/off

Setup:
1. Install lifxlan: pip install lifxlan
2. Ensure LIFX lamp is on the same network
3. Run this script, it will auto-discover your LIFX lamp
"""

import hid
import time
from lifxlan import LifxLAN

# QMK HID RAW settings
VID = 0x6E61  # OffReno keyboard vendor ID
PID = 0x6064  # OffReno keyboard product ID

# LIFX settings
BRIGHTNESS_STEP = 6553  # ~10% of 65535 max brightness
MIN_BRIGHTNESS = 6553   # Minimum 10%
MAX_BRIGHTNESS = 65535  # Maximum 100%

# Global state
lifx = None
lamp = None
current_brightness = 32768  # Start at ~50%

def find_keyboard():
    """Find the OffReno keyboard RAW HID interface"""
    for device in hid.enumerate():
        if device['vendor_id'] == VID and device['product_id'] == PID:
            # QMK RAW HID uses usage_page 0xFF60 and usage 0x61
            if device.get('usage_page') == 0xFF60 and device.get('usage') == 0x61:
                print(f"Found RAW HID interface: {device.get('interface_number', 'N/A')}")
                return device['path']

    # Fallback
    print("RAW HID interface not found by usage page, trying fallback...")
    for device in hid.enumerate():
        if device['vendor_id'] == VID and device['product_id'] == PID:
            print(f"Using interface: {device.get('interface_number', 'N/A')}")
            return device['path']

    return None

def discover_lifx_lamp():
    """Discover LIFX lamp on network"""
    global lifx, lamp

    print("Discovering LIFX devices on network...")
    lifx = LifxLAN(1)  # Expect 1 lamp
    devices = lifx.get_lights()

    if not devices:
        print("ERROR: No LIFX devices found!")
        print("Make sure your LIFX lamp is powered on and connected to WiFi.")
        return False

    lamp = devices[0]
    print(f"Found LIFX lamp: {lamp.get_label()}")

    # Get current state
    try:
        power = lamp.get_power()
        power_str = "ON" if power else "OFF"
        print(f"  Power: {power_str}")

        color = lamp.get_color()
        brightness = color[2]  # HSBK format: [hue, saturation, brightness, kelvin]
        print(f"  Brightness: {int(brightness / 65535 * 100)}%")

        return True
    except Exception as e:
        print(f"ERROR: Could not communicate with lamp: {e}")
        return False

def toggle_lamp():
    """Toggle lamp on/off"""
    global lamp

    if not lamp:
        print("ERROR: No lamp connected")
        return

    try:
        current_power = lamp.get_power()
        new_power = 0 if current_power else 65535
        lamp.set_power(new_power, duration=500)  # 500ms transition

        state = "ON" if new_power else "OFF"
        print(f"Lamp toggled {state}")
    except Exception as e:
        print(f"ERROR toggling lamp: {e}")

def adjust_brightness(direction):
    """Adjust lamp brightness

    Args:
        direction: 1 for increase (CW), -1 for decrease (CCW)
    """
    global lamp, current_brightness

    if not lamp:
        print("ERROR: No lamp connected")
        return

    try:
        # Get current color (HSBK format)
        color = lamp.get_color()
        hue, saturation, brightness, kelvin = color

        # Calculate new brightness
        new_brightness = brightness + (BRIGHTNESS_STEP * direction)
        new_brightness = max(MIN_BRIGHTNESS, min(MAX_BRIGHTNESS, new_brightness))

        # Set new brightness
        lamp.set_color([hue, saturation, new_brightness, kelvin], duration=100)

        current_brightness = new_brightness
        percent = int(new_brightness / 65535 * 100)
        print(f"Brightness: {percent}%")
    except Exception as e:
        print(f"ERROR adjusting brightness: {e}")

def hid_listener():
    """Listen for HID commands from keyboard"""
    keyboard_path = find_keyboard()
    if not keyboard_path:
        print("ERROR: Could not find OffReno keyboard!")
        return

    try:
        keyboard = hid.device()
        keyboard.open_path(keyboard_path)
        print("Connected to OffReno keyboard\n")
    except Exception as e:
        print(f"ERROR: Could not open keyboard: {e}")
        return

    print("Listening for encoder commands...")
    print("  CW rotation  = Brightness up")
    print("  CCW rotation = Brightness down")
    print("  Button press = Toggle on/off")
    print("\nPress Ctrl+C to stop\n")

    try:
        while True:
            # Read HID data with timeout
            data = keyboard.read(33, timeout_ms=100)

            if data and len(data) > 0 and data[0] == 0xF3:  # LIFX command
                command = data[1]

                if command == 0x01:  # CW rotation - brightness up
                    adjust_brightness(1)

                elif command == 0x02:  # CCW rotation - brightness down
                    adjust_brightness(-1)

                elif command == 0x03:  # Button press - toggle on/off
                    toggle_lamp()

            time.sleep(0.01)  # Small delay to prevent CPU spinning

    except KeyboardInterrupt:
        print("\n\nStopping LIFX control...")
    finally:
        keyboard.close()

def main():
    print("=" * 50)
    print("LIFX Smart Lamp Control")
    print("=" * 50)
    print()

    # Discover LIFX lamp
    if not discover_lifx_lamp():
        return

    print()
    print("=" * 50)
    print("Ready! Use encoder 3 (KC_P0) to control lamp")
    print("=" * 50)
    print()

    # Start listening for commands
    hid_listener()

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n\nLIFX control stopped")
