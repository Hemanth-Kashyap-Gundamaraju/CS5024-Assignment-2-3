#include "bezier.h"
#include <cassert>
#include <cmath>
#include <iostream>

static bool near(double a, double b) { return std::abs(a - b) < 1e-9; }

int main() {
    using trajectory::Point;
    const std::vector<Point> line{{0,0,0}, {10,0,0}};
    assert(near(trajectory::evaluate_bernstein(line, 0.0).x, 0.0));
    assert(near(trajectory::evaluate_bernstein(line, 1.0).x, 10.0));
    assert(near(trajectory::evaluate_bernstein(line, 0.25).x, 2.5));
    assert(near(trajectory::evaluate_de_casteljau(line, 0.25).x, 2.5));

    const std::vector<Point> quadratic{{0,0,0}, {1,2,0}, {2,0,0}};
    const Point a = trajectory::evaluate_bernstein(quadratic, 0.5);
    const Point b = trajectory::evaluate_de_casteljau(quadratic, 0.5);
    assert(glm::length(a - b) < 1e-9);
    assert(trajectory::de_casteljau_levels(quadratic, 0.5).size() == 3);
    assert(trajectory::sample_curve(quadratic, 20).size() == 21);
    std::cout << "All Bezier tests passed!\n";
}
