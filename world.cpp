#include "world.h"
#include <cstdio>
#include <cstring>
#include <queue>

namespace {
const Col C_GRASS = RGB(0x5a8d3d), C_ASPHALT = RGB(0x33363b), C_DIRT = RGB(0x8d6e4a), C_SIDEWALK = RGB(0xb9b8b0),
          C_LOT = RGB(0x9c9b94), C_YELLOW = RGB(0xe8c43a), C_WHITE = RGB(0xe9e9e4), C_GLASS = RGB(0x2b4766),
          C_TRUNK = RGB(0x5b3d24), C_LEAF = RGB(0x2f6b34), C_LEAF2 = RGB(0x3d7f3a), C_STEEL = RGB(0x6b7078);
const float ROAD_H = 0.06f, SIDE_H = 0.10f, LOT_H = 0.12f, FLOOR_H = 0.13f;

struct Frame {
    V3 c; float yaw, cs, sn;
    Frame(V3 c_, float yaw_) : c(c_), yaw(yaw_), cs(std::cos(yaw_)), sn(std::sin(yaw_)) {}
    V3 w(float lx, float ly, float lz) const { return {c.x + lx * cs + lz * sn, ly, c.z - lx * sn + lz * cs}; }
};
float pointSegDist(float px, float pz, V3 a, V3 b) {
    float dx = b.x - a.x, dz = b.z - a.z, l2 = dx * dx + dz * dz;
    float t = l2 > 0 ? clampf(((px - a.x) * dx + (pz - a.z) * dz) / l2, 0, 1) : 0;
    float qx = a.x + dx * t - px, qz = a.z + dz * t - pz;
    return std::sqrt(qx * qx + qz * qz);
}
}  // namespace

// ---------------------------------------------------------------- primitives
void World::addSolid(V3 mn, V3 mx) { solids.push_back({mn, mx}); }
void World::addBoxMin(V3 mn, V3 mx, Col c, bool solid) {
    V3 ctr = (mn + mx) * 0.5f, h = (mx - mn) * 0.5f;
    statics.box(ctr, h, 0, c);
    if (solid) addSolid(mn, mx);
}
static void fbox(World& w, MeshBuilder& mb, const Frame& f, float x0, float x1, float y0, float y1, float z0, float z1,
                 Col c, bool solid, std::vector<AABB>& solids) {
    (void)w;
    V3 a = f.w(x0, y0, z0), b = f.w(x1, y1, z1);
    V3 mn{std::min(a.x, b.x), y0, std::min(a.z, b.z)}, mx{std::max(a.x, b.x), y1, std::max(a.z, b.z)};
    mb.box((mn + mx) * 0.5f, (mx - mn) * 0.5f, 0, c);
    if (solid) solids.push_back({mn, mx});
}
#define FB(x0, x1, y0, y1, z0, z1, col, solid) fbox(*this, statics, F, x0, x1, y0, y1, z0, z1, col, solid, solids)

int World::addNode(float x, float z, bool signal) {
    RoadNode n; n.p = {x, 0, z}; n.signal = signal;
    nodes.push_back(n);
    return (int)nodes.size() - 1;
}
void World::addEdge(int a, int b, float hw, bool rural, bool dirt) {
    RoadEdge e; e.a = a; e.b = b; e.hw = hw; e.rural = rural;
    V3 d = nodes[b].p - nodes[a].p;
    e.length = len2d(d); e.dir = norm2d(d);
    edges.push_back(e);
    int id = (int)edges.size() - 1;
    nodes[a].edges.push_back(id);
    nodes[b].edges.push_back(id);
    mapRoads.push_back({nodes[a].p, nodes[b].p, hw, dirt ? RGB(0xb59a6a) : (rural ? RGB(0xd8d4c8) : RGB(0xe6e3d8))});
}

// ---------------------------------------------------------------- roads
void World::roadStrip(V3 a, V3 b, float hw, Col c, bool dashed, bool edgeLines) {
    bool alongX = std::fabs(b.x - a.x) > std::fabs(b.z - a.z);
    V3 mn{std::min(a.x, b.x), 0, std::min(a.z, b.z)}, mx{std::max(a.x, b.x), ROAD_H, std::max(a.z, b.z)};
    if (alongX) { mn.z -= hw; mx.z += hw; } else { mn.x -= hw; mx.x += hw; }
    addBoxMin(mn, mx, c, false);
    float L = alongX ? (mx.x - mn.x) : (mx.z - mn.z);
    if (edgeLines) {
        for (int s = -1; s <= 1; s += 2) {
            float off = s * (hw - 0.4f);
            if (alongX) addBoxMin({mn.x, ROAD_H, a.z + off - 0.08f}, {mx.x, ROAD_H + 0.012f, a.z + off + 0.08f}, C_WHITE, false);
            else addBoxMin({a.x + off - 0.08f, ROAD_H, mn.z}, {a.x + off + 0.08f, ROAD_H + 0.012f, mx.z}, C_WHITE, false);
        }
    }
    if (dashed) {
        for (float t = 2; t < L - 2; t += 8) {
            if (alongX) addBoxMin({mn.x + t, ROAD_H, a.z - 0.12f}, {mn.x + t + 3.5f, ROAD_H + 0.012f, a.z + 0.12f}, C_YELLOW, false);
            else addBoxMin({a.x - 0.12f, ROAD_H, mn.z + t}, {a.x + 0.12f, ROAD_H + 0.012f, mn.z + t + 3.5f}, C_YELLOW, false);
        }
    }
}

