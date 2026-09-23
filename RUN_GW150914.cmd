@echo off
setlocal EnableExtensions
cd /d "%~dp0A_VISUALIZER_NUMERICAL_ENGINE"
call run_gw150914.cmd %*
exit /b %ERRORLEVEL%
