#include <iostream>
#include <cmath>
#include <vector>
#include <immintrin.h>
#include <limits>

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
    Vector(float v_x, float v_y) {
        x() = v_x;
        y() = v_y;
    };
    const float & x() const { return buffer[0]; }
    float       & x()       { return buffer[0]; }
    const float & y() const { return buffer[1]; }
    float       & y()       { return buffer[1]; }
    // py::array_t<float> coord() { 
    //     return new float[]{m_x[0], m_y[0]};
    // }
    void normalize() {
        float v_x = x(), v_y = y();
        float length = sqrt(v_x * v_x + v_y * v_y);
        x() = v_x / length;
        y() = v_y / length;
    }
    float distance(Vector &p2) {
        float x_diff = (x() - p2.x());
        float y_diff = (y() - p2.y());
        return sqrt(x_diff * x_diff + y_diff * y_diff);
    }
private:
    float buffer[2];
    // float * m_x = (float *) aligned_alloc(32, nelem * sizeof(float));
    // float * m_y = (float *) aligned_alloc(32, nelem * sizeof(float));
};

class Boundary 
{
public:
    Boundary(float x1, float y1, float x2, float y2) {
        a = Vector(x1, y1);
        b = Vector(x2, y2);
        // std::cout << x1 << ", " << y1 << std::endl;
        // std::cout << x2 << ", " << y2 << std::endl;
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
        // dir.normalize();
    }
    bool cast(Boundary wall, Vector& pt) {
        float x1 = wall.a.x(), y1 = wall.a.y();
        float x2 = wall.b.x(), y2 = wall.b.y();
        // std::cout << x1 << ", " << y1 << std::endl;
        // std::cout << x2 << ", " << y2 << std::endl;
        float x3 = pos.x(), y3 = pos.y();
        float x4 = pos.x() + dir.x(), y4 = pos.y() + dir.y();
        float den = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
        // std::cout << "den: " << den;
        if (den == 0) {
            return false;
        }
        float t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / den;
        float u = -((x1 - x2) * (y1 - y3) - (y1 - y2) * (x1 - x3)) / den;
        // std::cout << " t: " << t << " u: " << u << std::endl;
        if (t >= 0 && t < 1 && u >= 0) {
            pt = Vector(x1 + t * (x2 - x1), y1 + t * (y2 - y1));
            return true;
        }
        return false;
    }
    bool cast_simd(Boundary wall, Vector& pt) {
        float * data = (float *) aligned_alloc(32, nelem * sizeof(float));
        Vector p4(pos.x() + dir.x(), pos.y() + dir.y());
        __m256 * mx1 = (__m256 *) (&wall.a.x()), * my1 = (__m256 *) (&wall.a.y());
        __m256 * mx2 = (__m256 *) (&wall.b.x()), * my2 = (__m256 *) (&wall.b.y());
        __m256 * mx3 = (__m256 *) (&pos.x()), * my3 = (__m256 *) (&pos.y());
        __m256 * mx4 = (__m256 *) (&p4.x()), * my4 = (__m256 *) (&p4.y());
        __m256 * mpt_x = (__m256 *) (&pt.x()), * mpt_y = (__m256 *) (&pt.y());
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
        // std::cout << " t: " << t << " u: " << u << std::endl;
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
    std::vector<float> cast(const std::vector<Boundary> &walls) {
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
                // std::cout << "nearest: " << nearest->x() << " " << nearest->y();
                result.push_back(nearest.x());
                result.push_back(nearest.y());
            }
        }
        return result;
    }
};

class Map {
public:
    Map() = default;
    Map(float light_x, float light_y): light(Light(light_x, light_y)) {};
    Map(Light lt): light(lt) {};
    void add_wall(Boundary wall) {
        walls.push_back(wall);
    }
    void delete_wall(int index) {
        walls.erase(walls.begin() + index);
    }
    std::vector<float> light_cast() {
        return light.cast(walls);
    }
    Light light;
    std::vector<Boundary> walls;
};


PYBIND11_MODULE(_raycast2d, m)
{
    py::bind_vector<std::vector<float>>(m, "VectorFloat");
    py::class_<Vector>(m, "Vector")
        .def(py::init<float, float>())
        .def_property("x", static_cast<const float& (Vector::*)() const>(&Vector::x), static_cast<float& (Vector::*)()>(&Vector::x))
        .def_property("y", static_cast<const float& (Vector::*)() const>(&Vector::y), static_cast<float& (Vector::*)()>(&Vector::y));
        // .def_property("coord", static_cast<const float[]& Vector::coord() const>(&Vector::coord), static_cast<float[]& Vector::coord()>(&Vector::coord));
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
        .def("move", &Light::move)
        .def("cast", &Light::cast);
    py::class_<Map>(m, "Map")
        .def(py::init<float, float>())
        .def(py::init<Light>())
        .def_readwrite("light", &Map::light)
        .def("light_cast", &Map::light_cast)
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