void World::lampPost(float x, float z, float yawToRoad) {
    V3 base{x, 0, z};
    statics.box({x, 3.3f, z}, {0.11f, 3.3f, 0.11f}, 0, C_STEEL);
    V3 d = fwdOf(yawToRoad);
    V3 top{x, 6.6f, z};
    statics.beam(top, top + d * 1.8f, 0.07f, C_STEEL);
    statics.box(top + d * 1.9f + V3{0, -0.08f, 0}, {0.35f, 0.08f, 0.2f}, yawToRoad, RGB(0xfff2c0, 0.9f));
    addSolid({x - 0.2f, 0, z - 0.2f}, {x + 0.2f, 6.6f, z + 0.2f});
    lampPosts.push_back(base);
}

void World::buildRoads() {
    const float XS[5] = {-160, -80, 0, 80, 160};
    // grass with large tonal patches
    addBoxMin({-1500, -2, -1500}, {1500, 0, 1500}, C_GRASS, false);
    for (int i = 0; i < 46; i++) {
        float cx = rng.range(-700, 900), cz = rng.range(-500, 900), hx = rng.range(25, 110), hz = rng.range(25, 110);
        Col c = rng.chance(0.5f) ? RGB(0x52853a) : RGB(0x659a45);
        addBoxMin({cx - hx, 0, cz - hz}, {cx + hx, 0.02f, cz + hz}, c, false);
    }
    // city grid
    for (int i = 0; i < 5; i++) {
        roadStrip({XS[i], 0, -CITY_HALF}, {XS[i], 0, CITY_HALF}, 7, C_ASPHALT, false, false);
        roadStrip({-CITY_HALF, 0, XS[i]}, {CITY_HALF, 0, XS[i]}, 7, C_ASPHALT, false, false);
    }
    for (int i = 0; i < 5; i++)
        for (float t = -CITY_HALF + 2; t < CITY_HALF - 4; t += 8) {
            float nearest = 1e9f;
            for (int k = 0; k < 5; k++) nearest = std::min(nearest, std::fabs(t + 1.75f - XS[k]));
            if (nearest < 10) continue;
            addBoxMin({XS[i] - 0.12f, ROAD_H, t}, {XS[i] + 0.12f, ROAD_H + 0.012f, t + 3.5f}, C_YELLOW, false);
            addBoxMin({t, ROAD_H, XS[i] - 0.12f}, {t + 3.5f, ROAD_H + 0.012f, XS[i] + 0.12f}, C_YELLOW, false);
        }
    // crosswalks and stop lines
    for (int i = 0; i < 5; i++)
        for (int j = 0; j < 5; j++)
            for (int arm = 0; arm < 4; arm++) {
                int dx = arm == 0 ? 1 : arm == 1 ? -1 : 0, dz = arm == 2 ? 1 : arm == 3 ? -1 : 0;
                float cx = XS[i] + dx * 9.5f, cz = XS[j] + dz * 9.5f;
                if (std::fabs(cx) > CITY_HALF - 8 || std::fabs(cz) > CITY_HALF - 8) {
                    if (!((i == 4 && j == 2 && dx == 1) || (i == 2 && j == 4 && dz == 1))) continue;  // keep highway arms
                    if (std::fabs(cx) > CITY_HALF + 1 || std::fabs(cz) > CITY_HALF + 1) {}
                }
                for (int s = -3; s <= 3; s++) {
                    float off = s * 1.8f;
                    if (dx) addBoxMin({cx - 1.5f, ROAD_H, XS[j] + off - 0.4f}, {cx + 1.5f, ROAD_H + 0.014f, XS[j] + off + 0.4f}, C_WHITE, false);
                    else addBoxMin({XS[i] + off - 0.4f, ROAD_H, cz - 1.5f}, {XS[i] + off + 0.4f, ROAD_H + 0.014f, cz + 1.5f}, C_WHITE, false);
                }
            }
    // rural highways and village streets
    roadStrip({160, 0, 0}, {700, 0, 0}, 4, C_ASPHALT, true, true);
    roadStrip({0, 0, 160}, {0, 0, 640}, 4, C_ASPHALT, true, true);
    roadStrip({-95, 0, 640}, {95, 0, 640}, 4, C_ASPHALT, true, true);
    roadStrip({620, 0, -62}, {620, 0, 62}, 3, C_DIRT, false, false);
    roadStrip({-60, 0, 640}, {-60, 0, 702}, 3, C_DIRT, false, false);
    roadStrip({60, 0, 640}, {60, 0, 702}, 3, C_DIRT, false, false);
}

