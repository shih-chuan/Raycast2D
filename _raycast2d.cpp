#include <iostream>
#include <algorithm>
#include <cmath>
#include <vector>
#include <immintrin.h>
#include <limits>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

namespace py = pybind11;
PYBIND11_MAKE_OPAQUE(std::vector<float>);

constexpr const size_t width = 8;
constexpr const size_t repeat = 1<<10;
constexpr const size_t nelem = width * repeat;

float distance(
    const float& x1, const float& y1, 
    const float& x2, const float& y2
) {
    float x_diff = (x1 - x2);
    float y_diff = (y1 - y2);
    return sqrt(x_diff * x_diff + y_diff * y_diff);
}

float _angle(
    const float& x1, const float& y1, 
    const float& x2, const float& y2
) {
    return atan2f(y1 - y2, x1 - x2);
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
    if (t >= 0 && t <= 1 && u >= 0) {
        pt[0] = x1 + t * (x2 - x1);
        pt[1] = y1 + t * (y2 - y1);
        return true;
    }
    return false;
}

bool intersect_simd(
    const int nWalls, float* pt,
    __m256* mx1, __m256* my1, 
    __m256* mx2, __m256* my2,
    __m256* mx3, __m256* my3, 
    __m256* mx4, __m256* my4
) {
    bool hasIntersection = false;
    int nPacks = nWalls / 8 + 1;
    float min_dist = std::numeric_limits<float>::max();
    for (int i = 0; i < nPacks; i++) {
        __m256 mden = _mm256_set1_ps(0.0);
        __m256 mt = _mm256_set1_ps(0.0);
        __m256 mu = _mm256_set1_ps(0.0);
        __m256 mpt_x = _mm256_set1_ps(0.0);
        __m256 mpt_y = _mm256_set1_ps(0.0);
        mden    = _mm256_sub_ps(
                    _mm256_mul_ps(_mm256_sub_ps(mx1[i], mx2[i]), _mm256_sub_ps(*my3, *my4)), 
                    _mm256_mul_ps(_mm256_sub_ps(my1[i], my2[i]), _mm256_sub_ps(*mx3, *mx4))
                );
        mt      = _mm256_div_ps(
                    _mm256_sub_ps(
                        _mm256_mul_ps(_mm256_sub_ps(mx1[i], *mx3), _mm256_sub_ps(*my3, *my4)), 
                        _mm256_mul_ps(_mm256_sub_ps(my1[i], *my3), _mm256_sub_ps(*mx3, *mx4))
                    ), 
                mden);
        mu     = _mm256_div_ps(
                    _mm256_sub_ps(
                        _mm256_mul_ps(_mm256_sub_ps(mx1[i], mx2[i]), _mm256_sub_ps(my1[i], *my3)), 
                        _mm256_mul_ps(_mm256_sub_ps(my1[i], my2[i]), _mm256_sub_ps(mx1[i], *mx3))
                    ), 
                mden);
        mpt_x = _mm256_add_ps(mx1[i], _mm256_mul_ps(mt, _mm256_sub_ps(mx2[i], mx1[i])));
        mpt_y = _mm256_add_ps(my1[i], _mm256_mul_ps(mt, _mm256_sub_ps(my2[i], my1[i])));
        float* dens = (float*) &mden;
        float* ts = (float*) &mt;
        float* us = (float*) &mu;
        float* pt_xs = (float*) &mpt_x;
        float* pt_ys = (float*) &mpt_y;
        for (int j = 0; j < 8; j++) {
            if (i == nPacks - 1 && j > nWalls % 8 - 1)
                break;
            float den = dens[j], u = -us[j], t = ts[j];
            if (den != 0 && t >= 0 && t <= 1 && u >= 0) {
                if (u < min_dist) {
                    min_dist = u;
                    pt[0] = pt_xs[j];
                    pt[1] = pt_ys[j];
                    hasIntersection = true;
                }
            }
        }
    }
    return hasIntersection;
}

std::vector<float> castRays(float light_x, float light_y, std::vector<float>& walls, int n_rays) {
    std::vector<float> results;
    for (int i = 0; i < n_rays; i++) {
        float angle = i * (M_PI / (n_rays / 2));
        float min_dist = std::numeric_limits<float>::max();
        float min_px = 0, min_py = 0;
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
                    hasIntersection = true;
                }
            }
        }
        if (hasIntersection) {
            results.push_back(min_px);
            results.push_back(min_py);
        }
    }
    return results;
}

