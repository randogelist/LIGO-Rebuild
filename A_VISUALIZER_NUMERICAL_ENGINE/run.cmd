@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"
set "VSCMD_SKIP_SENDTELEMETRY=1"
set "EXE=BKQR_GR_OPTICAL_DYNAMICS_DX12_v1_6_6_2.exe"
echo ============================================================
echo BKQR GR OPTICAL DYNAMICS DX12 v1.6.6.2
echo   INPUT: M1, M2, Jtot, optional aligned spins
echo   DYNAMICS: 3PN binding + 3.5PN GW flux energy balance
echo   MERGER: NR-calibrated final mass/spin + Kerr 220 ringdown scale
echo   VISUAL: retarded two-hole optics -^> single remnant + GW power telemetry
echo   WAVE PANEL: induced h+, hx, and projected far-observer strain h_obs
echo   UI: GLOBAL RESTART button + F5 reset all simulation/view/optical state
echo   VIEW CLOCK: phase-normalized to ~1.00 characteristic revolution / wall-second by default
echo   CLAIM: PN/NR-calibrated reduced model; not a BSSN/Z4c field evolution
echo ============================================================
if not defined BKQR_USE_LIGO_PRESET set "BKQR_USE_LIGO_PRESET=1"
if /I "%BKQR_USE_LIGO_PRESET%"=="1" (
  if not defined BKQR_M1 set "BKQR_M1=0.5356037151702787"
  if not defined BKQR_M2 set "BKQR_M2=0.4643962848297214"
  if not defined BKQR_JTOT set "BKQR_JTOT=0.9893951715474206"
  if not defined BKQR_S1Z set "BKQR_S1Z=0"
  if not defined BKQR_S2Z set "BKQR_S2Z=0"
  if not defined BKQR_VIEW_ORBIT_HZ set "BKQR_VIEW_ORBIT_HZ=1.0"
  if not defined BKQR_PHASE_NORMALIZED set "BKQR_PHASE_NORMALIZED=1"
  echo PRESET=GW150914_LIGO_SOURCE_FRAME_CENTRAL_VALUES
  echo PRESET_M1_NORMALIZED=%BKQR_M1%
  echo PRESET_M2_NORMALIZED=%BKQR_M2%
  echo PRESET_JTOT_R0_12M=%BKQR_JTOT%
  echo PRESET_NOTE=Mass ratio seeded from LIGO/GWOSC GW150914 central source-frame masses; dynamics normalized to total M=1.
) else (
  echo PRESET=USER_DEFINED_OR_DISABLED
)
where cl.exe >nul 2>&1
if not errorlevel 1 goto :MSVC_READY
set "VSDEV="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do if exist "%%I\Common7\Tools\VsDevCmd.bat" set "VSDEV=%%I\Common7\Tools\VsDevCmd.bat"
if not defined VSDEV if exist "C:\BuildTools\Common7\Tools\VsDevCmd.bat" set "VSDEV=C:\BuildTools\Common7\Tools\VsDevCmd.bat"
if not defined VSDEV if exist "C:\Program Files\Microsoft Visual Studio\18\BuildTools\Common7\Tools\VsDevCmd.bat" set "VSDEV=C:\Program Files\Microsoft Visual Studio\18\BuildTools\Common7\Tools\VsDevCmd.bat"
if not defined VSDEV if exist "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" set "VSDEV=C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"
if not defined VSDEV echo MSVC_DISCOVERY=FAIL& exit /b 10
call "%VSDEV%" -arch=x64 -host_arch=x64 >nul 2>&1
if errorlevel 1 echo MSVC_ENVIRONMENT=FAIL& exit /b 11
:MSVC_READY
echo MSVC_ENVIRONMENT=PASS

if /I "%~1"=="fast" goto :PRE_RENDER_AUDIT

echo [1/10] M1 M2 JTOT PN/NR DERIVATION
pushd tests
cl.exe /nologo /std:c++20 /EHsc /O2 /MT test_binary_driver.cpp /Fe:test_binary_driver.exe
if errorlevel 1 (echo DRIVER_COMPILE=FAIL&popd&exit /b 20)
test_binary_driver.exe
if errorlevel 1 (echo DRIVER_RUN=FAIL&popd&exit /b 21)
echo DRIVER_RUN=PASS
popd

echo [2/10] TIME-EVOLUTION / COM REGRESSION
pushd tests
cl.exe /nologo /std:c++20 /EHsc /O2 /MT test_optical_driver.cpp /Fe:test_optical_driver.exe
if errorlevel 1 (echo EVOLUTION_COMPILE=FAIL&popd&exit /b 22)
test_optical_driver.exe
if errorlevel 1 (echo EVOLUTION_RUN=FAIL&popd&exit /b 23)
echo EVOLUTION_RUN=PASS
popd

echo [3/10] 3.5PN ENERGY + ANGULAR-MOMENTUM BALANCE
pushd tests
cl.exe /nologo /std:c++20 /EHsc /O2 /MT test_energy_balance.cpp /Fe:test_energy_balance.exe
if errorlevel 1 (echo BALANCE_COMPILE=FAIL&popd&exit /b 40)
test_energy_balance.exe
if errorlevel 1 (echo BALANCE_RUN=FAIL&popd&exit /b 41)
echo BALANCE_RUN=PASS
popd