// ---------------------------------------------------------------- buildings
void World::building(float cx, float cz, float w, float d, float h, Col c, bool windows, int roofKind) {
    addBoxMin({cx - w / 2, 0, cz - d / 2}, {cx + w / 2, h, cz + d / 2}, c, true);
    mapRects.push_back({cx - w / 2, cz - d / 2, w, d, shade(c, 0.8f)});
    addBoxMin({cx - w / 2 - 0.15f, h, cz - d / 2 - 0.15f}, {cx + w / 2 + 0.15f, h + 0.3f, cz + d / 2 + 0.15f}, shade(c, 0.7f), false);
    if (windows) {
        int floors = std::max(1, (int)(h / 3.4f));
        for (int f = 0; f < floors; f++) {
            float y = 1.0f + f * 3.4f;
            if (y + 1.6f > h) break;
            bool lit = rng.chance(0.22f);
            Col wc = lit ? RGB(0xffd98a, 0.75f) : C_GLASS;
            statics.box({cx, y + 0.8f, cz + d / 2 + 0.04f}, {w / 2 - 1.0f, 0.7f, 0.04f}, 0, wc);
            statics.box({cx, y + 0.8f, cz - d / 2 - 0.04f}, {w / 2 - 1.0f, 0.7f, 0.04f}, 0, wc);
            statics.box({cx + w / 2 + 0.04f, y + 0.8f, cz}, {0.04f, 0.7f, d / 2 - 1.0f}, 0, wc);
            statics.box({cx - w / 2 - 0.04f, y + 0.8f, cz}, {0.04f, 0.7f, d / 2 - 1.0f}, 0, wc);
        }
    }
    if (roofKind == 1) {  // rooftop units
        statics.box({cx + w * 0.2f, h + 0.9f, cz - d * 0.2f}, {1.4f, 0.6f, 1.0f}, 0, RGB(0x8a8f96));
        statics.box({cx - w * 0.25f, h + 0.7f, cz + d * 0.25f}, {0.9f, 0.4f, 0.9f}, 0, RGB(0x777c83));
    }
}

void World::midRise(float cx, float cz, float w, float d, float h, Col c) {
    building(cx, cz, w, d, h, c, true, 1);
    addBoxMin({cx - w / 2 - 0.6f, 0, cz - d / 2 - 0.6f}, {cx + w / 2 + 0.6f, 4.2f, cz + d / 2 + 0.6f}, shade(c, 0.55f), true);
    statics.beam({cx, h + 0.3f, cz}, {cx, h + 9.0f, cz}, 0.1f, RGB(0xc8c8c8));
    statics.box({cx, h + 9.0f, cz}, {0.25f, 0.25f, 0.25f}, 0, RGB(0xff3030, 1.0f));
    statics.box({cx, h + 1.4f, cz}, {w * 0.3f, 1.1f, d * 0.3f}, 0, shade(c, 0.85f));
}

void World::house(float cx, float cz, float yaw, Col wall, Col roof) {
    Frame F({cx, 0, cz}, yaw);
    FB(-4.5f, 4.5f, 0, 3.6f, -4, 4, wall, true);
    FB(-3.9f, 3.9f, 3.6f, 4.0f, -4, 4, wall, false);
    FB(-2.6f, 2.6f, 4.0f, 4.8f, -4, 4, wall, false);
    FB(-1.3f, 1.3f, 4.8f, 5.5f, -4, 4, wall, false);
    FB(-0.55f, 0.55f, 0, 2.1f, 4.0f, 4.07f, RGB(0x6b4426), false);
    FB(-3.4f, -1.8f, 1.1f, 2.3f, 4.0f, 4.06f, RGB(0x9fc8e8, 0.2f), false);
    FB(1.8f, 3.4f, 1.1f, 2.3f, 4.0f, 4.06f, RGB(0x9fc8e8, 0.2f), false);
    FB(-4.7f, 4.7f, 0, 0.5f, -4.2f, 4.2f, shade(wall, 0.8f), false);
    Mat4 base = mMul(mTranslate({cx, 0, cz}), mRotY(yaw));
    statics.boxM(mMul(base, mMul(mTranslate({2.4f, 4.9f, 0}), mRotZ(-0.5f))), {2.8f, 0.12f, 4.6f}, roof);
    statics.boxM(mMul(base, mMul(mTranslate({-2.4f, 4.9f, 0}), mRotZ(0.5f))), {2.8f, 0.12f, 4.6f}, roof);
    FB(2.0f, 2.8f, 5.0f, 7.0f, -2.5f, -1.7f, RGB(0x8c5a44), false);
    mapRects.push_back({cx - 4.5f, cz - 4, 9, 8, RGB(0xb08a7a)});
}

void World::tree(float x, float z, float scale) {
    float yaw = rng.range(0, 6.28f);
    statics.box({x, 1.6f * scale, z}, {0.3f * scale, 1.6f * scale, 0.3f * scale}, 0, C_TRUNK);
    statics.box({x, 4.0f * scale, z}, {2.3f * scale, 1.4f * scale, 2.3f * scale}, yaw, C_LEAF);
    statics.box({x, 5.9f * scale, z}, {1.5f * scale, 1.2f * scale, 1.5f * scale}, yaw + 0.8f, C_LEAF2);
    addSolid({x - 0.3f * scale, 0, z - 0.3f * scale}, {x + 0.3f * scale, 3.2f * scale, z + 0.3f * scale});
}

void World::parkedCar(float x, float z, float yaw) { parked.push_back({{x, GROUND, z}, yaw}); }

