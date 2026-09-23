BKQR GR OPTICAL DYNAMICS DX12 v1.6.6.2

RUN
---
Normal:     run.cmd
Fast:       run.cmd fast

The normal runner compiles and executes portable physics/conservation/static audits before opening the DX12 renderer. The fast runner keeps the callback/safe-submission audit and then builds/opens the renderer.

PHYSICAL MODEL
--------------
Units: G=c=1.
Inputs: M1, M2, total Jz, optional aligned S1z/S2z.
Default equal-mass state is the 3PN circular-J solution at r0=12 M.

Inspiral:
  * gauge-invariant 3PN circular binding energy E(x), x=(M|Omega|)^(2/3)
  * 3PN circular angular-momentum map Jorb(x), used to invert Jtot-S1z-S2z -> x0
  * nonspinning point-mass 3.5PN gravitational-wave luminosity F(x)
  * TaylorT1-style balance dx/dt=-F/(dE/dx), dphi/dt=sign(L)x^(3/2)/M
  * angular-momentum loss dJrad/dt=sign(L) F/|Omega|

Strong field / merger:
  * transition before the 3PN minimum-energy circular orbit (and no deeper than the user PN safety radius)
  * no artificial separation floor and no perpetual floor orbit
  * final mass and aligned final spin from the Jimenez-Forteza et al. 2017 NR fit used in LALSuite/PhenomX
  * smooth finite-energy/final-J closure to the remnant
  * dominant Kerr (2,2,0) QNM fit sets the ringdown frequency/damping scale

OPTICS
------
Before merger: retarded moving two-puncture isotropic optical ansatz.
During merger: smoothly blends the two-hole field into a single central remnant field.
After merger: exact static Schwarzschild isotropic optical field outside the horizon, using the current/final remnant mass.
The orbital inset emits expanding GW-power telemetry rings. Those rings are visual diagnostics; they do not push the rays or feed back into the dynamics.

IMPORTANT CLAIM BOUNDARY
------------------------
This is a reduced-order PN + numerical-relativity-calibrated model. It is NOT a numerical solution of the 3+1 Einstein PDEs through common-horizon formation (BSSN/Z4c/generalized-harmonic evolution).
The pre-merger two-puncture optical field is an ansatz, not a solved binary spacetime.
The post-merger optical field is currently Schwarzschild; remnant spin is tracked dynamically but Kerr frame dragging is not yet included in ray propagation.
Aligned spin is used in the final-state fit. The inspiral E(x)/F(x) currently uses the nonspinning point-mass PN coefficients, so spin-orbit/spin-spin inspiral phasing is a future upgrade.

VIEW CLOCK
----------
Default wall-clock playback is phase-normalized to 1.00 Hz:
  dt_sim/dt_wall = 2*pi*f_view / |Omega_characteristic|.
During inspiral Omega_characteristic is the orbital rate. During merger/ringdown it smoothly approaches half the dominant GW QNM angular frequency. This changes only playback speed, not the physical trajectory.

Environment:
  BKQR_M1
  BKQR_M2
  BKQR_JTOT
  BKQR_S1Z
  BKQR_S2Z
  BKQR_VIEW_ORBIT_HZ      default 1.0
  BKQR_PHASE_NORMALIZED  default 1
  BKQR_TIME_RATE          physical M/s fallback
  BKQR_FPS_CAP            default 90, clamped 30..300
  BKQR_SAFE_GPU_SERIAL    default 1

STABILITY
---------
Dedicated DX12 child surface: 960x540.
Control-panel refresh: 20 Hz.
GPU fence timeout: 5000 ms.
Recoverable failures print diagnostic stage/frame and DXGI removal reason when available.


SOURCE / DETECTOR MODES
-----------------------
New render modes 4 and 5 (keyboard 4/5, or the Render mode slider) turn the image itself into a detector screen with the binary between source and detector along the x-axis.

  * Detector: point source
      - detector plane at x=+D_det
      - point source on the source plane x=-D_src
      - each detector pixel is traced backward through the binary toward the source plane
      - brightness is the source intensity sampled at the back-traced hit point

  * Detector: cylindrical beam
      - detector plane at x=+D_det
      - incoming beam is a parallel cylindrical bundle traveling from x=-D_src toward +x
      - the detector records which detector pixels trace back into the emitting cylinder

