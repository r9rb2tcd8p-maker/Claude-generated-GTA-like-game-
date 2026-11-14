#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <vector>

static const float PI_F = 3.14159265358979f;

struct V3 { float x = 0, y = 0, z = 0; };
inline V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline V3 operator*(V3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline V3 operator*(float s, V3 a) { return {a.x * s, a.y * s, a.z * s}; }
inline V3& operator+=(V3& a, V3 b) { a.x += b.x; a.y += b.y; a.z += b.z; return a; }
inline V3& operator-=(V3& a, V3 b) { a.x -= b.x; a.y -= b.y; a.z -= b.z; return a; }
inline float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float len(V3 a) { return std::sqrt(dot(a, a)); }
inline float len2d(V3 a) { return std::sqrt(a.x * a.x + a.z * a.z); }
inline float dist2d(V3 a, V3 b) { return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.z - b.z) * (a.z - b.z)); }
inline V3 norm(V3 a) { float l = len(a); return l > 1e-6f ? a * (1.0f / l) : V3{0, 0, 0}; }
inline V3 norm2d(V3 a) { float l = len2d(a); return l > 1e-6f ? V3{a.x / l, 0, a.z / l} : V3{0, 0, 0}; }
inline V3 lerp(V3 a, V3 b, float t) { return a + (b - a) * t; }

inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float wrapAngle(float a) { while (a > PI_F) a -= 2 * PI_F; while (a < -PI_F) a += 2 * PI_F; return a; }
inline float angleDiff(float target, float cur) { return wrapAngle(target - cur); }
inline float lerpAngle(float a, float b, float t) { return a + wrapAngle(b - a) * t; }
inline float smoothT(float rate, float dt) { return 1.0f - std::exp(-rate * dt); }
// Heading convention: yaw 0 faces +Z, forward = (sin, 0, cos), "right" = (-cos, 0, sin)
inline V3 fwdOf(float yaw) { return {std::sin(yaw), 0, std::cos(yaw)}; }
inline V3 rightOf(float yaw) { return {-std::cos(yaw), 0, std::sin(yaw)}; }

struct Mat4 {
    float m[16];
};
inline Mat4 mIdentity() { Mat4 r; for (int i = 0; i < 16; i++) r.m[i] = (i % 5 == 0) ? 1.0f : 0.0f; return r; }
inline Mat4 mMul(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int c = 0; c < 4; c++)
        for (int rr = 0; rr < 4; rr++) {
            float s = 0;
            for (int k = 0; k < 4; k++) s += a.m[k * 4 + rr] * b.m[c * 4 + k];
            r.m[c * 4 + rr] = s;
        }
    return r;
}
inline Mat4 mTranslate(V3 t) { Mat4 r = mIdentity(); r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z; return r; }
inline Mat4 mRotX(float a) { Mat4 r = mIdentity(); float c = std::cos(a), s = std::sin(a); r.m[5] = c; r.m[6] = s; r.m[9] = -s; r.m[10] = c; return r; }
inline Mat4 mRotY(float a) { Mat4 r = mIdentity(); float c = std::cos(a), s = std::sin(a); r.m[0] = c; r.m[2] = -s; r.m[8] = s; r.m[10] = c; return r; }
inline Mat4 mRotZ(float a) { Mat4 r = mIdentity(); float c = std::cos(a), s = std::sin(a); r.m[0] = c; r.m[1] = s; r.m[4] = -s; r.m[5] = c; return r; }
inline Mat4 mPerspective(float fovy, float aspect, float n, float f) {
    Mat4 r; for (int i = 0; i < 16; i++) r.m[i] = 0;
    float t = 1.0f / std::tan(fovy * 0.5f);
    r.m[0] = t / aspect; r.m[5] = t; r.m[10] = (f + n) / (n - f); r.m[11] = -1; r.m[14] = 2 * f * n / (n - f);
    return r;
}
inline Mat4 mLookAt(V3 eye, V3 center, V3 up) {
    V3 f = norm(center - eye), s = norm(cross(f, up)), u = cross(s, f);
    Mat4 r = mIdentity();
    r.m[0] = s.x; r.m[4] = s.y; r.m[8] = s.z;
    r.m[1] = u.x; r.m[5] = u.y; r.m[9] = u.z;
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
    r.m[12] = -dot(s, eye); r.m[13] = -dot(u, eye); r.m[14] = dot(f, eye);
    return r;
}
inline V3 mPoint(const Mat4& m, V3 p) {
    return {m.m[0] * p.x + m.m[4] * p.y + m.m[8] * p.z + m.m[12],
            m.m[1] * p.x + m.m[5] * p.y + m.m[9] * p.z + m.m[13],
            m.m[2] * p.x + m.m[6] * p.y + m.m[10] * p.z + m.m[14]};
}
inline V3 mDir(const Mat4& m, V3 p) {
    return {m.m[0] * p.x + m.m[4] * p.y + m.m[8] * p.z,
            m.m[1] * p.x + m.m[5] * p.y + m.m[9] * p.z,
            m.m[2] * p.x + m.m[6] * p.y + m.m[10] * p.z};
}

struct Col { float r, g, b, a; };  // a = emissive amount in the 3D shader, alpha in the 2D shader
inline Col RGB(uint32_t hex, float a = 0.0f) {
    return {((hex >> 16) & 255) / 255.0f, ((hex >> 8) & 255) / 255.0f, (hex & 255) / 255.0f, a};
}
inline Col shade(Col c, float f) { return {clampf(c.r * f, 0, 1), clampf(c.g * f, 0, 1), clampf(c.b * f, 0, 1), c.a}; }

struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed = 1) : s(seed ? seed : 1) {}
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float f() { return (next() & 0xFFFFFF) / 16777216.0f; }
    float range(float a, float b) { return a + (b - a) * f(); }
    int i(int n) { return n <= 0 ? 0 : (int)(next() % (uint32_t)n); }
    bool chance(float p) { return f() < p; }
};
