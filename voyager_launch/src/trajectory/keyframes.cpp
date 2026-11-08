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
        if (!(row >> k.active >> k.parameter
                  >> k.eye.x >> k.eye.y >> k.eye.z
                  >> k.target.x >> k.target.y >> k.target.z)) {
            error = "Invalid keyframe row: " + line;
            return false;
        }
        if ((k.active != 0 && k.active != 1) ||
            k.parameter < 0.0 || k.parameter > 1.0) {
            error = "Activity must be 0/1 and curve parameter must be in [0,1].";
            return false;
        }
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
    if (time <= f.front().time)
        return {f.front().active, f.front().parameter, f.front().eye, f.front().target};
    if (time >= f.back().time)
        return {f.back().active, f.back().parameter, f.back().eye, f.back().target};

    auto upper = std::upper_bound(f.begin(), f.end(), time,
        [](double t, const Keyframe& k) { return t < k.time; });
    const Keyframe& b = *upper;
    const Keyframe& a = *(upper - 1);
    const double u = (time - a.time) / (b.time - a.time);
    KeyframeState s;
    // Activity is discrete; it changes exactly at the next keyframe.
    s.active = a.active;
    s.parameter = (1.0 - u) * a.parameter + u * b.parameter;
    s.eye = (1.0 - u) * a.eye + u * b.eye;
    s.target = (1.0 - u) * a.target + u * b.target;
    return s;
}
}
