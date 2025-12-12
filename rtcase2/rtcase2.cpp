// raytracer_case1.cpp
#define _USE_MATH_DEFINES
#include <iostream>
#include <vector>
#include <fstream>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <limits>
#include <chrono>

using namespace std;

// ---------------------- Utility / small helpers ----------------------
template<typename T>
T Clamp(T v, T lo, T hi) {
    return (v < lo) ? lo : (v > hi) ? hi : v;
}
uint8_t ClampUint8(int v, int lo, int hi) {
    return static_cast<uint8_t>(Clamp(v, lo, hi));
}

struct Color {
    uint8_t r, g, b;
    Color() : r(0), g(0), b(0) {}
    Color(uint8_t rr, uint8_t gg, uint8_t bb) : r(rr), g(gg), b(bb) {}
    Color operator*(float s) const {
        return Color(
            ClampUint8(static_cast<int>(r * s), 0, 255),
            ClampUint8(static_cast<int>(g * s), 0, 255),
            ClampUint8(static_cast<int>(b * s), 0, 255)
        );
    }
    Color operator+(const Color& o) const {
        return Color(
            ClampUint8(static_cast<int>(r) + static_cast<int>(o.r), 0, 255),
            ClampUint8(static_cast<int>(g) + static_cast<int>(o.g), 0, 255),
            ClampUint8(static_cast<int>(b) + static_cast<int>(o.b), 0, 255)
        );
    }
    float brightness() const { return (r + g + b) / (3.0f * 255.0f); }
};

