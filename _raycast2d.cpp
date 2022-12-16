#include <iostream>
#include <algorithm>
#include <cmath>
#include <vector>
#include <immintrin.h>
#include <limits>
#include <set>
#include <unordered_set>
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

class Vector
{
public:
    Vector() = default;
    Vector(float v_x, float v_y): x(v_x), y(v_y) {};
    void normalize() {
        float v_x = x, v_y = y;
        float length = sqrt(v_x * v_x + v_y * v_y);
        x = v_x / length;
        y = v_y / length;
    }
    float distance(Vector &p2) {
        float x_diff = (x - p2.x);
        float y_diff = (y - p2.y);
        return sqrt(x_diff * x_diff + y_diff * y_diff);
    }
    bool operator< (const Vector& v) const {
        if (x != v.x)
            return x > v.x;
        else 
            return y > v.y;
    }
    bool operator== (const Vector& v) const {
        return x == v.x && y == v.y;
    }
public:
    float x, y;
    // float buffer[2];
    // float * buffer = (float *) aligned_alloc(32, nelem * sizeof(float));
    // float * m_x = (float *) aligned_alloc(32, nelem * sizeof(float));
    // float * m_y = (float *) aligned_alloc(32, nelem * sizeof(float));
};

class Boundary 
{
public:
    Boundary() = default;
    Boundary(float x1, float y1, float x2, float y2) {
        a = Vector(x1, y1);
        b = Vector(x2, y2);
    }
    bool operator< (const Boundary& boundary) const {
        if (a.x != boundary.a.x)
            return a.x > boundary.a.x;
        else 
            return b.x > boundary.b.x;
    }
    bool operator== (const Boundary& boundary) const {
        return a.x == boundary.a.x && a.y == boundary.b.y;
    }
    Vector a;
    Vector b;
};

class Ray 
{
public:
    Ray() = default;
    Ray(float x, float y, float angle) { // angle with radians
        pos = Vector(x, y);
        dir = Vector(cosf(angle), sinf(angle));
        dir.normalize();
    }
    bool cast(Boundary wall, Vector& pt) {
        float x1 = wall.a.x, y1 = wall.a.y;
        float x2 = wall.b.x, y2 = wall.b.y;
        float x3 = pos.x, y3 = pos.y;
        float x4 = pos.x + dir.x, y4 = pos.y + dir.y;
        float den = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
        if (den == 0) {
            return false;
        }
        float t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / den;
        float u = -((x1 - x2) * (y1 - y3) - (y1 - y2) * (x1 - x3)) / den;
        if (t >= 0 && t < 1 && u >= 0) {
            pt = Vector(x1 + t * (x2 - x1), y1 + t * (y2 - y1));
            return true;
        }
        return false;
    }
    bool cast_simd(Boundary wall, Vector& pt) {
        float * data = (float *) aligned_alloc(32, nelem * sizeof(float));
        Vector p4(pos.x + dir.x, pos.y + dir.y);
        __m256 * mx1 = (__m256 *) (&wall.a.x), * my1 = (__m256 *) (&wall.a.y);
        __m256 * mx2 = (__m256 *) (&wall.b.x), * my2 = (__m256 *) (&wall.b.y);
        __m256 * mx3 = (__m256 *) (&pos.x), * my3 = (__m256 *) (&pos.y);
        __m256 * mx4 = (__m256 *) (&p4.x), * my4 = (__m256 *) (&p4.y);
        __m256 * mpt_x = (__m256 *) (&pt.x), * mpt_y = (__m256 *) (&pt.y);
        __m256 * mt = (__m256 *) (&data[0]), * mu = (__m256 *) (&data[1 * width]), * mden = (__m256 *) (&data[2 * width]);

        *mden   = _mm256_sub_ps(
                    _mm256_mul_ps(_mm256_sub_ps(*mx1, *mx2), _mm256_sub_ps(*my3, *my4)), 
                    _mm256_mul_ps(_mm256_sub_ps(*my1, *my2), _mm256_sub_ps(*mx3, *mx4))
                );

        float den = data[2 * width];

        if (den == 0) {
            return false;
        }
        *mt     = _mm256_div_ps(
                    _mm256_sub_ps(
                        _mm256_mul_ps(_mm256_sub_ps(*mx1, *mx3), _mm256_sub_ps(*my3, *my4)), 
                        _mm256_mul_ps(_mm256_sub_ps(*my1, *my3), _mm256_sub_ps(*mx3, *mx4))
                    ), 
                *mden);
        *mu     = _mm256_div_ps(
                    _mm256_sub_ps(
                        _mm256_mul_ps(_mm256_sub_ps(*mx1, *mx2), _mm256_sub_ps(*my1, *my3)), 
                        _mm256_mul_ps(_mm256_sub_ps(*my1, *my2), _mm256_sub_ps(*mx1, *mx3))
                    ), 
                *mden);
        float t = data[0], u = -data[1 * width]; 
        if (t > 0 && t < 1 && u > 0) {
            *mpt_x = _mm256_add_ps(*mx1, _mm256_mul_ps(*mt, _mm256_sub_ps(*mx2, *mx1)));
            *mpt_y = _mm256_add_ps(*my1, _mm256_mul_ps(*mt, _mm256_sub_ps(*my2, *my1)));
            return true;
        }
        return false;
    }
    Vector pos;
    Vector dir;
};

