#pragma once
#include "math3d.h"
#include <vector>
#include <cstddef>

struct Vert {
    float p[3];
    float n[3];
    float c[4];  // rgb + emissive
};

// Builds triangle soup for oriented boxes. Used for both the baked static world and the per-frame dynamic batch.
struct MeshBuilder {
    std::vector<Vert> v;
    void clear() { v.clear(); }
    void boxM(const Mat4& m, V3 half, Col c);
    void box(V3 center, V3 half, float yaw, Col c) { boxM(mMul(mTranslate(center), mRotY(yaw)), half, c); }
    void beam(V3 a, V3 b, float halfWidth, Col c);  // thin box between two points (tracers, poles)
};

class Renderer {
public:
    bool init(int w, int h);
    void resize(int w, int h);
    void uploadStatic(const std::vector<Vert>& verts);
    void beginFrame();
    void draw3D(const Mat4& view, const Mat4& proj, V3 eye, const std::vector<Vert>& dynamic,
                Col fogColor, float fogNear, float fogFar, float sunPower);
    // 2D overlay (pixel coordinates, origin top-left)
    void rect(float x, float y, float w, float h, Col c);
    void gradRect(float x, float y, float w, float h, Col top, Col bottom);
    void tri(float x0, float y0, float x1, float y1, float x2, float y2, Col c);
    void quad(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, Col c);
    void line(float x0, float y0, float x1, float y1, float thick, Col c);
    void disc(float cx, float cy, float r, Col c, int seg = 20);
    void text(float x, float y, float scale, const char* s, Col c, bool shadow = true);
    float textWidth(const char* s, float scale) const;
    void flush2D();
    void setScissor(float x, float y, float w, float h);  // top-left pixel coordinates
    void clearScissor();
    int W = 1280, H = 720;

private:
    struct V2 { float x, y, r, g, b, a; };
    std::vector<V2> b2;
    unsigned prog3 = 0, prog2 = 0;
    unsigned vaoS = 0, vboS = 0, vaoD = 0, vboD = 0, vao2 = 0, vbo2 = 0;
    int staticCount = 0;
    unsigned compile(unsigned type, const char* src);
    unsigned link(const char* vs, const char* fs);
    void setupVertexLayout3D();
};
