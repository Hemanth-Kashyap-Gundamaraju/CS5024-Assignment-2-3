#include "keyframes.h"
#include <algorithm>
#include <fstream>
#include <sstream>

namespace trajectory {
bool load_keyframes(const std::string& filename, const std::string& mission,
                    std::vector<Keyframe>& frames, std::string& error) {
    std::ifstream in(filename);
    if (!in) { error = "Could not open " + filename + "."; return false; }

    std::vector<Keyframe> parsed;
    std::string line;
    bool in_section = false;
    bool found_section = false;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (line[0] == '#') continue;
        std::istringstream row(line);
        std::string first;
        row >> first;
        if (first == "MISSION") {
            std::string name;
            row >> name;
            in_section = (name == mission);
            found_section = found_section || in_section;
            continue;
        }
        if (first == "END") {
            in_section = false;
            continue;
        }
        if (!in_section) continue;

        Keyframe k;
        try { k.time = std::stod(first); }
        catch (...) { error = "Invalid keyframe time: " + line; return false; }
        std::vector<double> values;
        double value;
        while (row >> value) values.push_back(value);
        // Legacy format: active u eye.xyz target.xyz (8 values).
        // Extended format: active1 u1 active2 u2 ... eye.xyz target.xyz.
        if (values.size() < 8 || (values.size() - 6) % 2 != 0) {
            error = "Invalid keyframe row (expected activity/parameter pairs followed by eye and target): " + line;
            return false;
        }
        const std::size_t pair_count = (values.size() - 6) / 2;
        for (std::size_t i = 0; i < pair_count; ++i) {
            const int active = static_cast<int>(values[2*i]);
            const double parameter = values[2*i+1];
            if ((active != 0 && active != 1) || parameter < 0.0 || parameter > 1.0 ||
                values[2*i] != active) {
                error = "Activity flags must be 0/1 and curve parameters must be in [0,1].";
                return false;
            }
            k.active_flags.push_back(active);
            k.parameters.push_back(parameter);
        }
        k.active = k.active_flags.front();
        k.parameter = k.parameters.front();
        const std::size_t c = values.size() - 6;
        k.eye = Point(values[c], values[c+1], values[c+2]);
        k.target = Point(values[c+3], values[c+4], values[c+5]);
        if (!parsed.empty() && k.time <= parsed.back().time) {
            error = "Keyframe times must be strictly increasing.";
            return false;
        }
        parsed.push_back(k);
    }
    if (!found_section || parsed.empty()) {
        error = "No keyframes found for mission " + mission + ".";
        return false;
    }
    frames = std::move(parsed);
    return true;
}

KeyframeState evaluate_keyframes(const std::vector<Keyframe>& f, double time) {
    if (f.empty()) return {};
    if (time <= f.front().time) {
        KeyframeState s;
        s.active=f.front().active; s.parameter=f.front().parameter;
        s.active_flags=f.front().active_flags; s.parameters=f.front().parameters;
        s.eye=f.front().eye; s.target=f.front().target;
        return s;
    }
    if (time >= f.back().time) {
        KeyframeState s;
        s.active=f.back().active; s.parameter=f.back().parameter;
        s.active_flags=f.back().active_flags; s.parameters=f.back().parameters;
        s.eye=f.back().eye; s.target=f.back().target;
        return s;
    }

    auto upper = std::upper_bound(f.begin(), f.end(), time,
        [](double t, const Keyframe& k) { return t < k.time; });
    const Keyframe& b = *upper;
    const Keyframe& a = *(upper - 1);
    const double raw_u = (time - a.time) / (b.time - a.time);
    // Smoothstep easing removes abrupt velocity changes between authored keyframes.
    // Discrete activity flags below remain exactly 0 or 1 and switch only at boundaries.
    const double u = raw_u * raw_u * (3.0 - 2.0 * raw_u);
    KeyframeState s;
    // Activity is discrete; it changes exactly at the next keyframe.
    s.active = a.active;
    s.parameter = (1.0 - u) * a.parameter + u * b.parameter;
    s.active_flags = a.active_flags;
    s.parameters.resize(std::min(a.parameters.size(), b.parameters.size()));
    for (std::size_t i=0; i<s.parameters.size(); ++i) {
        s.active_flags[i] = a.active_flags[i]; // discrete flags never interpolate
        s.parameters[i] = (1.0-u)*a.parameters[i] + u*b.parameters[i];
    }
    s.eye = (1.0 - u) * a.eye + u * b.eye;
    s.target = (1.0 - u) * a.target + u * b.target;
    return s;
}
}