// ---------------------------------------------------------------- interiors
void World::store(float cx, float cz, float yaw, const std::string& name) {
    Frame F({cx, 0, cz}, yaw);
    const float H = 4.2f, T = 0.35f;
    Col wall = RGB(0xe4e0d4), trim = RGB(0x1f8a4c);
    FB(-7, 7, 0, FLOOR_H, -5, 5, RGB(0xe8e6e0), false);
    FB(-7, 7, 0, H, -5, -5 + T, wall, true);
    FB(-7, -7 + T, 0, H, -5, 5, wall, true);
    FB(7 - T, 7, 0, H, -5, 5, wall, true);
    FB(-7, -1.6f, 0, H, 5 - T, 5, wall, true);
    FB(1.6f, 7, 0, H, 5 - T, 5, wall, true);
    FB(-1.6f, 1.6f, 2.7f, H, 5 - T, 5, wall, true);
    FB(-7.2f, 7.2f, H, H + 0.3f, -5.2f, 5.2f, shade(trim, 0.8f), true);
    FB(-4.5f, 4.5f, H - 1.1f, H - 0.2f, 5.0f, 5.18f, RGB(0x33e07a, 0.9f), false);
    FB(-4.5f, 4.5f, H - 0.35f, H - 0.2f, 5.18f, 5.22f, RGB(0xff4040, 0.9f), false);
    FB(-6.5f, -2.0f, 0.9f, 2.4f, 5.0f, 5.06f, RGB(0xaed8f0, 0.25f), false);
    FB(2.0f, 6.5f, 0.9f, 2.4f, 5.0f, 5.06f, RGB(0xaed8f0, 0.25f), false);
    FB(1.0f, 6.0f, 0, 1.1f, -2.4f, -1.5f, RGB(0x8a5a35), true);       // counter
    FB(4.6f, 5.4f, 1.1f, 1.45f, -2.2f, -1.7f, RGB(0x222428), false);  // register
    const Col prod[4] = {RGB(0xe0453a), RGB(0xf2c230), RGB(0x3a8ee0), RGB(0x6ac060)};
    for (int r = 0; r < 2; r++) {
        float z0 = r == 0 ? -3.6f : -0.2f;
        FB(-6.2f, -1.6f, 0, 1.8f, z0, z0 + 0.7f, RGB(0xb8bcc2), true);
        for (int k = 0; k < 6; k++) FB(-6.0f + k * 0.75f, -5.4f + k * 0.75f, 1.0f + (k % 3) * 0.25f, 1.7f, z0 + 0.05f, z0 + 0.65f, prod[(k + r) % 4], false);
    }
    FB(5.7f, 6.65f, 0, 2.0f, 0.4f, 3.6f, RGB(0xcfe7f2, 0.3f), true);   // fridge
    Interior in;
    in.kind = Interior::STORE; in.name = name;
    V3 a = F.w(-7, 0, -5), b = F.w(7, 0, 5);
    in.bounds = {{std::min(a.x, b.x), 0, std::min(a.z, b.z)}, {std::max(a.x, b.x), H, std::max(a.z, b.z)}};
    in.registerPos = F.w(4.2f, GROUND, -0.6f);
    in.clerkPos[0] = F.w(4.2f, GROUND, -3.3f);
    in.clerkCount = 1; in.clerkYaw = yaw;
    in.doorPos = F.w(0, GROUND, 6.5f);
    interiors.push_back(in);
    mapRects.push_back({in.bounds.mn.x, in.bounds.mn.z, in.bounds.mx.x - in.bounds.mn.x, in.bounds.mx.z - in.bounds.mn.z, RGB(0x6fd0a0)});
}

