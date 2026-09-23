@echo off
setlocal
cd /d "%~dp0"
where py >nul 2>nul
if %errorlevel%==0 (
  py -3 generate_experimental_vs_fit.py
) else (
  python generate_experimental_vs_fit.py
)
endlocal
