// frames.h - maths for where things are: Earth spin, the launch site, the Sun, the Moon.
// World frame: origin = Earth centre, +Y = Earth's spin axis, Sun is near +X. Units: metres.
// All positions are double so a 6,371 km number still keeps centimetre detail.
#pragma once
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include "config.h"

using glm::dvec3;
using glm::dmat3;

// Description: degrees -> radians.  In: degrees.  Out: radians.
inline double deg_to_rad(double degrees) { return degrees * glm::pi<double>() / 180.0; }

// Description: matrix that spins things about +Y by theta (Earth turning west->east).
inline dmat3 earth_rotation(double theta) {
    double c = std::cos(theta), s = std::sin(theta);
    return dmat3(dvec3(c, 0, -s), dvec3(0, 1, 0), dvec3(s, 0, c));
}

// Description: unit vector from Earth centre to a lat/lon, in Earth's own (spinning) frame.
inline dvec3 body_direction(double lat_deg, double lon_deg) {
    double la = deg_to_rad(lat_deg), lo = deg_to_rad(lon_deg);
    return dvec3(std::cos(la) * std::cos(lo), std::sin(la), -std::cos(la) * std::sin(lo));
}

// The three directions on the ground at a place. south is used so (east, up, south) is right-handed.
struct SiteFrame { dvec3 east, up, south; };

// Description: east/up/south directions at a lat/lon, in Earth's own frame.
inline SiteFrame site_frame_body(double lat_deg, double lon_deg) {
    double la = deg_to_rad(lat_deg), lo = deg_to_rad(lon_deg);
    SiteFrame f;
    f.up = body_direction(lat_deg, lon_deg);
    f.east = dvec3(-std::sin(lo), 0.0, -std::cos(lo));
    dvec3 north(-std::sin(la) * std::cos(lo), std::cos(la), std::sin(la) * std::sin(lo));
    f.south = -north;
    return f;
}

// A place in the world: where it is and how it is turned (columns of basis = east, up, south).
struct SitePose { dvec3 position; dmat3 basis; };

// Description: world position and orientation of the launch pad when Earth has turned by theta.
// In: theta, height above ground (m).  Out: SitePose.
inline SitePose launch_site_pose(double theta, double height_m = 0.0) {
    SiteFrame f = site_frame_body(LAUNCH_LATITUDE_DEG, LAUNCH_LONGITUDE_DEG);
    dmat3 spin = earth_rotation(theta);
    SitePose pose;
    pose.basis = spin * dmat3(f.east, f.up, f.south);
    pose.position = spin * (f.up * (EARTH_RADIUS_M + height_m));
    return pose;
}

// Description: direction towards the Sun for a mission (Sun is on +X, tilted by its declination).
inline glm::vec3 sun_direction(const MissionInfo& mission) {
    double dec = deg_to_rad(mission.sun_declination_deg);
    return glm::vec3((float)std::cos(dec), (float)std::sin(dec), 0.0f);
}

// Description: Earth's spin angle at liftoff so the Sun is at the right place for the launch time.
// (At 12:00 UTC the Greenwich meridian faces the Sun, each hour turns 15 degrees.)
inline double earth_angle_at_launch(const MissionInfo& mission) {
    return deg_to_rad((mission.launch_utc_hours - 12.0) * 15.0);
}

// Description: Moon position in the world (fixed on the night side, roughly full Moon).
inline dvec3 moon_position() {
    return glm::normalize(dvec3(-0.80, 0.06, 0.60)) * MOON_DISTANCE_M;
}

// Description: velocity of the ground/air at a point because Earth is spinning (omega x r).
inline dvec3 earth_surface_velocity(const dvec3& position) {
    return dvec3(EARTH_SPIN_RATE * position.z, 0.0, -EARTH_SPIN_RATE * position.x);
}

// Description: air density (kg/m^3) at a height. Zero above the atmosphere, so no drag in space.
inline double air_density(double altitude_m) {
    if (altitude_m < 0.0) altitude_m = 0.0;
    if (altitude_m > 150000.0) return 0.0;
    return SEA_LEVEL_DENSITY * std::exp(-altitude_m / AIR_SCALE_HEIGHT_M);
}

// Description: the smallest turn that takes direction `from` onto direction `to` (both unit vectors).
// Used to keep the rocket pointing along its thrust without rolling it around.
inline glm::dquat rotation_between(const dvec3& from, const dvec3& to) {
    double d = glm::clamp(glm::dot(from, to), -1.0, 1.0);
    dvec3 axis = glm::cross(from, to);
    double len = glm::length(axis);
    if (len < 1e-12) {
        if (d > 0.0) return glm::dquat(1.0, 0.0, 0.0, 0.0);
        dvec3 any = std::fabs(from.x) < 0.9 ? dvec3(1, 0, 0) : dvec3(0, 1, 0);
        return glm::angleAxis(glm::pi<double>(), glm::normalize(glm::cross(from, any)));
    }
    return glm::angleAxis(std::acos(d), axis / len);
}