void World::bank(float cx, float cz, float yaw) {
    Frame F({cx, 0, cz}, yaw);
    const float H = 9.0f, T = 0.5f;
    Col stone = RGB(0xcdbd9c), dark = RGB(0x9d8d70), marble = RGB(0xdcd6c8);
    FB(-15, 15, 0, FLOOR_H, -13, 13, marble, false);
    FB(-15, 15, 0, H, -13, -13 + T, stone, true);
    FB(-15, -15 + T, 0, H, -13, 13, stone, true);
    FB(15 - T, 15, 0, H, -13, 13, stone, true);
    FB(-15, -2.5f, 0, H, 13 - T, 13, stone, true);
    FB(2.5f, 15, 0, H, 13 - T, 13, stone, true);
    FB(-2.5f, 2.5f, 3.4f, H, 13 - T, 13, stone, true);
    FB(-15.6f, 15.6f, H, H + 0.6f, -13.6f, 13.6f, dark, true);
    FB(-8, 8, H + 0.6f, H + 1.6f, 12.0f, 13.5f, dark, false);
    for (int k = 0; k < 6; k++) {  // columns
        float x = (k < 3 ? -1 : 1) * (4.5f + (k % 3) * 3.6f);
        FB(x - 0.45f, x + 0.45f, 0, H, 13.1f, 14.0f, marble, true);
    }
    FB(-13, 13, 0, 0.3f, 13.1f, 16.0f, dark, true);  // steps
    for (int w = 0; w < 2; w++) {  // big front windows
        float x0 = w == 0 ? -13.0f : 8.0f;
        FB(x0, x0 + 5.0f, 1.2f, 5.5f, 13.0f, 13.06f, RGB(0x9fc4e0, 0.25f), false);
    }
    // 3D letters "BANK" over the door
    const char* rows[4][5] = {{"11110", "10001", "11110", "10001", "11110"}, {"01110", "10001", "11111", "10001", "10001"},
                              {"10001", "11001", "10101", "10011", "10001"}, {"10010", "10100", "11000", "10100", "10010"}};
    for (int L = 0; L < 4; L++)
        for (int r = 0; r < 5; r++)
            for (int c = 0; c < 5; c++)
                if (rows[L][r][c] == '1') {
                    float x = -6.6f + L * 3.5f + c * 0.55f;
                    FB(x, x + 0.5f, 5.0f + (4 - r) * 0.55f, 5.5f + (4 - r) * 0.55f, 13.0f, 13.3f, RGB(0xf0c040, 1.0f), false);
                }
    // staff area / teller counter
    FB(-12, -2.8f, 0, 1.15f, 3.5f, 4.4f, RGB(0x5a3d28), true);
    FB(2.8f, 12, 0, 1.15f, 3.5f, 4.4f, RGB(0x5a3d28), true);
    FB(-12, -2.8f, 1.15f, 2.3f, 3.9f, 4.0f, RGB(0x9fd0f0, 0.3f), true);
    FB(2.8f, 12, 1.15f, 2.3f, 3.9f, 4.0f, RGB(0x9fd0f0, 0.3f), true);
    // vault partition with doorway, open vault door, safe deposit wall
    FB(-15, -2.5f, 0, H, -4.75f, -4.25f, stone, true);
    FB(2.5f, 15, 0, H, -4.75f, -4.25f, stone, true);
    FB(-2.5f, 2.5f, 3.3f, H, -4.75f, -4.25f, stone, true);
    FB(-4.9f, -2.7f, 0, 3.2f, -5.2f, -3.9f, C_STEEL, true);
    FB(-14, 14, 0, 3.4f, -12.8f, -12.1f, RGB(0x8a9099), true);
    for (int k = 0; k < 9; k++) FB(-12.5f + k * 3.0f, -11.5f + k * 3.0f, 0.2f, 0.9f, -12.0f, -11.5f, RGB(0xf2c53a, 0.5f), false);
    FB(-4, 4, 0, 1.2f, -9.8f, -8.6f, RGB(0x70757d), true);   // cash cart
    FB(-9, -6, 0, 0.9f, 7.0f, 7.5f, RGB(0x4a2f20), true);     // lobby benches
    FB(6, 9, 0, 0.9f, 7.0f, 7.5f, RGB(0x4a2f20), true);
    Interior in;
    in.kind = Interior::BANK; in.name = "DOWNTOWN BANK";
    V3 a = F.w(-15, 0, -13), b = F.w(15, 0, 13);
    in.bounds = {{std::min(a.x, b.x), 0, std::min(a.z, b.z)}, {std::max(a.x, b.x), H, std::max(a.z, b.z)}};
    in.registerPos = F.w(0, GROUND, -9.0f);
    in.clerkPos[0] = F.w(-6.5f, GROUND, 2.2f);
    in.clerkPos[1] = F.w(6.5f, GROUND, 2.2f);
    in.clerkCount = 2; in.clerkYaw = yaw;
    in.doorPos = F.w(0, GROUND, 16.5f);
    interiors.push_back(in);
    mapRects.push_back({in.bounds.mn.x, in.bounds.mn.z, in.bounds.mx.x - in.bounds.mn.x, in.bounds.mx.z - in.bounds.mn.z, RGB(0xf0c040)});
}

