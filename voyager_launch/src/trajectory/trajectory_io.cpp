#include "trajectory_io.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

namespace trajectory {
bool append_trajectory(const std::string& filename,
                       const std::vector<Point>& points, std::string& error) {
    if (points.size() < 2) {
        error = "A trajectory must contain at least two control points.";
        return false;
    }
    std::ofstream out(filename, std::ios::app);
    if (!out) { error = "Could not open " + filename + " for writing."; return false; }
    out << points.size() << '\n' << std::setprecision(17);
    for (const Point& p : points) out << p.x << ' ' << p.y << ' ' << p.z << '\n';
    if (!out) { error = "An error occurred while writing " + filename + "."; return false; }
    return true;
}

bool load_trajectories(const std::string& filename,
                       std::vector<std::vector<Point>>& trajectories,
                       std::string& error) {
    std::ifstream in(filename);
    if (!in) { error = "Could not open " + filename + "."; return false; }
    std::vector<std::vector<Point>> loaded;
    std::string line;
    auto next_data_line = [&]() -> bool {
        while (std::getline(in, line)) {
            const auto first = line.find_first_not_of(" \t\r");
            if (first == std::string::npos || line[first] == '#') continue;
            return true;
        }
        return false;
    };
    while (next_data_line()) {
        std::istringstream count_row(line);
        std::size_t count = 0;
        if (!(count_row >> count) || count < 2 || count > 10000) {
            error = "Invalid control-point count in " + filename + ": " + line;
            return false;
        }
        std::vector<Point> curve; curve.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            if (!next_data_line()) {
                error = "Incomplete control-point block in " + filename + ".";
                return false;
            }
            std::istringstream point_row(line); Point p;
            if (!(point_row >> p.x >> p.y >> p.z)) {
                error = "Invalid point row in " + filename + ": " + line;
                return false;
            }
            curve.push_back(p);
        }
        loaded.push_back(std::move(curve));
    }
    if (loaded.empty()) { error = "No trajectories found in " + filename + "."; return false; }
    trajectories = std::move(loaded);
    return true;
}
}
