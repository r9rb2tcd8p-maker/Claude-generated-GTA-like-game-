#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#else
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>
#endif
#include "renderer.h"
#include <cstdio>
#include <cstring>
#include <cctype>

// ---------------------------------------------------------------- mesh building
void MeshBuilder::boxM(const Mat4& m, V3 h, Col c) {
    const float hh[3] = {h.x, h.y, h.z};
    const int su[4] = {-1, 1, 1, -1}, sv[4] = {-1, -1, 1, 1};
    const int idx[6] = {0, 1, 2, 0, 2, 3};
    for (int a = 0; a < 3; a++)
        for (int s = -1; s <= 1; s += 2) {
            int ua = (a + 1) % 3, va = (a + 2) % 3;
            float nl[3] = {0, 0, 0};
            nl[a] = (float)s;
            V3 nw = norm(mDir(m, {nl[0], nl[1], nl[2]}));
            V3 corner[4];
            for (int k = 0; k < 4; k++) {
                float p[3];
                p[a] = s * hh[a];
                p[ua] = su[k] * hh[ua];
                p[va] = sv[k] * hh[va];
                corner[k] = mPoint(m, {p[0], p[1], p[2]});
            }
            for (int k = 0; k < 6; k++) {
                V3 q = corner[idx[k]];
                Vert vt;
                vt.p[0] = q.x; vt.p[1] = q.y; vt.p[2] = q.z;
                vt.n[0] = nw.x; vt.n[1] = nw.y; vt.n[2] = nw.z;
                vt.c[0] = c.r; vt.c[1] = c.g; vt.c[2] = c.b; vt.c[3] = c.a;
                v.push_back(vt);
            }
        }
}

void MeshBuilder::beam(V3 a, V3 b, float hw, Col c) {
    V3 d = b - a;
    float l = len(d);
    if (l < 1e-4f) return;
    float yaw = std::atan2(d.x, d.z);
    float pitch = -std::asin(clampf(d.y / l, -1, 1));
    Mat4 m = mMul(mTranslate((a + b) * 0.5f), mMul(mRotY(yaw), mRotX(pitch)));
    boxM(m, {hw, hw, l * 0.5f}, c);
}

// ---------------------------------------------------------------- shaders
static const char* VS3 = R"(#version 330 core
layout(location=0) in vec3 aP;
layout(location=1) in vec3 aN;
layout(location=2) in vec4 aC;
uniform mat4 uVP;
out vec3 vN; out vec4 vC; out vec3 vW;
void main(){ vN=aN; vC=aC; vW=aP; gl_Position=uVP*vec4(aP,1.0); }
)";
static const char* FS3 = R"(#version 330 core
in vec3 vN; in vec4 vC; in vec3 vW;
uniform vec3 uEye; uniform vec3 uSun; uniform vec3 uFog; uniform vec2 uFogRange; uniform float uSunPower;
out vec4 oC;
void main(){
  vec3 n=normalize(vN);
  float d=max(dot(n,-uSun),0.0);
  vec3 amb=mix(vec3(0.34,0.33,0.30),vec3(0.50,0.58,0.72),n.y*0.5+0.5);
  vec3 lit=vC.rgb*(amb+vec3(1.0,0.93,0.80)*d*uSunPower);
  lit=mix(lit,vC.rgb*1.25,clamp(vC.a,0.0,1.0));
  float fd=length(vW-uEye);
  float f=clamp((fd-uFogRange.x)/(uFogRange.y-uFogRange.x),0.0,1.0);
  oC=vec4(mix(lit,uFog,f*f),1.0);
}
)";
static const char* VS2 = R"(#version 330 core
layout(location=0) in vec2 aP;
layout(location=1) in vec4 aC;
uniform vec2 uRes;
out vec4 vC;
void main(){ vC=aC; gl_Position=vec4(aP.x/uRes.x*2.0-1.0, 1.0-aP.y/uRes.y*2.0, 0.0, 1.0); }
)";
static const char* FS2 = R"(#version 330 core
in vec4 vC; out vec4 oC;
void main(){ oC=vC; }
)";

unsigned Renderer::compile(unsigned type, const char* src) {
    unsigned s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    int ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, 2048, nullptr, log);
        std::fprintf(stderr, "Shader compile error: %s\n", log);
    }
    return s;
}
unsigned Renderer::link(const char* vs, const char* fs) {
    unsigned p = glCreateProgram();
    unsigned a = compile(GL_VERTEX_SHADER, vs), b = compile(GL_FRAGMENT_SHADER, fs);
    glAttachShader(p, a);
    glAttachShader(p, b);
    glLinkProgram(p);
    int ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(p, 2048, nullptr, log);
        std::fprintf(stderr, "Program link error: %s\n", log);
    }
    glDeleteShader(a);
    glDeleteShader(b);
    return p;
}
void Renderer::setupVertexLayout3D() {
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vert), (void*)offsetof(Vert, p));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vert), (void*)offsetof(Vert, n));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vert), (void*)offsetof(Vert, c));
}