echo [4/10] MERGER / REMNANT CONSERVATION
pushd tests
cl.exe /nologo /std:c++20 /EHsc /O2 /MT test_merger.cpp /Fe:test_merger.exe
if errorlevel 1 (echo MERGER_COMPILE=FAIL&popd&exit /b 42)
test_merger.exe
if errorlevel 1 (echo MERGER_RUN=FAIL&popd&exit /b 43)
echo MERGER_RUN=PASS
popd

echo [5/10] 1 HZ PHASE-NORMALIZED VIEW CLOCK
pushd tests
cl.exe /nologo /std:c++20 /EHsc /O2 /MT test_view_clock.cpp /Fe:test_view_clock.exe
if errorlevel 1 (echo VIEW_CLOCK_COMPILE=FAIL&popd&exit /b 35)
test_view_clock.exe
if errorlevel 1 (echo VIEW_CLOCK_RUN=FAIL&popd&exit /b 36)
cl.exe /nologo /std:c++20 /EHsc /O2 /MT test_view_clock_static.cpp /Fe:test_view_clock_static.exe
if errorlevel 1 (echo VIEW_CLOCK_STATIC_COMPILE=FAIL&popd&exit /b 37)
test_view_clock_static.exe
if errorlevel 1 (echo VIEW_CLOCK_STATIC_RUN=FAIL&popd&exit /b 38)
echo VIEW_CLOCK_AUDIT_RUN=PASS
popd

echo [6/10] PACKET + SHADER CONTRACT
pushd tests
cl.exe /nologo /std:c++20 /EHsc /O2 /MT test_protocol.cpp /Fe:test_protocol.exe
if errorlevel 1 (echo PROTOCOL_COMPILE=FAIL&popd&exit /b 24)
test_protocol.exe
if errorlevel 1 (echo PROTOCOL_RUN=FAIL&popd&exit /b 25)
cl.exe /nologo /std:c++20 /EHsc /O2 /MT test_shader_static.cpp /Fe:test_shader_static.exe
if errorlevel 1 (echo SHADER_AUDIT_COMPILE=FAIL&popd&exit /b 26)
test_shader_static.exe
if errorlevel 1 (echo SHADER_AUDIT_RUN=FAIL&popd&exit /b 27)
echo CONTRACT_AUDITS=PASS
popd

echo [7/10] SNAPSHOT / PHYSICAL-INPUT OWNERSHIP AUDIT
pushd tests
cl.exe /nologo /std:c++20 /EHsc /O2 /MT test_snapshot_ownership.cpp /Fe:test_snapshot_ownership.exe
if errorlevel 1 (echo OWNERSHIP_AUDIT_COMPILE=FAIL&popd&exit /b 28)
test_snapshot_ownership.exe
if errorlevel 1 (echo OWNERSHIP_AUDIT_RUN=FAIL&popd&exit /b 29)
echo OWNERSHIP_AUDIT_RUN=PASS
popd

echo [8/10] STABILITY / WINDOW-ISOLATION AUDIT
pushd tests
cl.exe /nologo /std:c++20 /EHsc /O2 /MT test_stability_static.cpp /Fe:test_stability_static.exe
if errorlevel 1 (echo STABILITY_AUDIT_COMPILE=FAIL&popd&exit /b 30)
test_stability_static.exe
if errorlevel 1 (echo STABILITY_AUDIT_RUN=FAIL&popd&exit /b 31)
echo STABILITY_AUDIT_RUN=PASS
popd

:PRE_RENDER_AUDIT
echo [9/10] STORED-CALLBACK LIFETIME / SAFE-SUBMISSION AUDIT
pushd tests
cl.exe /nologo /std:c++20 /EHsc /O2 /MT test_callback_lifetime_static.cpp /Fe:test_callback_lifetime_static.exe
if errorlevel 1 (echo CALLBACK_AUDIT_COMPILE=FAIL&popd&exit /b 33)
test_callback_lifetime_static.exe
if errorlevel 1 (echo CALLBACK_AUDIT_RUN=FAIL&popd&exit /b 34)
echo CALLBACK_AUDIT_RUN=PASS
popd

echo [10/10] BUILD + OPEN INTERACTIVE OPTICAL DYNAMICS
pushd renderer
if not exist "%EXE%" (
  cl.exe /nologo /std:c++20 /O2 /GL /Gw /Gy /EHsc /MT /DNDEBUG /DUNICODE /D_UNICODE /DNOMINMAX main.cpp /Fe:"%EXE%" /link /LTCG /OPT:REF /OPT:ICF d3d12.lib dxgi.lib d3dcompiler.lib user32.lib gdi32.lib ole32.lib
  if errorlevel 1 (echo RENDERER_COMPILE=FAIL&popd&exit /b 32)
)
echo RENDERER_COMPILE=PASS
echo BACKEND=3PN_3P5PN_NR_CALIBRATED_OPTICAL_DYNAMICS
echo INPUT_ENV=BKQR_M1,BKQR_M2,BKQR_JTOT,BKQR_S1Z,BKQR_S2Z,BKQR_VIEW_ORBIT_HZ,BKQR_PHASE_NORMALIZED,BKQR_TIME_RATE
echo RENDERER_LAUNCH=BEGIN
"%EXE%"
set "RC=!ERRORLEVEL!"
if not "!RC!"=="0" (echo RENDERER_RUN=FAIL EXIT_CODE=!RC!&popd&exit /b !RC!)
echo RENDERER_RUN=PASS
popd
echo ============================================================
echo RUN=PASS
echo ============================================================
exit /b 0
