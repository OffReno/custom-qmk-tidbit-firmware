@echo off
cd /d "C:\Users\Renobatio\qmk_firmware\keyboards\nullbitsco\tidbit" >nul 2>&1
start "" ".venv\Scripts\pythonw.exe" "keymaps\default\system_monitor_hid.py"
exit