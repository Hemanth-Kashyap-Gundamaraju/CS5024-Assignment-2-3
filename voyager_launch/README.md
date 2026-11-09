# Voyager Mission Visualisation

A C++17 and OpenGL project visualising the Voyager 1 and Voyager 2 space missions.

## Features
- 3D spacecraft and planetary scene rendering
- Interactive Bézier trajectory editor with De Casteljau visualisation
- Keyframe-based mission playback
- Schematic planetary flybys and procedural planetary textures

## Build
    cmake -S . -B build
    cmake --build build -j$(nproc)

## Run
    ./build/a3-trajectory

Mission trajectories and planetary representations are illustrative, not to scale.
