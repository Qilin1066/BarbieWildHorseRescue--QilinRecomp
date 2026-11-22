@echo off
rem Play Barbie Horse Adventures: Wild Horse Rescue: double-click. The first time it builds the
rem game from your disc (5 to 20 minutes); after that it just starts it.
setlocal
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "kit\play.ps1" %*
if errorlevel 1 (
    echo.
    echo Something stopped the build: the red line above says what.
    pause
)