// ---------------------------------------------------------------- city
void World::buildCity() {
    auto bc = [](int i) { return -120.0f + 80.0f * i; };
    const Col pastel[6] = {RGB(0xe8c9b0), RGB(0xc9d8e8), RGB(0xe8e0b0), RGB(0xd8b8c8), RGB(0xb8d8c0), RGB(0xe0d8d0)};
    const Col commercial[6] = {RGB(0xa86a52), RGB(0x8b95a1), RGB(0xb9a68a), RGB(0x7d8fa6), RGB(0xa7a39a), RGB(0x9b6b6b)};
    struct StoreDef { int i, j; float yaw; const char* name; };
    const StoreDef stores[3] = {{0, 2, PI_F / 2, "24/7 MART WEST"}, {3, 1, -PI_F / 2, "24/7 MART EAST"}, {1, 3, PI_F, "24/7 MART SOUTH"}};
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            float cx = bc(i), cz = bc(j);
            bool downtown = (i == 1 || i == 2) && (j == 1 || j == 2);
            addBoxMin({cx - 33, 0, cz - 33}, {cx + 33, SIDE_H, cz + 33}, C_SIDEWALK, false);
            addBoxMin({cx - 29, 0, cz - 29}, {cx + 29, LOT_H, cz + 29}, downtown ? C_LOT : RGB(0x8fae78), false);
            for (int sx = -1; sx <= 1; sx += 2)
                for (int sz = -1; sz <= 1; sz += 2) {
                    float lx = cx + sx * 31.0f, lz = cz + sz * 31.0f;
                    lampPost(lx, lz, std::atan2(sx * 1.0f, sz * 1.0f));
                }
            if (i == 1 && j == 1) {
                midRise(cx - 12, cz - 12, 26, 26, 62, RGB(0x5d7fa3));
                midRise(cx + 18, cz + 14, 20, 20, 46, RGB(0x8a96a6));
                building(cx - 12, cz + 18, 18, 18, 9, commercial[1], true, 1);
                building(cx + 18, cz - 12, 18, 18, 12, commercial[0], true, 1);
                continue;
            }
            if (i == 2 && j == 2) {
                bank(cx - 6, cz, -PI_F / 2);   // faces west toward the central avenue
                building(cx + 22, cz - 16, 16, 18, 11, commercial[2], true, 1);
                building(cx + 22, cz + 16, 16, 18, 8, commercial[4], true, 1);
                parkedCar(cx - 22, cz + 24, 0);
                continue;
            }
            const StoreDef* sd = nullptr;
            for (const auto& s : stores) if (s.i == i && s.j == j) sd = &s;
            V3 fdir{0, 0, 0};
            if (sd) fdir = fwdOf(sd->yaw);
            for (int sx = -1; sx <= 1; sx += 2)
                for (int sz = -1; sz <= 1; sz += 2) {
                    float scx = cx + sx * 15.0f, scz = cz + sz * 15.0f;
                    if (sd && (sx * fdir.x + sz * fdir.z) > 0) continue;  // facing half is the store forecourt
                    if (rng.chance(0.12f)) {  // pocket park
                        addBoxMin({scx - 13, LOT_H, scz - 13}, {scx + 13, LOT_H + 0.01f, scz + 13}, RGB(0x6fa05a), false);
                        tree(scx - 5, scz - 4, 1.0f); tree(scx + 6, scz + 5, 0.9f);
                        continue;
                    }
                    float w = rng.range(17, 25), d = rng.range(17, 25);
                    float h = downtown ? rng.range(10, 20) : (std::fabs(cx) < 130 && std::fabs(cz) < 130 ? rng.range(7, 13) : rng.range(5, 9));
                    Col c = downtown ? commercial[rng.i(6)] : (rng.chance(0.4f) ? commercial[rng.i(6)] : pastel[rng.i(6)]);
                    building(scx, scz, w, d, h, c, true, rng.chance(0.5f) ? 1 : 0);
                }
            if (sd) {
                V3 c = V3{cx, 0, cz} + fdir * 22.5f;
                store(c.x, c.z, sd->yaw, sd->name);
                V3 lat{-fdir.z, 0, fdir.x};
                parkedCar(c.x + fdir.x * 12 + lat.x * 17, c.z + fdir.z * 12 + lat.z * 17, sd->yaw + PI_F / 2);
                parkedCar(c.x + fdir.x * 12 - lat.x * 17, c.z + fdir.z * 12 - lat.z * 17, sd->yaw - PI_F / 2);
            }
        }
    // curb-side parked cars
    for (int i = 0; i < 5; i++)
        for (int k = 0; k < 4; k++) {
            if (!rng.chance(0.45f)) continue;
            float along = -120.0f + k * 80.0f + rng.range(-12, 12);
            float side = rng.chance(0.5f) ? 5.6f : -5.6f;
            float line = -160.0f + 80.0f * i;
            if (rng.chance(0.5f)) parkedCar(line + side, along, side > 0 ? PI_F : 0);
            else parkedCar(along, line + side, side > 0 ? -PI_F / 2 : PI_F / 2);
        }
}

// ---------------------------------------------------------------- villages
void World::buildVillages() {
    const Col walls[5] = {RGB(0xe9dcc0), RGB(0xc7d9e6), RGB(0xefe6a8), RGB(0xf0f0ec), RGB(0xd9b8a0)};
    const Col roofs[3] = {RGB(0x9b3b2e), RGB(0x6e4a34), RGB(0x55606b)};
    // Village 1 (east, Maple Creek) along z = 0
    float v1n[] = {584, 600, 658, 674, 690};
    for (float x : v1n) house(x, -17, 0, walls[rng.i(5)], roofs[rng.i(3)]);
    store(641, -16, 0, "MAPLE CREEK GENERAL");
    float v1s[] = {584, 600, 644, 660, 676, 692};
    for (float x : v1s) house(x, 17, PI_F, walls[rng.i(5)], roofs[rng.i(3)]);
    house(606, -42, PI_F / 2 * 1.0f, walls[1], roofs[0]);
    house(634, 44, -PI_F / 2, walls[2], roofs[1]);
    parkedCar(588, -10.5f, 0); parkedCar(662, 11.0f, PI_F); parkedCar(618.8f, -30, PI_F / 2); parkedCar(696, 11.0f, PI_F);
    // Village 2 (south, Pine Hollow) along z = 640
    float v2n[] = {-84, -42, -26, 44, 62, 84};
    for (float x : v2n) house(x, 623, 0, walls[rng.i(5)], roofs[rng.i(3)]);
    store(27, 624, 0, "PINE HOLLOW GROCERY");
    float v2s[] = {-84, -42, -26, 26, 44, 84};
    for (float x : v2s) house(x, 657, PI_F, walls[rng.i(5)], roofs[rng.i(3)]);
    house(-40, 686, PI_F / 2, walls[3], roofs[2]);
    house(40, 686, -PI_F / 2, walls[0], roofs[0]);
    parkedCar(-30, 635.5f, 0); parkedCar(-86, 644.5f, PI_F); parkedCar(48, 635.5f, 0); parkedCar(88, 644.5f, PI_F);
}

