#!/usr/bin/env python3
"""
System Monitor for QMK TIDBIT Keyboard
Monitors CPU, GPU, FPS, and MS (latency) and writes to a file for keyboard display
"""

import psutil
import time
import os
import struct
import mmap

try:
    import pynvml
    NVIDIA_AVAILABLE = True
except ImportError:
    NVIDIA_AVAILABLE = False
    print("pynvml not available - GPU monitoring disabled")

try:
    import win32api
    import win32con
    WIN32_AVAILABLE = True
except ImportError:
    WIN32_AVAILABLE = False
    print("pywin32 not available - install with: pip install pywin32")

# Output file path
OUTPUT_FILE = os.path.join(os.path.dirname(__file__), "system_stats.txt")

def get_gpu_load():
    """Get GPU load percentage (NVIDIA only for now)"""
    if not NVIDIA_AVAILABLE:
        return 0

    try:
        pynvml.nvmlInit()
        handle = pynvml.nvmlDeviceGetHandleByIndex(0)
        utilization = pynvml.nvmlDeviceGetUtilizationRates(handle)
        return utilization.gpu
    except Exception as e:
        print(f"GPU monitoring error: {e}")
        return 0

def get_cpu_load():
    """Get CPU load percentage"""
    return int(psutil.cpu_percent(interval=0.1))

def get_fps():
    """
    Get FPS (frames per second) from MSI Afterburner shared memory
    Requires MSI Afterburner with RivaTuner Statistics Server running
    """
    if not WIN32_AVAILABLE:
        return 0

    try:
        # MSI Afterburner shared memory name
        # Try to open the shared memory
        try:
            handle = mmap.mmap(-1, 0x100000, "MAHMSharedMemory", access=mmap.ACCESS_READ)
        except FileNotFoundError:
            # MSI Afterburner not running
            return 0

        # Read header signature (should be 'MAHM')
        handle.seek(0)
        signature = handle.read(4)

        if signature != b'MAHM':
            handle.close()
            return 0

        # Read version and entry count
        handle.seek(0x10)
        entry_count = struct.unpack('I', handle.read(4))[0]

        # Search for framerate entry
        for i in range(min(entry_count, 128)):  # Limit to reasonable number
            offset = 0x18 + (i * 0x4E8)  # Each entry is 0x4E8 bytes
            handle.seek(offset)

            # Read source name (null-terminated string)
            src_name = handle.read(260).split(b'\x00')[0].decode('utf-8', errors='ignore')

            # Look for "Framerate" entry
            if 'Framerate' in src_name or 'FPS' in src_name:
                # Read the data value (float at offset +0x104 from entry start)
                handle.seek(offset + 0x104)
                fps_value = struct.unpack('f', handle.read(4))[0]
                handle.close()
                return int(fps_value)

        handle.close()
        return 0

    except Exception as e:
        print(f"FPS reading error: {e}")
        return 0

def get_ping():
    """
    Get network latency (ping in milliseconds)
    Pings Google DNS (8.8.8.8) to measure internet latency
    """
    import subprocess
    try:
        # Ping Google DNS once with 1 second timeout
        result = subprocess.run(
            ['ping', '-n', '1', '-w', '1000', '8.8.8.8'],
            capture_output=True,
            text=True,
            timeout=2
        )

        # Parse ping time from output
        # Example output: "Reply from 8.8.8.8: bytes=32 time=15ms TTL=118"
        if 'time=' in result.stdout:
            time_str = result.stdout.split('time=')[1].split('ms')[0]
            return int(time_str.strip())
        return 0
    except Exception as e:
        print(f"Ping error: {e}")
        return 0

def main():
    """Main monitoring loop"""
    print("System Monitor started...")
    print(f"Writing to: {OUTPUT_FILE}")

    while True:
        try:
            cpu_load = get_cpu_load()
            gpu_load = get_gpu_load()
            fps = get_fps()
            ping = get_ping()

            # Write stats to file in format: CPU:XX GPU:YY FPS:ZZZ PING:AA
            with open(OUTPUT_FILE, 'w') as f:
                f.write(f"CPU:{cpu_load:3d} GPU:{gpu_load:3d}\\n")
                f.write(f"FPS:{fps:3d} PING:{ping:3d}\\n")

            # Print to console for debugging
            print(f"CPU:{cpu_load:3d}% GPU:{gpu_load:3d}% FPS:{fps:3d} PING:{ping:3d}ms", end='\r')

            # Update every 1 second
            time.sleep(1.0)

        except KeyboardInterrupt:
            print("\\nMonitoring stopped by user")
            break
        except Exception as e:
            print(f"Error: {e}")
            time.sleep(1)

    # Cleanup
    if NVIDIA_AVAILABLE:
        try:
            pynvml.nvmlShutdown()
        except:
            pass

if __name__ == "__main__":
    main()
