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
constexpr const size_t repeat = 1<<20;
constexpr const size_t nelem = width * repeat;
struct WallList 
{
    int nWalls = 0;
    float* x1 = (float*)aligned_alloc(32, nelem * sizeof(float));
    float* y1 = (float*)aligned_alloc(32, nelem * sizeof(float));
    float* x2 = (float*)aligned_alloc(32, nelem * sizeof(float));
    float* y2 = (float*)aligned_alloc(32, nelem * sizeof(float));
    float* data[4] = {x1, y1, x2, y2};
    WallList() = default;
    float operator() (int wallId, int pos) const { return data[pos][wallId]; }
    float &operator() (int wallId, int pos) { return data[pos][wallId]; }
    void append(float sx, float sy, float ex, float ey) {
        x1[nWalls] = sx;
        y1[nWalls] = sy;
        x2[nWalls] = ex;
        y2[nWalls] = ey;
        nWalls += 1;
    }
    int size() {
        return nWalls;
    }
    void checkBoundary(int index) {
        if (index >= nWalls) {
            std::cout << "index out of bound, exiting";
            exit(0);
        }
    }
    void clear() {
        nWalls = 0;
    }
};

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

std::vector<float> castRays(float light_x, float light_y, WallList& walls, int n_rays) {
    std::vector<float> results;
    for (int i = 0; i < n_rays; i++) {
        float angle = i * (M_PI / (n_rays / 2));
        float min_dist = std::numeric_limits<float>::max();
        float min_px = 0, min_py = 0;
        bool hasIntersection = false;
        for (int k = 0; k < walls.size(); k++) {
            float pt[2], dist;
            bool intersected =  intersect(
                walls(k, 0), walls(k, 1), 
                walls(k, 2), walls(k, 3),
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

std::vector<float> litArea(float light_x, float light_y, WallList& walls) {
    __m256 light_x1 = _mm256_set1_ps(light_x);
    __m256 light_y1 = _mm256_set1_ps(light_y);
    __m256* mx1 = (__m256*) walls.x1;
    __m256* my1 = (__m256*) walls.y1;
    __m256* mx2 = (__m256*) walls.x2;
    __m256* my2 = (__m256*) walls.y2;
    std::vector<std::tuple<float, float, float>> visibilityAreaPoints;
    std::set<std::tuple<float, float>> endpoints;
    std::vector<float> result;
    for (int i = 0; i < walls.size(); i++) {
        endpoints.insert({walls(i, 0), walls(i, 1)});
        endpoints.insert({walls(i, 2), walls(i, 3)});
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
            __m256 light_x2 = _mm256_set1_ps(light_x + cosf(angle));
            __m256 light_y2 = _mm256_set1_ps(light_y + sinf(angle));
            const int nWalls = walls.size();
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

std::vector<float> litArea_naive(float light_x, float light_y, WallList& walls) {
    std::vector<std::tuple<float, float, float>> visibilityAreaPoints;
    std::set<std::tuple<float, float>> endpoints;
    std::vector<float> result;
    for (int i = 0; i < walls.size(); i++) {
        endpoints.insert({walls(i, 0), walls(i, 1)});
        endpoints.insert({walls(i, 2), walls(i, 3)});
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
            for (int k = 0; k < walls.size(); k ++) {
                float pt[2], dist;
                bool intersected =  intersect(
                    walls(k, 0), walls(k, 1), 
                    walls(k, 2), walls(k, 3),
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
    py::bind_vector<std::vector<float>>(m, "FloatVector");
    py::class_<WallList>(m, "WallList")
        .def(py::init<>())
        .def("clear", &WallList::clear)
        .def("append", &WallList::append)
        .def("__len__", [](WallList &list) { return list.nWalls; })
        .def("__getitem__", [](WallList &list, std::pair<int, int> pos) {
            return list(pos.first, pos.second);
        })
        .def("__setitem__", [](WallList &list, std::pair<int, int> pos, float value) {
            list(pos.first, pos.second) = value;
        });
    m.def("litArea_naive", &litArea_naive);
    m.def("litArea", &litArea);
    m.def("castRays", &castRays);
}