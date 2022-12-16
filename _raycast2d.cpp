#include <iostream>
#include <algorithm>
#include <cmath>
#include <vector>
#include <immintrin.h>
#include <limits>
#define NORTH 0
#define SOUTH 1
#define EAST 2
#define WEST 3 

#include <mkl.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

namespace py = pybind11;
PYBIND11_MAKE_OPAQUE(std::vector<float>);

constexpr const size_t width = 8;
constexpr const size_t repeat = 1024 * 1024;
constexpr const size_t nelem = width * repeat;

float distance(
    const float& x1, const float& y1, 
    const float& x2, const float& y2
) {
    float x_diff = (x1 - x2);
    float y_diff = (y1 - y2);
    return sqrt(x_diff * x_diff + y_diff * y_diff);
}

bool intersect(
    const float& x1, const float& y1, 
    const float& x2, const float& y2,
    const float& x3, const float& y3,
    const float& x4, const float& y4,
    float* pt, float& dist
) {
    float den = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
    if (den == 0) {
        return false;
    }
    float t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / den;
    float u = -((x1 - x2) * (y1 - y3) - (y1 - y2) * (x1 - x3)) / den;
    dist = u;
    if (t >= 0 && t < 1 && u >= 0) {
        pt[0] = x1 + t * (x2 - x1);
        pt[1] = y1 + t * (y2 - y1);
        return true;
    }
    return false;
}

std::vector<float> litArea(float light_x, float light_y, std::vector<float>& walls) {
    std::vector<std::tuple<float, float, float>> visibilityAreaPoints;
    std::set<std::tuple<float, float>> endpoints;
    std::vector<float> result;
    for (size_t i = 0; i < walls.size(); i += 2) {
        float& ex = walls[i], ey = walls[i + 1];
        if (endpoints.find({ex, ey}) != endpoints.end()) {
            continue;
        }
        endpoints.insert({ex, ey});
    }
    for (const std::tuple<float, float> &endpoint : endpoints) {
        float ex = std::get<0>(endpoint), ey = std::get<1>(endpoint);
        float rdx = ex - light_x, rdy = ey - light_y;
        float base_angle = atan2f(rdy, rdx);
        float angle = 0;
        for (int j = 0; j < 3; j++) {
            switch (j) {
                case 0: angle = base_angle - 0.0001f; break;
                case 2: angle = base_angle + 0.0001f; break;
                default: angle = base_angle;
            }
            float min_dist = std::numeric_limits<float>::max();
            float min_px = 0, min_py = 0, min_ang = 0;
            bool hasIntersection = false;
            for (size_t k = 0; k < walls.size(); k += 4) {
                float pt[2], dist;
                bool intersected =  intersect(
                    walls[k], walls[k + 1], 
                    walls[k + 2], walls[k + 3],
                    light_x, light_y,
                    light_x + cosf(angle), light_y + sinf(angle),
                    pt, dist
                );
                if (intersected) {
                    if (dist < min_dist) {
                        min_dist = dist;
                        min_px = pt[0];
                        min_py = pt[1];
                        min_ang = atan2f(min_py - light_y, min_px - light_x);
                        hasIntersection = true;
                    }
                }
            }
            if (hasIntersection) {
                visibilityAreaPoints.push_back({min_px, min_py, min_ang});
            }
        }
    }
    std::sort(
        visibilityAreaPoints.begin(),
        visibilityAreaPoints.end(),
        [&](const std::tuple<float, float, float> &t1, const std::tuple<float, float, float> &t2)
        {
            return std::get<2>(t1) < std::get<2>(t2);
        }
    );
    for (const std::tuple<float, float, float> &t : visibilityAreaPoints) {
        result.push_back(std::get<0>(t));
        result.push_back(std::get<1>(t));
    }
    return result;
}

PYBIND11_MODULE(_raycast2d, m)
{
    py::bind_vector<std::vector<float>>(m, "WallList");
    m.def("litArea", &litArea);
}