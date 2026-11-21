@echo off
rem Analyse game: double-click. Builds the qilin tool (a few minutes the
rem first time) and reads your copy of Barbie Horse Adventures: Wild Horse
rem Rescue with it. The results go to analysis-out\ in this folder, which is
rem never committed.
setlocal
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "tools\analyse-game.ps1" %*
if errorlevel 1 (
    echo.
    echo It stopped. The message above says where; copy it for Claude.
)
pause
