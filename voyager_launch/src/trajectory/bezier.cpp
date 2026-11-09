#include "bezier.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace trajectory {
namespace {
double binomial(unsigned n, unsigned k) {
    if (k > n) return 0.0;
    k = std::min(k, n - k);
    double result = 1.0;
    for (unsigned i = 1; i <= k; ++i)
        result = result * static_cast<double>(n - k + i) / i;
    return result;
}
}

Point evaluate_bernstein(const std::vector<Point>& p, double t) {
    if (p.empty()) throw std::invalid_argument("A Bezier curve needs at least one point");
    t = std::clamp(t, 0.0, 1.0);
    const unsigned n = static_cast<unsigned>(p.size() - 1);
    Point result(0.0);
    for (unsigned i = 0; i <= n; ++i) {
        const double weight = binomial(n, i) *
            std::pow(1.0 - t, static_cast<int>(n - i)) *
            std::pow(t, static_cast<int>(i));
        result += weight * p[i];
    }
    return result;
}

std::vector<std::vector<Point>> de_casteljau_levels(
    const std::vector<Point>& p, double t) {
    if (p.empty()) throw std::invalid_argument("A Bezier curve needs at least one point");
    t = std::clamp(t, 0.0, 1.0);
    std::vector<std::vector<Point>> levels;
    levels.push_back(p);
    while (levels.back().size() > 1) {
        const auto& previous = levels.back();
        std::vector<Point> next;
        next.reserve(previous.size() - 1);
        for (std::size_t i = 0; i + 1 < previous.size(); ++i)
            next.push_back((1.0 - t) * previous[i] + t * previous[i + 1]);
        levels.push_back(std::move(next));
    }
    return levels;
}

Point evaluate_de_casteljau(const std::vector<Point>& p, double t) {
    return de_casteljau_levels(p, t).back().front();
}

std::vector<Point> sample_curve(const std::vector<Point>& p, unsigned segments) {
    if (p.empty()) return {};
    segments = std::max(segments, 1u);
    std::vector<Point> result;
    result.reserve(segments + 1);
    for (unsigned i = 0; i <= segments; ++i)
        result.push_back(evaluate_bernstein(p, static_cast<double>(i) / segments));
    return result;
}
}