bool Renderer::init(int w, int h) {
    W = w; H = h;
    prog3 = link(VS3, FS3);
    prog2 = link(VS2, FS2);
    glGenVertexArrays(1, &vaoS); glGenBuffers(1, &vboS);
    glBindVertexArray(vaoS); glBindBuffer(GL_ARRAY_BUFFER, vboS); setupVertexLayout3D();
    glGenVertexArrays(1, &vaoD); glGenBuffers(1, &vboD);
    glBindVertexArray(vaoD); glBindBuffer(GL_ARRAY_BUFFER, vboD); setupVertexLayout3D();
    glGenVertexArrays(1, &vao2); glGenBuffers(1, &vbo2);
    glBindVertexArray(vao2); glBindBuffer(GL_ARRAY_BUFFER, vbo2);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(V2), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(V2), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
    return prog3 && prog2;
}
void Renderer::resize(int w, int h) { W = w; H = h; glViewport(0, 0, w, h); }

void Renderer::uploadStatic(const std::vector<Vert>& verts) {
    glBindVertexArray(vaoS);
    glBindBuffer(GL_ARRAY_BUFFER, vboS);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vert), verts.data(), GL_STATIC_DRAW);
    staticCount = (int)verts.size();
}

void Renderer::beginFrame() {
    glViewport(0, 0, W, H);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.55f, 0.70f, 0.88f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    b2.clear();
}

void Renderer::draw3D(const Mat4& view, const Mat4& proj, V3 eye, const std::vector<Vert>& dyn,
                      Col fog, float fogNear, float fogFar, float sunPower) {
    flush2D();  // sky gradient queued before 3D
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glUseProgram(prog3);
    Mat4 vp = mMul(proj, view);
    glUniformMatrix4fv(glGetUniformLocation(prog3, "uVP"), 1, GL_FALSE, vp.m);
    glUniform3f(glGetUniformLocation(prog3, "uEye"), eye.x, eye.y, eye.z);
    V3 sun = norm({-0.45f, -0.75f, -0.35f});
    glUniform3f(glGetUniformLocation(prog3, "uSun"), sun.x, sun.y, sun.z);
    glUniform3f(glGetUniformLocation(prog3, "uFog"), fog.r, fog.g, fog.b);
    glUniform2f(glGetUniformLocation(prog3, "uFogRange"), fogNear, fogFar);
    glUniform1f(glGetUniformLocation(prog3, "uSunPower"), sunPower);
    glBindVertexArray(vaoS);
    glDrawArrays(GL_TRIANGLES, 0, staticCount);
    if (!dyn.empty()) {
        glBindVertexArray(vaoD);
        glBindBuffer(GL_ARRAY_BUFFER, vboD);
        glBufferData(GL_ARRAY_BUFFER, dyn.size() * sizeof(Vert), dyn.data(), GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)dyn.size());
    }
    glBindVertexArray(0);
    glDisable(GL_DEPTH_TEST);
}

// ---------------------------------------------------------------- 2D
void Renderer::flush2D() {
    if (b2.empty()) return;
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(prog2);
    glUniform2f(glGetUniformLocation(prog2, "uRes"), (float)W, (float)H);
    glBindVertexArray(vao2);
    glBindBuffer(GL_ARRAY_BUFFER, vbo2);
    glBufferData(GL_ARRAY_BUFFER, b2.size() * sizeof(V2), b2.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)b2.size());
    glBindVertexArray(0);
    b2.clear();
}
void Renderer::setScissor(float x, float y, float w, float h) {
    flush2D();
    glEnable(GL_SCISSOR_TEST);
    glScissor((int)x, (int)(H - y - h), (int)w, (int)h);
}
void Renderer::clearScissor() { flush2D(); glDisable(GL_SCISSOR_TEST); }

