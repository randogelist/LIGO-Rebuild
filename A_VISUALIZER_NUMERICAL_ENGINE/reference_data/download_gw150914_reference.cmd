@echo off
setlocal EnableExtensions
cd /d "%~dp0"
set "HURL=https://gwosc.org/GW150914data/P1500229/H1_reconstructions.txt"
set "LURL=https://gwosc.org/GW150914data/P1500229/L1_reconstructions.txt"
where curl.exe >nul 2>&1
if errorlevel 1 goto :powershell
curl.exe -L --fail --silent --show-error "%HURL%" -o H1_reconstructions.txt || exit /b 1
curl.exe -L --fail --silent --show-error "%LURL%" -o L1_reconstructions.txt || exit /b 1
echo DOWNLOAD=PASS
exit /b 0
:powershell
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -Command "$ProgressPreference='SilentlyContinue'; Invoke-WebRequest -UseBasicParsing -Uri '%HURL%' -OutFile 'H1_reconstructions.txt'; Invoke-WebRequest -UseBasicParsing -Uri '%LURL%' -OutFile 'L1_reconstructions.txt'"
if errorlevel 1 exit /b 1
echo DOWNLOAD=PASS
