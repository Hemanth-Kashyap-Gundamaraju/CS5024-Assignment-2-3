# Assignment 3: implementation and commit plan

This patch adds a separate `a3-trajectory` executable and reusable trajectory modules.
It deliberately leaves the Assignment 2 applications untouched.

## Commit 1 — Bézier mathematics
```bash
git add voyager_launch/src/trajectory/bezier.h \
        voyager_launch/src/trajectory/bezier.cpp
git commit -m "feat(a3): add Bernstein and De Casteljau Bezier evaluation"
```
`evaluate_bernstein` evaluates the Bernstein polynomial. `de_casteljau_levels`
returns every interpolation level, so the final item is B(t) and earlier
levels can be drawn to demonstrate the algorithm. `sample_curve` generates
a polyline for rendering.

## Commit 2 — mathematical tests
```bash
git add voyager_launch/tests/bezier_tests.cpp
git commit -m "test(a3): add Bezier evaluation tests"
```
Tests endpoints, a linear curve, agreement between both algorithms, the
number of De Casteljau levels, and sample count.

Build/run from the CMake project directory:
```bash
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc/trajectory \
    src/trajectory/bezier.cpp tests/bezier_tests.cpp -o bezier_tests
./bezier_tests
```

## Commit 3 — trajectory persistence
```bash
git add voyager_launch/src/trajectory/trajectory_io.h \
        voyager_launch/src/trajectory/trajectory_io.cpp
git commit -m "feat(a3): save and load trajectory control points"
```
`T/t` appends a point-count block and xyz rows to `trajectory.txt`.
`Y/y` loads every block and displays saved curves in different colours.

## Commit 4 — keyframe model and parser
```bash
git add voyager_launch/src/trajectory/keyframes.h \
        voyager_launch/src/trajectory/keyframes.cpp \
        voyager_launch/keyframes.txt
git commit -m "feat(a3): parse and interpolate mission keyframes"
```
Rows contain time, activity, curve parameter, camera eye, and camera target.
The activity flag is discrete; the other values are interpolated linearly.

## Commit 5 — mission paths
```bash
git add voyager_launch/src/trajectory/mission_paths.h \
        voyager_launch/src/trajectory/mission_paths.cpp
git commit -m "feat(a3): add Voyager 1 and Voyager 2 teaching paths"
```
These are stylised Bézier paths for visualisation, not precise ephemerides.

## Commit 6 — interactive editor and playback
```bash
git add voyager_launch/src/apps/a3_trajectory.cpp
git commit -m "feat(a3): add interactive 3D trajectory editor and playback"
```
Controls:
- `1` / `2`: choose Voyager 1 / Voyager 2.
- Left-click: select a control point; drag to move it in the XY plane.
- Right-click: append a control point.
- `W A S D`: move the selected point in XY; `Q/E`: move it in Z.
- Arrow keys: pan the camera; `+/-`: zoom.
- `Delete`: remove the selected point (at least two points remain).
- `T`: append the current curve to `trajectory.txt`.
- `Y`: load saved curves from `trajectory.txt`.
- `P`: start/pause keyframe playback; `R`: reset playback.

## Commit 7 — CMake integration
Add the following after the existing `add_app` calls in CMakeLists.txt:
```cmake
add_executable(a3-trajectory
  src/apps/a3_trajectory.cpp
  src/trajectory/bezier.cpp
  src/trajectory/trajectory_io.cpp
  src/trajectory/keyframes.cpp
  src/trajectory/mission_paths.cpp
)
target_include_directories(a3-trajectory PRIVATE
  src/common src/trajectory third_party ${GLM_INCLUDE_DIR}
)
target_compile_definitions(a3-trajectory PRIVATE ASSET_DIR="${ASSET_DIR}")
target_link_libraries(a3-trajectory PRIVATE glfw ${GLEW_LIBRARIES} OpenGL::GL)
```
Then:
```bash
git add voyager_launch/CMakeLists.txt
git commit -m "build(a3): add trajectory editor target"
```

## Build the complete project
From `voyager_launch/voyager_launch`:
```bash
cmake -S . -B build
cmake --build build -j
./build/a3-trajectory
```
`keyframes.txt` is read relative to the current working directory, so run
the executable from the project root as shown.

## Notes / scope
- The mission paths are pedagogical curves, not accurate spacecraft ephemerides.
- The editor uses an XY editing plane; Z can be adjusted with Q/E.
- The existing Assignment 2 scene is preserved. A later integration commit
  can use the same Bézier evaluator and keyframe system to drive its rocket
  and camera transforms.
- The De Casteljau levels are available in the math API. The starter renderer
  draws the control polygon and curve; adding a selectable `t` slider and
  rendering every intermediate level is a suitable visualisation extension.
