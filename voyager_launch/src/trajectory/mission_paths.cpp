#include "mission_paths.h"

namespace trajectory {
MissionPath voyager_one_path() {
    // Stylised teaching path, not an ephemeris or a precision orbital solution.
    return {"Voyager 1", {
        {0, 0, 0}, {5, 2, -1}, {12, 5, -2},
        {19, 8, 1}, {27, 7, 4}, {38, 12, 2}, {52, 18, 8}
    }};
}
MissionPath voyager_two_path() {
    // Stylised teaching path through the outer planets.
    return {"Voyager 2", {
        {0, 0, 0}, {4, -2, 1}, {13, -5, 3},
        {20, 1, 4}, {30, 5, 0}, {42, -5, -4},
        {55, -8, 3}, {69, -2, 7}, {84, 8, 10}
    }};
}
}