struct Vec3f {
    float x, y, z;
    Vec3f() : x(0), y(0), z(0) {}
    Vec3f(float xx, float yy, float zz) : x(xx), y(yy), z(zz) {}
    Vec3f operator+(const Vec3f& o) const { return {x+o.x, y+o.y, z+o.z}; }
    Vec3f operator-(const Vec3f& o) const { return {x-o.x, y-o.y, z-o.z}; }
    Vec3f operator*(float s) const { return {x*s, y*s, z*s}; }
    Vec3f operator/(float s) const { return {x/s, y/s, z/s}; }
    Vec3f operator-() const { return {-x, -y, -z}; }
    float length() const { return sqrtf(x*x + y*y + z*z); }
};
float dot(const Vec3f& a, const Vec3f& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
Vec3f cross(const Vec3f& a, const Vec3f& b) {
    return Vec3f(
        a.y*b.z - a.z*b.y,
        a.z*b.x - a.x*b.z,
        a.x*b.y - a.y*b.x
    );
}
Vec3f normalize(const Vec3f& v) {
    float l = v.length();
    return (l > 0.0f) ? v * (1.0f / l) : v;
}

// ---------------------- Image (PPM P3) ----------------------
struct Image {
    int W, H;
    vector<Color> pix;
    explicit Image(int w, int h, Color bg = Color(80,90,110)) : W(w), H(h), pix(w*h, bg) {}
    void PutPixel(int x, int y, Color c) {
        if (x < 0 || x >= W || y < 0 || y >= H) return;
        pix[y * W + x] = c;
    }
    void SavePPM(const string& path) const {
        ofstream f(path);
        f << "P3\n" << W << " " << H << "\n255\n";
        for (const auto& p : pix) f << int(p.r) << " " << int(p.g) << " " << int(p.b) << "\n";
        cout << "Saved: " << path << endl;
    }
};

// ---------------------- Ray / Scene objects ----------------------
struct Ray {
    Vec3f origin, direction;
    Ray(const Vec3f& o, const Vec3f& d) : origin(o), direction(normalize(d)) {}
    Vec3f pointAt(float t) const { return origin + direction * t; }
};

struct Material {
    Color color;
    float ambient, diffuse, specular, shininess;
    Material() : color(255,255,255), ambient(0.1f), diffuse(0.9f), specular(0.3f), shininess(32.0f) {}
    Material(const Color& c, float a, float d, float s, float sh) : color(c), ambient(a), diffuse(d), specular(s), shininess(sh) {}
};

struct Sphere {
    Vec3f center; float radius; Material material;
    Sphere(const Vec3f& c, float r, const Material& m) : center(c), radius(r), material(m) {}
    bool intersect(const Ray& ray, float& t) const {
        Vec3f oc = ray.origin - center;
        float a = dot(ray.direction, ray.direction);
        float b = 2.0f * dot(oc, ray.direction);
        float c = dot(oc, oc) - radius * radius;
        float disc = b*b - 4*a*c;
        if (disc < 0) return false;
        float sq = sqrtf(disc);
        float t0 = (-b - sq) / (2*a);
        float t1 = (-b + sq) / (2*a);
        t = (t0 > 0.001f) ? t0 : t1;
        return t > 0.001f;
    }
    Vec3f normalAt(const Vec3f& pt) const { return normalize(pt - center); }
};

struct Plane {
    Vec3f point; Vec3f normal; Material material;
    Plane(const Vec3f& p, const Vec3f& n, const Material& m) : point(p), normal(normalize(n)), material(m) {}
    bool intersect(const Ray& ray, float& t) const {
        float denom = dot(normal, ray.direction);
        if (fabs(denom) > 1e-6f) {
            Vec3f p0l0 = point - ray.origin;
            t = dot(p0l0, normal) / denom;
            return t >= 0.001f;
        }
        return false;
    }
};

struct Light {
    Vec3f position;
    Color color;
    float intensity;
    Light(const Vec3f& p, const Color& c, float i = 1.0f) : position(p), color(c), intensity(i) {}
};

struct HitRecord {
    float t; Vec3f point; Vec3f normal; Material material; bool hit;
    HitRecord() : t(numeric_limits<float>::max()), hit(false) {}
};

class Scene {
public:
    vector<Sphere> spheres;
    vector<Plane> planes;
    vector<Light> lights;
    Color background;
    Scene() : background(80,90,110) {}

    bool intersect(const Ray& ray, HitRecord& rec) const {
        rec.hit = false;
        rec.t = numeric_limits<float>::max();
        for (const auto& s : spheres) {
            float t;
            if (s.intersect(ray, t) && t < rec.t) {
                rec.t = t;
                rec.point = ray.pointAt(t);
                rec.normal = s.normalAt(rec.point);
                rec.material = s.material;
                rec.hit = true;
            }
        }
        for (const auto& p : planes) {
            float t;
            if (p.intersect(ray, t) && t < rec.t) {
                rec.t = t;
                rec.point = ray.pointAt(t);
                rec.normal = p.normal;
                rec.material = p.material;
                rec.hit = true;
            }
        }
        return rec.hit;
    }

    // Single-sample hard shadow check with normal offset to avoid acne
    bool isInShadow(const Vec3f& point, const Vec3f& lightPos) const {
        Vec3f toLight = lightPos - point;
        float lightDist = toLight.length();
        Vec3f lightDir = normalize(toLight);

        // Offset along normal: helps avoid self-shadowing (shadow acne)
        Ray shadowRay(point + lightDir * 1e-4f /* small offset in light direction */,
                      lightDir);
        HitRecord srec;
        if (intersect(shadowRay, srec)) {
            return srec.t < lightDist;
        }
        return false;
    }

    // Trace a ray (no recursion for reflections), returns color
    Color traceRay(const Ray& ray) const {
        HitRecord rec;
        if (!intersect(ray, rec)) return background;

        // Start with ambient
        float ar = rec.material.ambient;
        Color result = rec.material.color * ar;

        for (const auto& light : lights) {
            // Shadow test (hard shadow)
            bool inShadow = isInShadow(rec.point, light.position);
            if (inShadow) continue;

            // Diffuse
            Vec3f lightDir = normalize(light.position - rec.point);
            float diff = max(0.0f, dot(rec.normal, lightDir));
            Color diffuse = rec.material.color * (rec.material.diffuse * diff);

            // Specular (Blinn-Phong)
            Vec3f viewDir = normalize(-ray.direction);
            Vec3f halfDir = normalize(lightDir + viewDir);
            float spec = powf(max(0.0f, dot(rec.normal, halfDir)), rec.material.shininess);
            Color specCol = light.color * (rec.material.specular * spec);

            // Combine
            Color contrib = diffuse + specCol;
            // Multiply by light intensity (and clamp via Color ops)
            contrib = contrib * light.intensity;
            result = result + contrib;
        }

        // final clamp (already handled by Color ops)
        return result;
    }
};

// ---------------------- Simple evaluation (shadow pixel counting) ----------------------
struct EvalMetrics {
    double renderTimeMs = 0.0;
    int shadowPixels = 0;
    float shadowAreaRatio = 0.0f;
};

EvalMetrics EvaluateImage(const Image& img, float shadowThreshold = 0.30f) {
    EvalMetrics m;
    for (const auto& p : img.pix) {
        if (p.brightness() < shadowThreshold) m.shadowPixels++;
    }
    m.shadowAreaRatio = static_cast<float>(m.shadowPixels) / (img.W * img.H);
    return m;
}

// ---------------------- Scene setup (Case 1: three spheres, hard shadows) ----------------------
void setupCase1(Scene& scene) {
    // Materials
    //Material floorMat(Color(200,200,200), 0.3f, 0.7f, 0.2f, 8.0f);
    Material sphereMat1(Color(255,220,200), 0.2f, 0.8f, 0.3f, 32.0f);
    Material sphereMat2(Color(200,220,255), 0.2f, 0.8f, 0.3f, 32.0f);
    Material sphereMat3(Color(220,255,200), 0.2f, 0.8f, 0.3f, 32.0f);
    //Material wallMat(Color(150,150,200), 0.4f, 0.6f, 0.2f, 8.0f);

    // Planes: floor + back wall + left/right walls
    scene.planes.clear();
    //scene.planes.push_back(Plane(Vec3f(0, 0, 0), Vec3f(0, 1, 0), floorMat)); // floor
    //scene.planes.push_back(Plane(Vec3f(0, 0, -5), Vec3f(0, 0, 1), wallMat)); // back wall
    //scene.planes.push_back(Plane(Vec3f(-5, 0, 0), Vec3f(1, 0, 0), wallMat)); // left
    //scene.planes.push_back(Plane(Vec3f(5, 0, 0), Vec3f(-1, 0, 0), wallMat)); // right
    Material sphereMat4(Color(255,180,180), 0.2f, 0.8f, 0.3f, 32.0f);
    Material sphereMat5(Color(180,255,180), 0.2f, 0.8f, 0.3f, 32.0f);
    // Spheres: positions & radii match your original scene
    scene.spheres.clear();
    scene.spheres.push_back(Sphere(Vec3f(-1.5f, 1.0f, 1.5f), 0.5f, sphereMat1));
    scene.spheres.push_back(Sphere(Vec3f(1.5f, 0.0f, 2.5f), 0.5f, sphereMat2));
    scene.spheres.push_back(Sphere(Vec3f(0.0f, 0.5f, 2.0f), 0.5f, sphereMat3));
    scene.spheres.push_back(Sphere(Vec3f(-0.8f, 2.3f, 1.5f), 0.5f, sphereMat4));
    scene.spheres.push_back(Sphere(Vec3f(0.8f, 1.8f, 2.0f), 0.5f, sphereMat5));

    // Light: brighter point light
    scene.lights.clear();
    scene.lights.push_back(Light(Vec3f(3.0f, 1.0f, 3.0f), Color(255,255,230), 1.0f));

    // background
    scene.background = Color(80,90,110);
}

// ---------------------- Main render ----------------------
int main() {
    const int W = 800;
    const int H = 600;

    cout << "=== RAY TRACER CASE 1: 3 SPHERES, HARD SHADOWS ===" << endl;
    cout << "Image: " << W << " x " << H << endl;

    Scene scene;
    setupCase1(scene);

    Image image(W, H);
    Image shadowMask(W, H, Color(255,255,255)); // white background -> shadow = black

    // Camera
    Vec3f cameraPos(0, 3, 8);
    Vec3f lookAt(0, 0.5f, 0);
    Vec3f up(0, 1, 0);
    Vec3f forward = normalize(lookAt - cameraPos);
    Vec3f right = normalize(cross(forward, up));
    Vec3f cameraUp = normalize(cross(right, forward));
    float fov = 60.0f * M_PI / 180.0f;
    float aspect = static_cast<float>(W) / H;

    auto t0 = chrono::high_resolution_clock::now();

    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            float px = (2.0f * (x + 0.5f) / W - 1.0f) * tanf(fov / 2.0f) * aspect;
            float py = (1.0f - 2.0f * (y + 0.5f) / H) * tanf(fov / 2.0f);

            Vec3f rayDir = normalize(forward + right * px + cameraUp * py);
            Ray r(cameraPos, rayDir);

            // Trace
            Color c = scene.traceRay(r);
            image.PutPixel(x, y, c);

            // For shadow mask: shoot primary hit then check shadow if hit
            HitRecord hr;
            if (scene.intersect(r, hr)) {
                bool inShadow = scene.isInShadow(hr.point, scene.lights[0].position);
                if (inShadow) shadowMask.PutPixel(x, y, Color(0,0,0));
                else shadowMask.PutPixel(x, y, Color(255,255,255));
            } else {
                // background -> not shadowed
                shadowMask.PutPixel(x, y, Color(255,255,255));
            }
        }
        if (y % 60 == 0) cout << "Progress: " << (y * 100 / H) << "%" << endl;
    }

    auto t1 = chrono::high_resolution_clock::now();
    double renderMs = chrono::duration<double, milli>(t1 - t0).count();

    // Save outputs
    image.SavePPM("rtcase2.ppm");
    shadowMask.SavePPM("rtcase2_shadowmask.ppm");

    // Evaluate
    EvalMetrics metrics = EvaluateImage(image, 0.30f);
    metrics.renderTimeMs = renderMs;

    cout << "\n=== SHADOW METRICS ===" << endl;
    cout << "Shadow pixels (brightness < 0.3): " << metrics.shadowPixels << endl;
    cout << "Shadow area ratio: " << (metrics.shadowAreaRatio * 100.0f) << " %" << endl;
    cout << "Render time: " << metrics.renderTimeMs << " ms" << endl;
    cout << "Pixels per second: " << (W * H) / (metrics.renderTimeMs / 1000.0) << endl;

    return 0;
}