class Light {
public:
    Light() = default;
    Light(float x, float y): pos(Vector(x, y)) {
        for (int i = 0; i < 360; i++) {
            Ray r (x, y, i * M_PI / 180);
            rays.push_back(r);
        }
    };
    void move(float x, float y) {
        pos = Vector(x, y);
        rays.clear();
        for (int i = 0; i <= 360; i++) {
            Ray r (x, y, i * M_PI / 180);
            rays.push_back(r);
        }
    }
    Vector pos;
    std::vector<Ray> rays;
    bool cast(float rad, Boundary wall, Vector &pt) {
        Ray r (pos.x, pos.y, rad);
        return r.cast(wall, pt);
    }
    std::vector<float> radiate(const std::vector<Boundary> &walls) {
        std::vector<float> result;
        for (Ray ray : rays) {
            Vector nearest;
            bool hasIntersection = false;
            float record = std::numeric_limits<float>::max();
            for (const Boundary& wall : walls) {
                Vector pt;
                bool intersected =  ray.cast(wall, pt);
                if (intersected) {
                    float d = pos.distance(pt);
                    if (d < record) {
                        record = d;
                        nearest = pt;
                        hasIntersection = true;
                    }
                }
            }
            if (hasIntersection) {
                result.push_back(nearest.x);
                result.push_back(nearest.y);
            }
        }
        return result;
    }
};

class Endpoint
{
public:
    Endpoint() = default;
    Endpoint(float x, float y, float l_x, float l_y): pos(Vector(x, y)) {
        angle = atan2f(l_y - y, l_x - x);
    }
    bool operator< (const Endpoint& e) const {
        return angle > e.angle;
    }
    bool operator== (const Endpoint& e) const {
        return pos.x == e.pos.x && pos.y == e.pos.y && angle == e.angle;
    }
    Vector pos;
    float angle;
};

class Map {
public:
    Map() = default;
    Map(float light_x, float light_y): light(Light(light_x, light_y)) {};
    Map(Light lt): light(lt) {};
    void add_wall(Boundary wall) {
        walls.push_back(wall);
        endpoints.insert(Vector(wall.a.x, wall.a.y));
        endpoints.insert(Vector(wall.b.x, wall.b.y));
    }
    void delete_wall(int index) {
        walls.erase(walls.begin() + index);
    }
    std::vector<float> light_cast() {
        return light.radiate(walls);
    }
    std::vector<float> visibility_polygon() {
        std::vector<std::tuple<float, float, float>> visibilityAreaPoints; 
        std::vector<float> result;
        for (Vector e : endpoints) {
            float rdx = e.x - light.pos.x, rdy = e.y - light.pos.y;
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
                
                for (Boundary wall : walls) {
                    Vector pt;
                    bool intersected =  light.cast(angle, wall, pt);
                    if (intersected) {
                        float dist = light.pos.distance(pt);
                        if (dist < min_dist) {
                            min_dist = dist;
                            min_px = pt.x;
                            min_py = pt.y;
                            min_ang = atan2f(min_py - light.pos.y, min_px - light.pos.x);
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
    Light light;
    std::vector<Boundary> walls;
    std::set<Vector> endpoints;
};


PYBIND11_MODULE(_raycast2d, m)
{
    py::bind_vector<std::vector<float>>(m, "VectorFloat");
    py::class_<Vector>(m, "Vector")
        .def(py::init<float, float>())
        .def_readwrite("x", &Vector::x)
        .def_readwrite("y", &Vector::y);
    py::class_<Boundary>(m, "Boundary")
        .def(py::init<float, float, float, float>())
        .def_readwrite("a", &Boundary::a)
        .def_readwrite("b", &Boundary::b);
    py::class_<Ray>(m, "Ray")
        .def_readwrite("pos", &Ray::pos)
        .def_readwrite("dir", &Ray::dir)
        .def("cast", &Ray::cast);
    py::class_<Light>(m, "Light")
        .def(py::init<float, float>())
        .def_readwrite("pos", &Light::pos)
        .def("move", &Light::move);
        // .def("radiate", &Light::radiate);
    py::class_<Map>(m, "Map")
        .def(py::init<float, float>())
        .def(py::init<Light>())
        .def_readwrite("light", &Map::light)
        .def_readwrite("walls", &Map::walls)
        .def("light_cast", &Map::light_cast)
        .def("visibility_polygon", &Map::visibility_polygon)
        .def("add_wall", &Map::add_wall);
}

// int main() {
//     Ray r (0, 50, 0 * M_PI / 180);
//     Boundary wall (0, 0, 5, 100);
//     Vector pt;
//     Light lt (150, 150);
//     Map map;
//     map.light = lt;
//     std::vector<Boundary> walls;
//     walls.push_back(wall);

//     float scr_w = 500, scr_h = 500; 
//     walls.push_back(Boundary(0, 0, scr_w, 0));
//     walls.push_back(Boundary(scr_w, 0, scr_w, scr_h));
//     walls.push_back(Boundary(scr_w, scr_h, 0, scr_h));
//     walls.push_back(Boundary(0, scr_h, 0, 0));

//     for (Boundary wall : walls) {
//         map.add_wall(wall.a.x(), wall.a.y(), wall.b.x(), wall.b.y());
//     }

//     std::vector<float> res = map.light_cast();
//     std::cout << res.size() << std::endl;
    // for (float ray : res) {
    //     std::cout << ray.x() << " " << ray.y() << std::endl;
    // }
    // std::vector<Vector> results = lt.cast(walls);
    // std::cout << results.size() << std::endl;
    // for (Vector res : results) {
    //     std::cout << "nearest: " << res.x() << " " << res.y() << std::endl;
    // }
    // bool intersect = r.cast_simd(wall, pt);
    // if (intersect) {
    //     std::cout << pt.x() << ", " << pt.y() << std::endl;
    // } else {
    //     std::cout << "no intersect";
    // }
//     return 0;
// }