@echo off
setlocal
if "%~1"=="" (
  echo Usage: upload_xiao.bat COM5
  exit /b 2
)

set FQBN=Seeeduino:nrf52:xiaonRF52840
set PORT=%~1

echo RejsaRubberTrac XIAO nRF52840 upload
echo Target: %FQBN%
echo Port:   %PORT%
..\arduino-cli.exe upload -p %PORT% -b %FQBN% main
exit /b %errorlevel%
