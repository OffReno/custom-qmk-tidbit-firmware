@echo off
REM Kill all pythonw.exe processes (background Python processes)
taskkill /F /IM pythonw.exe >nul 2>&1
exit
