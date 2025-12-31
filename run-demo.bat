@echo off
setlocal
cd /d "%~dp0"
set "CLIENTS=%~1"
if "%CLIENTS%"=="" set "CLIENTS=2"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\run-demo.ps1" -Clients %CLIENTS%
endlocal
