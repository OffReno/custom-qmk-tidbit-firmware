@echo off
taskkill /F /IM pythonw.exe >nul 2>&1
taskkill /F /IM python.exe /FI "WINDOWTITLE eq System Monitor*" >nul 2>&1
exit