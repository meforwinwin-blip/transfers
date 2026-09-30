@echo off
setlocal
set FQBN=Seeeduino:nrf52:xiaonRF52840

echo RejsaRubberTrac XIAO nRF52840 build
echo Target: %FQBN%
..\arduino-cli.exe compile -b %FQBN% main
exit /b %errorlevel%