void Renderer::tri(float x0, float y0, float x1, float y1, float x2, float y2, Col c) {
    b2.push_back({x0, y0, c.r, c.g, c.b, c.a});
    b2.push_back({x1, y1, c.r, c.g, c.b, c.a});
    b2.push_back({x2, y2, c.r, c.g, c.b, c.a});
}
void Renderer::quad(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, Col c) {
    tri(x0, y0, x1, y1, x2, y2, c);
    tri(x0, y0, x2, y2, x3, y3, c);
}
void Renderer::rect(float x, float y, float w, float h, Col c) { quad(x, y, x + w, y, x + w, y + h, x, y + h, c); }
void Renderer::gradRect(float x, float y, float w, float h, Col t, Col b) {
    b2.push_back({x, y, t.r, t.g, t.b, t.a});
    b2.push_back({x + w, y, t.r, t.g, t.b, t.a});
    b2.push_back({x + w, y + h, b.r, b.g, b.b, b.a});
    b2.push_back({x, y, t.r, t.g, t.b, t.a});
    b2.push_back({x + w, y + h, b.r, b.g, b.b, b.a});
    b2.push_back({x, y + h, b.r, b.g, b.b, b.a});
}
void Renderer::line(float x0, float y0, float x1, float y1, float th, Col c) {
    float dx = x1 - x0, dy = y1 - y0, l = std::sqrt(dx * dx + dy * dy);
    if (l < 1e-4f) return;
    float nx = -dy / l * th * 0.5f, ny = dx / l * th * 0.5f;
    quad(x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny, c);
}
void Renderer::disc(float cx, float cy, float r, Col c, int seg) {
    for (int i = 0; i < seg; i++) {
        float a0 = i * 2 * PI_F / seg, a1 = (i + 1) * 2 * PI_F / seg;
        tri(cx, cy, cx + std::cos(a0) * r, cy + std::sin(a0) * r, cx + std::cos(a1) * r, cy + std::sin(a1) * r, c);
    }
}

// 5x7 bitmap font, 7 rows per glyph, bit 4 = leftmost pixel
static const char* FONT_CHARS = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ.,:!?$+-/%()'=_[]";
static const unsigned char FONT[53][7] = {
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},{0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},{0x0E,0x11,0x01,0x02,0x04,0x08,0x1F},
    {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E},{0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},{0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},{0x1F,0x01,0x02,0x04,0x08,0x08,0x08},{0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},
    {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},
    {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},{0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},{0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},
    {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E},{0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},{0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
    {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F},{0x11,0x11,0x11,0x1F,0x11,0x11,0x11},{0x0E,0x04,0x04,0x04,0x04,0x04,0x0E},
    {0x07,0x02,0x02,0x02,0x02,0x12,0x0C},{0x11,0x12,0x14,0x18,0x14,0x12,0x11},{0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
    {0x11,0x1B,0x15,0x15,0x11,0x11,0x11},{0x11,0x11,0x19,0x15,0x13,0x11,0x11},{0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},
    {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},{0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},{0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
    {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E},{0x1F,0x04,0x04,0x04,0x04,0x04,0x04},{0x11,0x11,0x11,0x11,0x11,0x11,0x0E},
    {0x11,0x11,0x11,0x11,0x11,0x0A,0x04},{0x11,0x11,0x11,0x15,0x15,0x15,0x0A},{0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
    {0x11,0x11,0x0A,0x04,0x04,0x04,0x04},{0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C},{0x00,0x00,0x00,0x00,0x0C,0x04,0x08},{0x00,0x0C,0x0C,0x00,0x0C,0x0C,0x00},
    {0x04,0x04,0x04,0x04,0x04,0x00,0x04},{0x0E,0x11,0x01,0x02,0x04,0x00,0x04},{0x04,0x0F,0x14,0x0E,0x05,0x1E,0x04},
    {0x00,0x04,0x04,0x1F,0x04,0x04,0x00},{0x00,0x00,0x00,0x1F,0x00,0x00,0x00},{0x01,0x01,0x02,0x04,0x08,0x10,0x10},
    {0x18,0x19,0x02,0x04,0x08,0x13,0x03},{0x02,0x04,0x08,0x08,0x08,0x04,0x02},{0x08,0x04,0x02,0x02,0x02,0x04,0x08},
    {0x04,0x04,0x08,0x00,0x00,0x00,0x00},{0x00,0x00,0x1F,0x00,0x1F,0x00,0x00},{0x00,0x00,0x00,0x00,0x00,0x00,0x1F},
    {0x0E,0x08,0x08,0x08,0x08,0x08,0x0E},{0x0E,0x02,0x02,0x02,0x02,0x02,0x0E}};

float Renderer::textWidth(const char* s, float scale) const { return (float)std::strlen(s) * 6.0f * scale; }

void Renderer::text(float x, float y, float sc, const char* s, Col c, bool shadow) {
    if (shadow) text(x + sc, y + sc, sc, s, {0, 0, 0, c.a * 0.7f}, false);
    float cx = x;
    for (const char* p = s; *p; p++) {
        char ch = (char)std::toupper((unsigned char)*p);
        if (ch != ' ') {
            const char* f = std::strchr(FONT_CHARS, ch);
            if (f) {
                const unsigned char* g = FONT[f - FONT_CHARS];
                for (int r = 0; r < 7; r++)
                    for (int col = 0; col < 5; col++)
                        if (g[r] & (1 << (4 - col))) rect(cx + col * sc, y + r * sc, sc, sc, c);
            }
        }
        cx += 6 * sc;
    }
}