Source/detector controls:
  Detector distance
  Source distance
  Detector half-height
  Point-source size
  Beam radius
  Beam softness

These modes are intended specifically to show how the binary distorts transport between a source and a detector. They use optical reciprocity for efficiency: the GPU traces from the detector back to the source plane, which is equivalent for the static detector readout.


GLOBAL RESTART
--------------
A permanent GLOBAL RESTART button is pinned to the top of the control deck.
F5 calls the same restart path.
The global restart restores package defaults for:
  simulation time / pause / view clock
  binary masses, J, spins, phase and radiation-reaction state
  inspiral-merger-ringdown dynamical state
  camera
  source/detector render mode and source/beam parameters
  optical controls
  ray-quality controls
The button is fixed in the header and stays visible while the slider list is scrolled.


MIRRORED SOURCE / DETECTOR DEFAULT
----------------------------------
Startup now opens directly in the detector-view point-source mode. The rendered image is the detector screen itself, not a background sky.

Detector geometry:
  * detector plane is centered at the camera position
  * detector center pixel is the 2D centerpoint aligned with the binary barycenter
  * detector plane uses the camera right/up basis

Source geometry:
  * in detector modes the source is mirrored through the barycenter:
        x_source = - x_camera
  * so the source sits at the same distance on the opposite side of the system
  * point-source mode launches from this mirrored source point
  * cylindrical-beam mode uses a parallel beam along the same mirrored camera-source axis

Background handling:
  * background stars and textured sky are removed
  * detector pixels with no incoming source contribution stay black

Controls retained for detector work:
  Detector half-height
  Point-source size
  Beam radius
  Beam softness


LEFT CAMERA BAR
---------------
The UI is now split into two side bars:
  * left bar: dedicated camera controls (radius, azimuth, inclination, field of view, exposure)
  * right bar: simulation, physics, optics, detector, and ray-quality controls

This keeps camera movement separate from physical inputs during detector experiments.


GLOBAL RESTART PAUSE
--------------------
Every global restart (button, F5, or the shared global restart path) resets the entire simulation and leaves it PAUSED at t=0. Press Space or use the Paused control to resume. Normal first launch behavior is unchanged.


CAMERA CONTROL BEHAVIOR
-----------------------
The dedicated left camera bar now uses strict knob/track hit testing. Grab a knob and drag it; the value no longer jumps because of clicks on the label/value area.

In mirrored detector mode the detector aperture is now proportional to
  |camera position| * tan(FOV/2)
with the detector-size control acting as an additional scale factor. Thus all five camera controls have a direct visible role.


CENTER-LOCKED CAMERA
--------------------
The left camera bar now exposes only the spherical center-locked coordinates of the observer:
  * R     : observer radius from the binary barycenter
  * theta : polar angle
  * phi   : azimuthal angle

The camera always looks at the center (the binary barycenter / remnant center). Field of view and exposure remain available on the right control deck under VIEW / DISPLAY.


NATIVE CAMERA KNOBS
-------------------
The left camera bar no longer uses the custom painted slider widget. R, theta and phi are native Win32 trackbar controls with real trackbar thumbs/knobs. They are synchronized with keyboard camera motion and global restart. The camera remains center-locked: its forward direction is always toward the barycenter at (0,0,0).


MATCHED LEFT / RIGHT SLIDER DECKS
---------------------------------
The left camera bar and right control deck now use the exact same custom ControlPanel slider implementation.

Left deck:
  R
  theta
  phi

Both decks now share the same:
  * 360 px panel width
  * knob geometry and track drawing
  * hover highlighting
  * track/knob hit testing
  * grab-offset-preserving dragging
  * label/value typography
  * double-click default reset

The previous separate native Win32 camera-trackbar implementation has been removed. The camera remains center-locked.


v1.6.6.2 compile fix
--------------------
Removed obsolete Win32 common-controls initialization from the earlier native-camera prototype. Left and right sliders both use the custom ControlPanel implementation; no InitCommonControlsEx dependency remains.


GLOBAL PLAY / RESTART
---------------------
The right control deck now has two side-by-side global action buttons:
  * GLOBAL PLAY: sets paused=false and resumes the current state without resetting camera, binary dynamics, optics, or detector settings.
  * GLOBAL RESTART: resets the full state to defaults at t=0 and intentionally leaves the simulation paused.

This makes restart-and-inspect then play a two-button workflow.


