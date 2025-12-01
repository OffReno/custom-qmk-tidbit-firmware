#!/usr/bin/env python3
"""
System Monitor for QMK TIDBIT Keyboard - HID RAW Version
Sends CPU%, GPU%, GPU Memory%, and PING data directly to keyboard via USB HID RAW
"""

import psutil
import time
import sys
import subprocess

try:
    import pynvml
    NVIDIA_AVAILABLE = True
except ImportError:
    NVIDIA_AVAILABLE = False
    print("pynvml not available - GPU monitoring disabled")

try:
    import hid
    HID_AVAILABLE = True
except ImportError:
    HID_AVAILABLE = False
    print("ERROR: hidapi not available - install with: pip install hidapi")
    print("This is REQUIRED for sending data to the keyboard!")
    exit(1)

# QMK HID RAW settings
VID = 0x6E61  # OffReno keyboard vendor ID
PID = 0x6064  # OffReno keyboard product ID
USAGE_PAGE = 0xFF60
USAGE = 0x61

def get_gpu_load():
    if not NVIDIA_AVAILABLE:
        return 0
    try:
        pynvml.nvmlInit()
        handle = pynvml.nvmlDeviceGetHandleByIndex(0)
        utilization = pynvml.nvmlDeviceGetUtilizationRates(handle)
        return utilization.gpu
    except:
        return 0

def get_cpu_load():
    return int(psutil.cpu_percent(interval=0.1))

def get_gpu_memory():
    """Get GPU memory usage percentage using pynvml"""
    if not NVIDIA_AVAILABLE:
        return 0
    try:
        pynvml.nvmlInit()
        handle = pynvml.nvmlDeviceGetHandleByIndex(0)
        mem_info = pynvml.nvmlDeviceGetMemoryInfo(handle)
        # Return percentage of memory used
        return int((mem_info.used / mem_info.total) * 100)
    except:
        return 0

def get_ping():
    try:
        # On Windows, prevent console window flash
        if sys.platform == 'win32':
            startupinfo = subprocess.STARTUPINFO()
            startupinfo.dwFlags |= subprocess.STARTF_USESHOWWINDOW
            creationflags = subprocess.CREATE_NO_WINDOW
        else:
            startupinfo = None
            creationflags = 0

        result = subprocess.run(['ping', '-n', '1', '-w', '1000', '8.8.8.8'],
                              capture_output=True, text=True, timeout=2,
                              startupinfo=startupinfo, creationflags=creationflags)
        if 'time=' in result.stdout:
            time_str = result.stdout.split('time=')[1].split('ms')[0]
            return int(time_str.strip())
        return 0
    except:
        return 0

def find_keyboard():
    print("Searching for keyboard...")
    for device in hid.enumerate():
        if device['vendor_id'] == VID and device['product_id'] == PID:
            if device['usage_page'] == USAGE_PAGE and device['usage'] == USAGE:
                print(f"Found keyboard: {device['product_string']}")
                return device['path']
    for device in hid.enumerate():
        if device['vendor_id'] == VID and device['product_id'] == PID:
            print(f"Found possible keyboard: {device['product_string']}")
            return device['path']
    return None

def main():
    print("System Monitor with HID RAW started...")
    keyboard_path = find_keyboard()
    if not keyboard_path:
        print("ERROR: Could not find keyboard!")
        print(f"Looking for VID: 0x{VID:04X}, PID: 0x{PID:04X}")
        return

    try:
        # Fixed: Use correct hidapi syntax
        keyboard = hid.device()
        keyboard.open_path(keyboard_path)
        print("Connected to keyboard successfully!")
    except Exception as e:
        print(f"ERROR: Could not open keyboard: {e}")
        return

    print("\nMonitoring started. Press Ctrl+C to stop.\n")

    while True:
        try:
            cpu_load = get_cpu_load()
            gpu_load = get_gpu_load()
            gpu_mem = get_gpu_memory()
            ping = get_ping()

            # On Windows, HID requires a report ID as the first byte (0x00 for QMK RAW)
            data = bytearray(33)  # 1 byte report ID + 32 bytes data
            data[0] = 0x00  # Report ID
            data[1] = 0xF0  # Data packet identifier
            data[2] = min(cpu_load, 255)
            data[3] = min(gpu_load, 255)
            data[4] = (gpu_mem >> 8) & 0xFF
            data[5] = gpu_mem & 0xFF
            data[6] = (ping >> 8) & 0xFF
            data[7] = ping & 0xFF

            keyboard.write(data)
            print(f"CPU:{cpu_load:3d}% GPU:{gpu_load:3d}% VRAM:{gpu_mem:3d}% PING:{ping:3d}ms", end='\r')
            time.sleep(1.0)

        except KeyboardInterrupt:
            print("\n\nMonitoring stopped")
            break
        except Exception as e:
            print(f"\nError: {e}")
            time.sleep(1)

    keyboard.close()
    if NVIDIA_AVAILABLE:
        try:
            pynvml.nvmlShutdown()
        except:
            pass

if __name__ == "__main__":
    main()