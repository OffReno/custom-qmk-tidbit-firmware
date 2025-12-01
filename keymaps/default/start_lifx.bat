@echo off
cd /d "C:\Users\Renobatio\qmk_firmware\keyboards\nullbitsco\tidbit" >nul 2>&1
start /min cmd /c ".venv\Scripts\python.exe -u keymaps\default\lifx_control.py"