void World::buildTrees() {
    int placed = 0, tries = 0;
    while (placed < 420 && tries < 6000) {
        tries++;
        float x = rng.range(-430, 800), z = rng.range(-250, 820);
        if (std::fabs(x) < 185 && std::fabs(z) < 185) continue;
        if (x > 540 && x < 730 && z > -90 && z < 90) continue;
        if (x > -120 && x < 120 && z > 590 && z < 730) continue;
        if (nearRoad(x, z, 12)) continue;
        tree(x, z, rng.range(0.8f, 1.5f));
        placed++;
    }
    for (int k = 0; k < 40; k++) {  // roadside trees along the highways
        float x = rng.range(190, 540), z = (rng.chance(0.5f) ? 1 : -1) * rng.range(14, 40);
        if (!nearRoad(x, z, 10)) tree(x, z, rng.range(0.9f, 1.3f));
        float zz = rng.range(190, 600), xx = (rng.chance(0.5f) ? 1 : -1) * rng.range(14, 40);
        if (!nearRoad(xx, zz, 10)) tree(xx, zz, rng.range(0.9f, 1.3f));
    }
}

bool World::nearRoad(float x, float z, float margin) const {
    for (const auto& r : mapRoads)
        if (pointSegDist(x, z, r.a, r.b) < r.hw + margin) return true;
    return false;
}

// ---------------------------------------------------------------- graph
void World::buildGraph() {
    const float XS[5] = {-160, -80, 0, 80, 160};
    int id[5][5];
    for (int i = 0; i < 5; i++)
        for (int j = 0; j < 5; j++) id[i][j] = addNode(XS[i], XS[j], true);
    for (int i = 0; i < 5; i++)
        for (int j = 0; j < 5; j++) {
            if (i < 4) addEdge(id[i][j], id[i + 1][j], 7, false, false);
            if (j < 4) addEdge(id[i][j], id[i][j + 1], 7, false, false);
        }
    int h1a = addNode(560, 0), h1b = addNode(620, 0), h1c = addNode(700, 0), h1n = addNode(620, -60), h1s = addNode(620, 60);
    addEdge(id[4][2], h1a, 4, true, false); addEdge(h1a, h1b, 4, true, false); addEdge(h1b, h1c, 4, true, false);
    addEdge(h1b, h1n, 3, true, true); addEdge(h1b, h1s, 3, true, true);
    int v0 = addNode(-90, 640), v1 = addNode(-60, 640), v2 = addNode(0, 640), v3 = addNode(60, 640), v4 = addNode(90, 640);
    int s1 = addNode(-60, 700), s2 = addNode(60, 700);
    addEdge(id[2][4], v2, 4, true, false);
    addEdge(v0, v1, 4, true, false); addEdge(v1, v2, 4, true, false); addEdge(v2, v3, 4, true, false); addEdge(v3, v4, 4, true, false);
    addEdge(v1, s1, 3, true, true); addEdge(v3, s2, 3, true, true);
    int n = (int)nodes.size();
    nextHop.assign(n, std::vector<int>(n, -1));
    for (int t = 0; t < n; t++) {  // BFS rooted at each target
        std::vector<int> par(n, -2);
        std::queue<int> q;
        q.push(t); par[t] = t;
        while (!q.empty()) {
            int u = q.front(); q.pop();
            for (int e : nodes[u].edges) {
                int v = edges[e].a == u ? edges[e].b : edges[e].a;
                if (par[v] == -2) { par[v] = u; q.push(v); }
            }
        }
        for (int f = 0; f < n; f++) nextHop[f][t] = par[f] >= 0 ? par[f] : f;
    }
}

void World::buildGrid() {
    cells.assign(NXC * NZC, {});
    for (int i = 0; i < (int)solids.size(); i++) {
        const AABB& b = solids[i];
        int x0 = clampf((b.mn.x - OX) / CELL, 0, NXC - 1), x1 = clampf((b.mx.x - OX) / CELL, 0, NXC - 1);
        int z0 = clampf((b.mn.z - OZ) / CELL, 0, NZC - 1), z1 = clampf((b.mx.z - OZ) / CELL, 0, NZC - 1);
        for (int x = x0; x <= x1; x++)
            for (int z = z0; z <= z1; z++) cells[z * NXC + x].push_back(i);
    }
    stamp.assign(solids.size(), 0);
}

void World::build() {
    buildRoads();
    buildGraph();      // fills mapRoads (needed by tree placement)
    buildCity();
    buildVillages();
    buildTrees();
    buildGrid();
    std::printf("World: %zu static vertices, %zu colliders, %zu nodes, %zu interiors\n", statics.v.size(), solids.size(), nodes.size(), interiors.size());
}

