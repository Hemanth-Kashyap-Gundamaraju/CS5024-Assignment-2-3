#include "trajectory_io.h"
#include <fstream>
#include <iomanip>

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
    std::size_t count = 0;
    while (in >> count) {
        if (count < 2 || count > 10000) {
            error = "Invalid control-point count in " + filename + ".";
            return false;
        }
        std::vector<Point> curve;
        curve.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            Point p;
            if (!(in >> p.x >> p.y >> p.z)) {
                error = "Incomplete or invalid control-point block in " + filename + ".";
                return false;
            }
            curve.push_back(p);
        }
        loaded.push_back(std::move(curve));
    }
    if (!in.eof()) { error = "Unexpected text in " + filename + "."; return false; }
    trajectories = std::move(loaded);
    return true;
}
}
