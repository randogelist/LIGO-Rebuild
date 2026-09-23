@echo off
setlocal EnableExtensions
cd /d "%~dp0"
rem GW150914 / GWOSC GWTC-2.1 central source-frame mass ratio.
rem Dynamics are normalized to M1+M2=1; this preserves q and eta.
set "BKQR_USE_LIGO_PRESET=1"
set "BKQR_M1=0.5356037151702787"
set "BKQR_M2=0.4643962848297214"
set "BKQR_JTOT=0.9893951715474206"
set "BKQR_S1Z=0"
set "BKQR_S2Z=0"
set "BKQR_VIEW_ORBIT_HZ=1.0"
set "BKQR_PHASE_NORMALIZED=1"
echo PRESET=GW150914_GWOSC_GWTC2P1
call run.cmd %*
exit /b %ERRORLEVEL%