std::vector<float> litArea(float light_x, float light_y, std::vector<float>& walls) {
    std::vector<std::tuple<float, float, float>> visibilityAreaPoints;
    std::set<std::tuple<float, float>> endpoints;
    std::vector<float> result;
    float* x1 = (float*)aligned_alloc(32, nelem * sizeof(float));
    float* y1 = (float*)aligned_alloc(32, nelem * sizeof(float));
    float* x2 = (float*)aligned_alloc(32, nelem * sizeof(float));
    float* y2 = (float*)aligned_alloc(32, nelem * sizeof(float));
    for (size_t i = 0; i < walls.size() / 4; i++) {
        x1[i] = walls[i * 4];
        y1[i] = walls[i * 4 + 1];
        x2[i] = walls[i * 4 + 2];
        y2[i] = walls[i * 4 + 3];
    }
    for (size_t i = 0; i < walls.size() / 2; i++) {
        float& ex = walls[i * 2], ey = walls[i * 2 + 1];
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
            __m256 light_x1 = _mm256_set1_ps(light_x);
            __m256 light_y1 = _mm256_set1_ps(light_y);
            __m256 light_x2 = _mm256_set1_ps(light_x + cosf(angle));
            __m256 light_y2 = _mm256_set1_ps(light_y + sinf(angle));
            __m256* mx1 = (__m256*) x1;
            __m256* my1 = (__m256*) y1;
            __m256* mx2 = (__m256*) x2;
            __m256* my2 = (__m256*) y2;
            const int nWalls = (walls.size() / 4);
            float pt[2] = {0.0, 0.0};
            bool intersected = intersect_simd(
                nWalls, pt, mx1, my1, mx2, my2, &light_x1, &light_y1, &light_x2, &light_y2
            );
            if (intersected) {
                visibilityAreaPoints.push_back({pt[0], pt[1], atan2f(pt[1] - light_y, pt[0] - light_x)});
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

std::vector<float> litArea_naive(float light_x, float light_y, std::vector<float>& walls) {
    std::vector<std::tuple<float, float, float>> visibilityAreaPoints;
    std::set<std::tuple<float, float>> endpoints;
    std::vector<float> result;
    for (size_t i = 0; i < walls.size() / 2; i++) {
        endpoints.insert({walls[i * 2], walls[i * 2 + 1]});
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

struct Endpoint {
    float x, y, angle;
    Endpoint(float x, float y, float angle): x(x), y(y), angle(angle) {};
    bool operator<(const Endpoint &e) const {
        return e.angle > angle;
    }
};

std::tuple<float, float, float, float> find_nearest_wall(
    float light_x, float light_y, float angle,
    std::set<std::tuple<float, float, float, float>> walls,
    float intersect_pt[2]
) {
    float min_dist = std::numeric_limits<float>::max();
    float min_px = 0, min_py = 0;
    std::tuple<float, float, float, float> nearest_wall;
    bool hasIntersection = false;
    std::cout << "walls in find nearest wall:" << std::endl;
    for (std::tuple<float, float, float, float> wall : walls) {
        float pt[2], dist;
        std::cout << 
            std::get<0>(wall) << " " << std::get<1>(wall) << " " << 
            std::get<2>(wall) << " " << std::get<3>(wall) << std::endl;
        bool intersected = intersect(
            std::get<0>(wall), std::get<1>(wall), 
            std::get<2>(wall), std::get<3>(wall),
            light_x, light_y,
            light_x + cosf(angle), light_y + sinf(angle),
            pt, dist
        );
        if (intersected) {
            std::cout << "intersected" << std::endl;
            if (dist < min_dist) {
                min_dist = dist;
                min_px = pt[0];
                min_py = pt[1];
                nearest_wall = {
                    std::get<0>(wall), std::get<1>(wall), 
                    std::get<2>(wall), std::get<3>(wall)
                };
                hasIntersection = true;
            }
        }
    }
    if (hasIntersection) {
        intersect_pt[0] = min_px;
        intersect_pt[1] = min_py;
    } else {
        std::cout << "no intersection!!" << std::endl;
    }
    return nearest_wall;
}

std::vector<float> litAreaAccurate(float light_x, float light_y, std::vector<float>& walls) {
    std::set<Endpoint> endpoints;
    std::set<std::tuple<float, float, float, float>> current_walls;
    std::tuple<float, float, float, float> nearest_wall, new_nearest_wall;
    float nearest_pt[2], new_nearest_pt[2];
    std::vector<float> result;
    for (size_t i = 0; i < walls.size() / 2; i++) {
        float& ex = walls[i * 2], ey = walls[i * 2 + 1];
        float angle = _angle(ex, ey, light_x, light_y);
        endpoints.insert(Endpoint(ex, ey, angle));
    }
    std::cout << "angle:" << std::endl;
    // loop over endpoints
    for (const Endpoint &endpoint : endpoints) {
        float ex = endpoint.x, ey = endpoint.y, angle = endpoint.angle;
        // remember which wall is nearest
        std::cout << "endpoint:" << ex << " " << ey << std::endl;
        nearest_wall = find_nearest_wall(light_x, light_y, angle, current_walls, nearest_pt);
        std::cout << "nearest_wall: " << std::endl;
        std::cout << 
            std::get<0>(nearest_wall) << " " << std::get<1>(nearest_wall) << " " << 
            std::get<2>(nearest_wall) << " " << std::get<3>(nearest_wall) << std::endl;
        for (size_t k = 0; k < walls.size(); k += 4) {
            float angle1 = _angle(walls[k], walls[k + 1], light_x, light_y);
            float angle2 = _angle(walls[k + 2], walls[k + 3], light_x, light_y);
            float wall_begin[2], wall_end[2];
            if (angle1 < angle2) {
                wall_begin[0] = walls[k], wall_begin[1] = walls[k + 1];
                wall_end[0] = walls[k + 2], wall_end[1] = walls[k + 3];
            } else {
                wall_end[0] = walls[k], wall_end[1] = walls[k + 1];
                wall_begin[0] = walls[k + 2], wall_begin[1] = walls[k + 3];
            }
            // add any walls that BEGIN at this endpoint to 'walls'
            if (ex == wall_begin[0] && ey == wall_begin[1]) {
                current_walls.insert({walls[k], walls[k + 1], walls[k + 2], walls[k + 3]});
            }
            // remove any walls that END at this endpoint from 'walls'
            if (ex == wall_end[0] && ey == wall_end[1]) {
                current_walls.erase({walls[k], walls[k + 1], walls[k + 2], walls[k + 3]});
            }
        }
        std::cout << "current_walls: " << std::endl;
        for (std::tuple<float, float, float, float> wall : current_walls) {
            std::cout << 
                std::get<0>(wall) << " " << std::get<1>(wall) << " " << 
                std::get<2>(wall) << " " << std::get<3>(wall) << std::endl;
        }
        // figure out which wall is now nearest
        new_nearest_wall = find_nearest_wall(light_x, light_y, angle, current_walls, new_nearest_pt);
        std::cout << "new_nearest_wall: " << std::endl;
        std::cout <<
            std::get<0>(new_nearest_wall) << " " << std::get<1>(new_nearest_wall) << " " << 
            std::get<2>(new_nearest_wall) << " " << std::get<3>(new_nearest_wall) << std::endl;
        std::cout << "new_nearest_point: " << std::endl;
        std::cout << new_nearest_pt[0] << " " << new_nearest_pt[1] << std::endl;
        if (nearest_wall != new_nearest_wall) {
            result.push_back(nearest_pt[0]);
            result.push_back(nearest_pt[1]);
            result.push_back(new_nearest_pt[0]);
            result.push_back(new_nearest_pt[1]);
        }
        std::cout << "results: " << std::endl;
        for (float res : result) {
            std::cout << res << " ";
        }
        std::cout << std::endl;
    }
    return result;
}

PYBIND11_MODULE(_raycast2d, m)
{
    py::bind_vector<std::vector<float>>(m, "FloatVector");
    m.def("litArea_naive", &litArea_naive);
    m.def("litArea", &litArea);
    m.def("castRays", &castRays);
}