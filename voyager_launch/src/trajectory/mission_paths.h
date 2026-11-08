#pragma once
#include "bezier.h"
#include <string>
#include <vector>

namespace trajectory {
struct MissionPath {
    std::string name;
    std::vector<Point> control_points;
};
MissionPath voyager_one_path();
MissionPath voyager_two_path();
}
