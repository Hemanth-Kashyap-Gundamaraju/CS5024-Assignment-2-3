# Voyager 1 & 2 launch - CS5024 Assignment 2 (OpenGL)

Recreates the 1977 launches of **Voyager 2** (20 Aug) and **Voyager 1** (5 Sep) on a **Titan IIIE / Centaur D-1T**
from **Cape Canaveral LC-41**. C++17, OpenGL 3.3 core.

## Build (Ubuntu / WSL2)
    sudo apt install build-essential cmake libglfw3-dev libglew-dev libglm-dev libgl1-mesa-dev
    mkdir build && cd build && cmake .. && make
Run from the `build` folder:  `./a2-model-0`  `./a2-model-1`  `./a2-model-2`  `./a2-scene`
(`stb_image.h` is already in `third_party/`.)

## The four programs
| Program | What it shows | Keys |
|---|---|---|
| **a2-model-0** | Earth, atmosphere, stars, Sun, Moon | A/a, D/d spin Earth; arrows = camera X/Y; `[` `]` = camera Z; mouse drag = look; 1 space view, 2 low orbit, 3 ground at pad; F look at Earth; M look at Moon; V switch Sun (V1/V2 day) |
| **a2-model-1** | Launch pad (ground texture, concrete pad, launch table, umbilical tower, masts, tanks, service tower) | W/S/A/D/Q/E fly (Shift = fast); arrows, `[` `]`; mouse look; V show/hide rocket; B flames; 1/2 stripe colour |
| **a2-model-2** | Rocket + Voyager, detachable parts | mouse drag orbit, wheel zoom; **N** release next part (boosters, stage 1, shroud, stage 2, Centaur, kick motor); O open/close Voyager booms; B flames; H whole rocket; V Voyager close-up; 1/2 mission; R reset |
| **a2-scene** | Everything together + launch | SPACE start/pause; **1** Earth camera, **2** launch-site camera, **3** vehicle camera; M switch Voyager 1 <-> 2; R restart; `,` `.` time speed; W auto time-speed; Earth cam: arrows/`[` `]`/mouse, F; vehicle cam: `[` `]`/wheel zoom, mouse orbit |

The window title shows mission time, altitude, speed, time-speed and the last event. Events are also printed in the terminal.

## Files (src/common = shared code)
config.h (all constants) | frames.h (Earth spin, launch site, Sun, Moon maths) | vehicle_specs.h (masses, thrust, timeline - edit here)
flight_sim.h (gravity, thrust, drag, stage events; no OpenGL) | rocket.h (rocket + Voyager model, separation) | launch_pad.h
earth.h (stars, Moon, Earth, clouds, atmosphere) | renderer.h, shaders.h, mesh.h, camera.h, gl_utils.h, app_window.h, procedural_textures.h

## How the requirements are covered
* **Earth/atmosphere/space**: textured sphere, ray-marched atmosphere shader (works from space and from the ground), star shader, Sun disc, Moon.
* **Spin vs gravity/drag vs space**: Earth rotates (correct spin rate, correct angle for each launch time so the Sun matches); the rocket starts with Earth's rotation speed;
  air drag follows an exponential atmosphere and is exactly 0 above ~150 km, so the stack only feels gravity and engines in space.
  Dropped stages fall with gravity + drag too.
* **Position**: pad at 28.5836 N, 80.5828 W; lighting uses the Sun direction for the real launch date/time (UTC).
* **Cameras**: Earth camera (far, free), launch-site camera (tracks the vehicle and zooms), vehicle camera (rides with the rocket; switches to Voyager close-up after the shroud/stages are gone).
* **Two-pass rendering**: planets and rocket need very different near/far planes, so space is drawn first, depth is cleared, then pad + rocket are drawn.
  All positions are `double` and made camera-relative before drawing, so nothing jitters.
* **Blender**: not used. Every model is generated in code (revolved profiles, boxes, cylinders).

## Textures and credits (fill in for your report)
The program runs with built-in generated stand-in textures. For a better look put these in `assets/` (names are exact):
`earth_day.jpg` (equirectangular), `clouds.jpg` (optional), `moon.jpg`, `stars.jpg` (optional), `ground.jpg`, `concrete.jpg`.
Good free sources: Solar System Scope textures, NASA Visible Earth / Blue Marble. Write down where each came from.
Libraries: GLFW, GLEW, GLM, stb_image (public domain). Mention any AI/LLM help in the originality declaration.

## Honest notes
* Masses, thrusts, burn times and event times in `vehicle_specs.h` are **rounded approximations tuned so the simulation reaches orbit**, not official data. Check them against references before writing them in the report.
* The two missions use the same rocket; only the date, launch time (Sun position) and the stripe colour differ. Check the launch times and the launch azimuth (assumed due east) against a source.
* Launch-site and rocket shapes are simplified.
* Not included (Assignment 3): Bezier editor, keyframes, trajectory.txt / keyframes.txt.

## Ideas for bonus marks
Smoke cloud at liftoff, real ground texture of Florida, clouds, night-side city lights, shadows, separated stages with tumbling (already basic).
