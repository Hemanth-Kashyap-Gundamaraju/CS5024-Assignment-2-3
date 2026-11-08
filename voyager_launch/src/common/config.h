// config.h - all global settings in one place (ALL_CAPS = constant you can tune).
#pragma once
#include <glm/glm.hpp>

// ---------- window ----------
constexpr int   WINDOW_WIDTH = 1280;
constexpr int   WINDOW_HEIGHT = 720;
constexpr float FIELD_OF_VIEW_DEG = 55.0f;

// ---------- Earth, Moon, air (SI units: metres, seconds, kg) ----------
constexpr double EARTH_RADIUS_M = 6371000.0;
constexpr double EARTH_GM = 3.986004418e14;          // gravity strength (mu)
constexpr double EARTH_SPIN_RATE = 7.2921159e-5;     // radians per second
constexpr double STANDARD_GRAVITY = 9.80665;
constexpr double ATMOSPHERE_TOP_M = 100000.0;        // where the glow shell ends
constexpr double SEA_LEVEL_DENSITY = 1.225;          // kg/m^3
constexpr double AIR_SCALE_HEIGHT_M = 7200.0;        // air thins by e every 7.2 km
constexpr double MOON_RADIUS_M = 1737400.0;
constexpr double MOON_DISTANCE_M = 384400000.0;

// ---------- launch site: Cape Canaveral LC-41 (both Voyagers flew from here) ----------
constexpr double LAUNCH_LATITUDE_DEG = 28.5836;
constexpr double LAUNCH_LONGITUDE_DEG = -80.5828;    // negative = west
constexpr double LAUNCH_AZIMUTH_DEG = 90.0;          // flight direction (assumed: due east)
constexpr double ROCKET_BASE_HEIGHT_M = 3.0;         // rocket sits on a 3 m launch table
constexpr double COUNTDOWN_START_S = -10.0;          // scene starts at T-10 s

// ---------- the two missions ----------
// index 0 = Voyager 1, index 1 = Voyager 2 (same rocket, different day and time)
struct MissionInfo {
    const char* name;
    const char* launch_date;
    double launch_utc_hours;       // launch time of day in UTC
    double sun_declination_deg;    // how far north the Sun is on that date
    glm::vec3 accent_color;        // stripe colour so you can tell the rockets apart
};
inline const MissionInfo MISSIONS[2] = {
    {"Voyager 1", "5 Sep 1977",  12.9333, 6.5,  glm::vec3(0.10f, 0.35f, 0.90f)},
    {"Voyager 2", "20 Aug 1977", 14.4833, 12.5, glm::vec3(0.90f, 0.25f, 0.10f)},
};
inline int g_mission_index = 1;   // which mission is selected right now

// ---------- file locations ----------
#ifndef ASSET_DIR
#define ASSET_DIR "assets/"
#endif
