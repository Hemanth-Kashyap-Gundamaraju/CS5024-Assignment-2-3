#pragma once
#include <glm/glm.hpp>
#include <vector>

namespace trajectory {
using Point = glm::dvec3;

// Bernstein-polynomial evaluation. t is clamped to [0, 1].
Point evaluate_bernstein(const std::vector<Point>& control_points, double t);

// De Casteljau evaluation and all interpolation levels.
Point evaluate_de_casteljau(const std::vector<Point>& control_points, double t);
std::vector<std::vector<Point>> de_casteljau_levels(
    const std::vector<Point>& control_points, double t);

// Returns samples from t=0 through t=1, inclusive.
std::vector<Point> sample_curve(
    const std::vector<Point>& control_points, unsigned segments = 100);
}
