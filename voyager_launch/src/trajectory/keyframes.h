#pragma once
#include "bezier.h"
#include <string>
#include <vector>

namespace trajectory {
struct Keyframe {
    double time = 0.0;
    int active = 1;       // Discrete: copied from the preceding keyframe until the next boundary.
    double parameter = 0.0;
    std::vector<int> active_flags; // optional activity/parameter pairs for component trajectories
    std::vector<double> parameters;
    Point eye{0.0, 0.0, 160.0};
    Point target{0.0, 0.0, 0.0};
};
struct KeyframeState {
    int active = 0;
    double parameter = 0.0;
    std::vector<int> active_flags; // optional activity/parameter pairs for component trajectories
    std::vector<double> parameters;
    Point eye{0.0, 0.0, 160.0};
    Point target{0.0, 0.0, 0.0};
};
bool load_keyframes(const std::string& filename, const std::string& mission,
                    std::vector<Keyframe>& frames, std::string& error);
KeyframeState evaluate_keyframes(const std::vector<Keyframe>& frames, double time);
}
