#pragma once
#include "bezier.h"
#include <string>
#include <vector>

namespace trajectory {
// File format: one block per curve; first line is point count, followed by x y z rows.
bool append_trajectory(const std::string& filename,
                       const std::vector<Point>& points,
                       std::string& error);
bool load_trajectories(const std::string& filename,
                       std::vector<std::vector<Point>>& trajectories,
                       std::string& error);
}