MIRRORED SOURCE SPHERE
----------------------
A finite luminous source sphere can now be shown at the mirrored source location x_source = -x_camera.

Controls:
  Show mirrored source sphere   : toggles the source sphere on/off
  Source sphere radius          : sets the emitting sphere radius

Behavior:
  * In detector modes, the detector now receives light from the mirrored source sphere when enabled.
  * In sky mode, the same mirrored source sphere is also rendered as a finite luminous object behind the binary, so lensing can be viewed directly with no star background.


GLOBAL RESTART SEMANTICS
------------------------
GLOBAL RESTART now rewinds the current experiment rather than restoring package defaults.
It preserves all current parameters, including:
  * camera R/theta/phi
  * field of view and exposure
  * masses, total angular momentum, spins, phase, radiation-reaction setting
  * render/source/detector settings and mirrored source sphere controls
  * ray-quality settings
  * view-clock rate/mode

It resets only the dynamical trajectory/history and simulation time to t=0, then leaves the simulation PAUSED. GLOBAL PLAY resumes from that restarted state.


ON / OFF TOGGLES
----------------
Boolean settings are now rendered as dedicated ON/OFF switches rather than two-position sliders.

Converted controls:
  * Paused
  * Time normalization
  * Radiation reaction
  * Orbital-plane inset
  * Show mirrored source sphere

Time normalization:
  ON  = phase-normalized playback at the selected visual orbital rate
  OFF = no phase normalization; playback uses the Physical time rate (M/s) directly

GLOBAL RESTART preserves all of these current toggle states while rewinding the dynamical evolution to t=0 and pausing it.


DIRECT INITIAL SEPARATION INPUT
-------------------------------
The physical controls now include `Initial separation r0 / M`, the center-to-center distance between the two compact objects in units of the total mass M=m1+m2.

This is not an independent parameter from total angular momentum J for the current quasicircular model. Changing r0/M recomputes the compatible 3PN circular orbital angular momentum and updates J. Changing J still updates the derived r0/M readout.

Default: r0/M = 12.
Supported UI range: 6 .. 60.


MOVING OBJECT RADII
-------------------
The physical-input deck now includes independent Object 1 radius and Object 2 radius controls.

These radii are optical/absorbing surfaces in units of total mass M. A ray entering the selected radius of a pre-merger moving object is counted as captured by that object. The orbital inset also uses the selected radii for the visible object sizes.

Important: changing these radii does NOT change m1, m2, the PN gravitational field, or the orbital evolution. They are deliberately separated from mass so a visualization radius cannot silently rewrite the GR dynamics. Default equal-mass radii are 0.25 M each, matching the isotropic-coordinate Schwarzschild horizon radius m_i/2 for m_i=0.5 M.


TIME-EVOLVING OBJECT RADII
--------------------------
The two moving-object radii are no longer manual inputs. They are derived at each time from the same optical capture model used by the ray tracer.

For object i with companion j, current separation d, merger blend b, and capture threshold u_c:
  u_companion = (1-b) m_j / (2 d)
  R_i,local = m_i / [2 (u_c - u_companion)]
  R_i,eff = R_i,local sqrt(1-b)

Interpretation:
  * during inspiral the companion field raises the local optical potential and the effective individual capture radius changes with separation;
  * during merger the two individual radii fade as the common remnant field takes over;
  * the remnant optical radius is M_system / (2 u_c).

This is an effective radius inside the current two-puncture optical ansatz, not an exact apparent-horizon radius from a full 3+1 Einstein evolution.


INDUCED GW SIGNAL PANEL
-----------------------
This version adds a lower-center waveform panel synchronized to the binary dynamics. The panel shows the far-observer gravitational-wave response computed from the same reduced inspiral-merger-ringdown state that drives the optical scene. The plotted channels are h+, hx, and a projected detector response h_obs = F+ h+ + Fx hx, with observer inclination, detector polarization angle, observer distance, and waveform gain exposed as live controls. A constant propagation delay is omitted, so the panel shows retarded-time structure aligned for visual interpretation.


GW150914 DEFAULT PRESET
-----------------------
run.cmd enables a GW150914/LIGO central source-frame mass-ratio preset by default: normalized m1=0.5356037151702787, m2=0.4643962848297214, zero component spins, and the corresponding 3PN circular angular momentum at r0/M=12. Set BKQR_USE_LIGO_PRESET=0 to disable it.
