@echo off
cd /d "C:\Users\Renobatio\qmk_firmware\keyboards\nullbitsco\tidbit" >nul 2>&1
start /B .venv\Scripts\pythonw.exe keymaps\default\lifx_control.py
