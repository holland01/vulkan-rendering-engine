#pragma once

#include "common.h"
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/projection.hpp>
#include <sstream>
#include <string>

struct geom {
    struct ray {
        vec3_t orig{0.0f};
        vec3_t dir{0.0f}; // normalized
        real_t t0{0.0f};
        real_t t1{0.0f};

        std::string to_string(const std::string& prefix = "Ray") const {
            std::stringstream ss;
            ss << prefix << ": { orig: " << glm::to_string(orig) << ", dir: " << glm::to_string(dir) << ", t0: " << t0 << ", t1: " << t1 << " }";
            return ss.str();
        }
    };

    struct bvol {
        enum volume_type {
            type_sphere = 0,
            type_aabb
        };

        vec3_t center;
        
        union {
            vec3_t extents; // aabb 
            real_t radius; // bounds sphere
        };

        volume_type type{type_sphere};

        std::string to_string(volume_type t) const {
            std::string r;
            switch (t) {
                case type_sphere: r = "type_sphere"; break;
                case type_aabb: r = "type_aabb"; break;
                default: ASSERT(false); break;
            }
            return r;
        }

        std::string to_string(const std::string& prefix = "bvol") const {
            std::stringstream ss;

            ss << prefix << ": { center: " << glm::to_string(center);
            
            switch (type) {
                case type_sphere:
                    ss << ", radius: " <<  radius;
                    break;
                case type_aabb:
                    ss << ", extents: " << glm::to_string(extents); 
                    break;
            }
            
            ss << ", type: " << to_string(type) << " }";

            return ss.str();
        }
    };

    bool test_ray_sphere(ray& r, const bvol& s) {
        ASSERT(s.type == bvol::type_sphere);

        real_t t0 = 0.0f;
        real_t t1 = 0.0f; // solutions for t if the ray intersects 
        real_t radius2 = s.radius * s.radius;
        vec3_t L = s.center - r.orig;
        real_t tca = glm::dot(L, r.dir);
        real_t d2 = glm::dot(L, L) - tca * tca;
        if (d2 > radius2) return false;
        real_t thc = sqrt(radius2 - d2);
        t0 = tca - thc;
        t1 = tca + thc;

        if (t0 > t1) std::swap(t0, t1);

        if (t0 < 0) {
            t0 = t1; // if t0 is negative, let's use t1 instead 
            if (t0 < 0) return false; // both t0 and t1 are negative 
        }

        r.t0 = t0;
        r.t1 = t1;

        return true;
    }

    real_t dist_point_plane(const vec3_t& p, const vec3_t& normal, const vec3_t& plane_p) {
        vec3_t pl2p{p - plane_p};
        return glm::length(glm::proj(pl2p, normal));        
    }

} g_geom{};
