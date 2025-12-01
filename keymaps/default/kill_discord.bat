@echo off
REM Kill all pythonw.exe processes (will stop Discord bot)
taskkill /F /IM pythonw.exe >nul 2>&1
exit
