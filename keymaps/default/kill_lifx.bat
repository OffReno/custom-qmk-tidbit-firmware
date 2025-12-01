@echo off
taskkill /F /FI "WINDOWTITLE eq *lifx_control.py*" >nul 2>&1
taskkill /F /FI "COMMANDLINE eq *lifx_control.py*" >nul 2>&1