// ---------------------------------------------------------------- queries
void World::query(float x0, float z0, float x1, float z1, std::vector<int>& out) const {
    out.clear();
    stampCur++;
    int cx0 = clampf((x0 - OX) / CELL, 0, NXC - 1), cx1 = clampf((x1 - OX) / CELL, 0, NXC - 1);
    int cz0 = clampf((z0 - OZ) / CELL, 0, NZC - 1), cz1 = clampf((z1 - OZ) / CELL, 0, NZC - 1);
    for (int x = cx0; x <= cx1; x++)
        for (int z = cz0; z <= cz1; z++)
            for (int id : cells[z * NXC + x])
                if (stamp[id] != stampCur) { stamp[id] = stampCur; out.push_back(id); }
}

V3 World::pushCircle(V3 p, float r, float y0, float y1, V3* hitN) const {
    static std::vector<int> tmp;
    query(p.x - r - 0.1f, p.z - r - 0.1f, p.x + r + 0.1f, p.z + r + 0.1f, tmp);
    V3 n{0, 0, 0};
    for (int id : tmp) {
        const AABB& b = solids[id];
        if (b.mx.y <= y0 || b.mn.y >= y1) continue;
        float cx = clampf(p.x, b.mn.x, b.mx.x), cz = clampf(p.z, b.mn.z, b.mx.z);
        float dx = p.x - cx, dz = p.z - cz, d2 = dx * dx + dz * dz;
        if (d2 >= r * r) continue;
        if (d2 > 1e-8f) {
            float d = std::sqrt(d2);
            p.x += dx / d * (r - d); p.z += dz / d * (r - d);
            n.x += dx / d; n.z += dz / d;
        } else {  // centre inside the box: leave through the nearest face
            float l = p.x - b.mn.x, rr = b.mx.x - p.x, t = p.z - b.mn.z, bt = b.mx.z - p.z;
            float m = std::min(std::min(l, rr), std::min(t, bt));
            if (m == l) { p.x = b.mn.x - r; n.x -= 1; }
            else if (m == rr) { p.x = b.mx.x + r; n.x += 1; }
            else if (m == t) { p.z = b.mn.z - r; n.z -= 1; }
            else { p.z = b.mx.z + r; n.z += 1; }
        }
    }
    if (hitN) *hitN = norm2d(n);
    return p;
}

float World::groundHeight(float x, float z, float feetY) const {
    static std::vector<int> tmp;
    query(x - 0.5f, z - 0.5f, x + 0.5f, z + 0.5f, tmp);
    float best = GROUND;
    for (int id : tmp) {
        const AABB& b = solids[id];
        if (x >= b.mn.x - 0.15f && x <= b.mx.x + 0.15f && z >= b.mn.z - 0.15f && z <= b.mx.z + 0.15f && b.mx.y <= feetY + 0.65f && b.mx.y > best)
            best = b.mx.y;
    }
    return best;
}

bool World::raycast(V3 o, V3 d, float maxT, float& tHit) const {
    static std::vector<int> tmp;
    V3 e = o + d * maxT;
    query(std::min(o.x, e.x) - 0.2f, std::min(o.z, e.z) - 0.2f, std::max(o.x, e.x) + 0.2f, std::max(o.z, e.z) + 0.2f, tmp);
    float best = maxT;
    bool hit = false;
    for (int id : tmp) {
        const AABB& b = solids[id];
        float t0 = 0, t1 = best;
        const float oo[3] = {o.x, o.y, o.z}, dd[3] = {d.x, d.y, d.z}, mn[3] = {b.mn.x, b.mn.y, b.mn.z}, mx[3] = {b.mx.x, b.mx.y, b.mx.z};
        bool ok = true;
        for (int a = 0; a < 3 && ok; a++) {
            if (std::fabs(dd[a]) < 1e-8f) { if (oo[a] < mn[a] || oo[a] > mx[a]) ok = false; }
            else {
                float ta = (mn[a] - oo[a]) / dd[a], tb = (mx[a] - oo[a]) / dd[a];
                if (ta > tb) std::swap(ta, tb);
                t0 = std::max(t0, ta); t1 = std::min(t1, tb);
                if (t0 > t1) ok = false;
            }
        }
        if (ok && t0 < best) { best = t0; hit = true; }
    }
    if (d.y < -1e-5f) {
        float tg = (GROUND - o.y) / d.y;
        if (tg > 0 && tg < best) { best = tg; hit = true; }
    }
    if (hit) tHit = best;
    return hit;
}

bool World::lineOfSight(V3 a, V3 b) const {
    V3 d = b - a;
    float l = len(d);
    if (l < 0.1f) return true;
    float t;
    return !raycast(a, d * (1.0f / l), l - 0.2f, t);
}

int World::nearestNode(V3 p) const {
    int best = 0;
    float bd = 1e18f;
    for (int i = 0; i < (int)nodes.size(); i++) {
        float d = dist2d(p, nodes[i].p);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}
int World::edgeBetween(int a, int b) const {
    for (int e : nodes[a].edges)
        if ((edges[e].a == a && edges[e].b == b) || (edges[e].a == b && edges[e].b == a)) return e;
    return -1;
}
int World::insideInterior(V3 p) const {
    for (int i = 0; i < (int)interiors.size(); i++) {
        const AABB& b = interiors[i].bounds;
        if (p.x > b.mn.x && p.x < b.mx.x && p.z > b.mn.z && p.z < b.mx.z) return i;
    }
    return -1;
}
