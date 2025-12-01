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

def send_status_to_oled(keyboard, message):
    """Send LIFX status message to keyboard for OLED display"""
    # Format: [report_id, 0xF3, 0x04, message_bytes...]
    data = [0] * 33  # 33 bytes: 1 for report ID + 32 for data
    data[0] = 0x00  # Report ID (required for HID)
    data[1] = 0xF3  # LIFX command identifier
    data[2] = 0x04  # Status message command

    # Encode message (max 27 bytes to fit in remaining space)
    message_bytes = message[:27].encode('utf-8')
    for i, byte in enumerate(message_bytes):
        data[3 + i] = byte

    keyboard.write(bytes(data))

def discover_lifx_lamp(keyboard):
    """Discover LIFX lamp on network - only connect to 'Desk Light'"""
    global lifx, lamp

    lifx = LifxLAN()  # Discover all lamps
    devices = lifx.get_lights()

    if not devices:
        send_status_to_oled(keyboard, "Could not find Lamp")
        return False

    # Filter for "Desk Light" only
    for device in devices:
        label = device.get_label()
        if label == "Desk Light":
            lamp = device
            send_status_to_oled(keyboard, f"Found: {label}")

            # Get current state
            try:
                power = lamp.get_power()
                color = lamp.get_color()
                brightness = color[2]  # HSBK format: [hue, saturation, brightness, kelvin]
                return True
            except Exception as e:
                send_status_to_oled(keyboard, "Could not find Lamp")
                return False

    # "Desk Light" not found
    send_status_to_oled(keyboard, "Could not find Lamp")
    return False

def toggle_lamp():
    """Toggle lamp on/off"""
    global lamp

    if not lamp:
        return

    try:
        current_power = lamp.get_power()
        new_power = 0 if current_power else 65535
        lamp.set_power(new_power, duration=500)  # 500ms transition
    except Exception as e:
        pass

def adjust_brightness(direction):
    """Adjust lamp brightness

    Args:
        direction: 1 for increase (CW), -1 for decrease (CCW)
    """
    global lamp, current_brightness

    if not lamp:
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
    except:
        pass

def hid_listener():
    """Listen for HID commands from keyboard"""
    keyboard_path = find_keyboard()
    if not keyboard_path:
        return

    try:
        keyboard = hid.device()
        keyboard.open_path(keyboard_path)
    except:
        return

    # Discover LIFX lamp and send status to OLED
    if not discover_lifx_lamp(keyboard):
        return  # Exit if lamp not found

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
        pass
    finally:
        keyboard.close()

def main():
    # Start listening for commands (discovery happens inside)
    hid_listener()

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
