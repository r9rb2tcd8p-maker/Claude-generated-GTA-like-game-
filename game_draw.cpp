// Dynamic geometry (vehicles, people, effects), HUD and minimap.
#include "game.h"
#include <cstdio>

// ---------------------------------------------------------------- humanoid model
struct Look { Col shirt, pants, skin, hair; bool cop; };

// Local axes: +Z forward, +X = character's left, +Y up.
// gun: 0 none, 1 pistol, 2 SMG.  gunAngle: right-arm pitch (0 = hanging, -1.45 = pointing forward).
static void humanModel(MeshBuilder& mb, V3 pos, float yaw, const Look& L, float phase, float amp, bool handsUp, int gun, float gunAngle,
                       float crouchK, float punchK, bool lying, float flash) {
    Mat4 base = lying ? mMul(mTranslate(pos + V3{0, 0.22f, 0}), mMul(mRotY(yaw), mRotX(-PI_F / 2)))
                      : mMul(mTranslate(pos + V3{0, -crouchK * 0.32f, 0}), mRotY(yaw));
    auto part = [&](const Mat4& m, V3 off, V3 half, Col c) { mb.boxM(mMul(m, mTranslate(off)), half, c); };
    part(base, {0, 1.15f, 0}, {0.27f, 0.36f, 0.15f}, L.shirt);
    part(base, {0, 1.74f, 0}, {0.15f, 0.16f, 0.15f}, L.skin);
    part(base, {0, 1.88f, -0.02f}, {0.16f, 0.06f, 0.16f}, L.hair);
    if (L.cop) {
        part(base, {0, 1.95f, 0.02f}, {0.17f, 0.05f, 0.18f}, RGB(0x10162a));
        part(base, {0, 1.91f, 0.19f}, {0.17f, 0.02f, 0.08f}, RGB(0x10162a));
    }
    float legBend = crouchK * 0.9f;
    for (int s = -1; s <= 1; s += 2) {
        float a = lying ? 0.0f : s * std::sin(phase) * amp;
        Mat4 m = mMul(base, mMul(mTranslate({s * 0.13f, 0.82f, 0}), mRotX(a - legBend * 0.4f)));
        part(m, {0, -0.4f, 0}, {0.1f, 0.4f, 0.1f}, L.pants);
        part(m, {0, -0.8f, 0.04f}, {0.1f, 0.04f, 0.14f}, RGB(0x202020));
    }
    for (int s = -1; s <= 1; s += 2) {
        float a = lying ? 0.0f : -s * std::sin(phase) * amp;
        bool right = s < 0;
        if (handsUp) a = PI_F * 0.92f;
        else if (right && gun) a = gunAngle;
        else if (right && punchK > 0) a = -1.5f * std::sin(clampf(punchK, 0, 1) * PI_F);
        else if (!right && gun && gunAngle < -1.0f) a = gunAngle + 0.15f;  // two-handed grip
        Mat4 m = mMul(base, mMul(mTranslate({s * 0.37f, 1.45f, 0}), mRotX(a)));
        part(m, {0, -0.3f, 0}, {0.07f, 0.3f, 0.07f}, L.shirt);
        part(m, {0, -0.64f, 0}, {0.06f, 0.06f, 0.06f}, L.skin);
        if (right && gun && !handsUp) {
            float gl = gun == 2 ? 0.24f : 0.13f;
            part(m, {0, -0.64f - gl, 0}, {0.035f, gl, 0.05f}, RGB(0x1b1b1b));
            if (flash > 0) part(m, {0, -0.64f - gl * 2 - 0.1f, 0}, {0.07f, 0.1f, 0.07f}, RGB(0xffe070, 1.0f));
        }
    }
}

static int pedPose(const Ped& p, bool& handsUp, int& gun, float& ang, float& phase, float& amp) {
    handsUp = p.st == Ped::HANDSUP;
    gun = (p.type == Ped::COP && p.st == Ped::CHASE) ? 1 : 0;
    ang = -1.45f;
    switch (p.st) {
        case Ped::WALK: phase = p.anim * 7.0f * (p.speed / 1.4f); amp = 0.6f; break;
        case Ped::FLEE: phase = p.anim * 12.0f; amp = 0.95f; break;
        case Ped::CHASE: phase = p.anim * 10.0f; amp = 0.8f; break;
        default: phase = 0; amp = 0; break;
    }
    return 0;
}

void Game::drawPed(const Ped& p) {
    Look L{p.shirt, p.pants, p.skin, p.hair, p.type == Ped::COP};
    bool up; int gun; float ang, phase, amp;
    pedPose(p, up, gun, ang, phase, amp);
    humanModel(dyn, p.pos, p.yaw, L, phase, amp, up, gun, ang, 0, 0, p.st == Ped::DEAD, 0);
}

void Game::drawPlayerModel() {
    if (pv >= 0) return;
    Look L{RGB(0xe0782a), RGB(0x34476b), RGB(0xe8b894), RGB(0x2a1c12), false};
    bool shooting = aiming || aimT > 0;
    float ang = shooting ? -1.45f : -0.45f;
    float amp = moving ? 0.65f : 0.0f;
    float punch = (weapon == W_FISTS && meleeT > 0) ? 1.0f - meleeT / 0.3f : 0.0f;
    humanModel(dyn, pos, yaw, L, walkPhase * 3.0f, amp, false, weapon, ang, crouchT, punch, dead, muzzleFlash);
}

// ---------------------------------------------------------------- vehicle model
void Game::drawVehicle(const Vehicle& v) {
    Mat4 base = mMul(mTranslate(v.pos), mRotY(v.yaw));
    auto part = [&](V3 off, V3 half, Col c) { dyn.boxM(mMul(base, mTranslate(off)), half, c); };
    bool wreck = v.hp <= 0;
    Col body = wreck ? RGB(0x262626) : (v.hp < 35 ? shade(v.color, 0.65f) : v.color);
    Col glass = RGB(0x1b2836), dark = RGB(0x141414);
    float hl = v.halfLen, hw = v.halfWid;
    if (v.style == 2) {  // van
        part({0, 0.95f, 0}, {hw, 0.6f, hl}, body);
        part({0, 1.22f, hl * 0.78f}, {hw + 0.01f, 0.24f, hl * 0.2f}, glass);
        part({0, 0.5f, 0}, {hw + 0.02f, 0.05f, hl}, shade(body, 0.7f));
    } else {
        part({0, 0.55f, 0}, {hw, 0.3f, hl}, body);
        float cl = v.style == 1 ? hl * 0.42f : hl * 0.52f, cz = v.style == 1 ? -0.4f : -0.2f, ch = v.style == 1 ? 0.22f : 0.27f;
        part({0, 0.85f + ch, cz}, {hw - 0.1f, ch, cl}, glass);
        part({0, 0.85f + ch * 2, cz}, {hw - 0.1f, 0.04f, cl - 0.05f}, body);
    }
    part({0, 0.32f, hl}, {hw, 0.1f, 0.07f}, dark);
    part({0, 0.32f, -hl}, {hw, 0.1f, 0.07f}, dark);
    bool braking = v.inBrk > 0.1f || (v.inThr < 0 && v.fwdSpeed > 1);
    for (int s = -1; s <= 1; s += 2) {
        part({s * (hw - 0.26f), 0.64f, hl}, {0.16f, 0.09f, 0.04f}, RGB(0xfff2c0, wreck ? 0.0f : 1.0f));
        part({s * (hw - 0.26f), 0.64f, -hl}, {0.16f, 0.09f, 0.04f}, RGB(0xd01818, wreck ? 0.0f : (braking ? 1.0f : 0.45f)));
    }
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sz = -1; sz <= 1; sz += 2) {
            Mat4 wm = mMul(base, mTranslate({sx * (hw - 0.02f), 0.33f, sz * hl * 0.62f}));
            if (sz > 0) wm = mMul(wm, mRotY(v.steer * 0.5f));
            wm = mMul(wm, mRotX(v.wheelRot));
            dyn.boxM(wm, {0.12f, 0.3f, 0.3f}, RGB(0x101010));
        }
    if (v.police && !wreck) {
        part({0, 0.56f, 0.4f}, {hw + 0.012f, 0.16f, 0.75f}, RGB(0x1a1a1a));
        part({0, 0.56f, -0.9f}, {hw + 0.012f, 0.16f, 0.6f}, RGB(0x1a1a1a));
        bool active = v.drv == Vehicle::POLICE || stars > 0;
        bool a = active && ((int)(v.siren * 5.0f) % 2 == 0);
        bool b = active && !a;
        part({0.24f, 1.46f, -0.25f}, {0.22f, 0.07f, 0.1f}, RGB(0xff2020, a ? 1.0f : 0.1f));
        part({-0.24f, 1.46f, -0.25f}, {0.22f, 0.07f, 0.1f}, RGB(0x2040ff, b ? 1.0f : 0.1f));
    }
    if (v.drv != Vehicle::NONE && !wreck) {
        part({0.4f, 1.12f, 0.15f}, {0.14f, 0.2f, 0.14f}, RGB(0xd9a06b));
    }
}

void Game::drawSignals() {
    const Col steel = RGB(0x4a4d52), box = RGB(0x15161a);
    const Col cols[3] = {RGB(0x30e060, 1.0f), RGB(0xffd030, 1.0f), RGB(0xff3030, 1.0f)};
    for (int n = 0; n < (int)W->nodes.size(); n++) {
        const RoadNode& nd = W->nodes[n];
        if (!nd.signal || dist2d(nd.p, camPos) > 170.0f) continue;
        Col cx = cols[signalState(n, true)], cz = cols[signalState(n, false)];
        for (int sx = -1; sx <= 1; sx += 2)
            for (int sz = -1; sz <= 1; sz += 2) {
                float x = nd.p.x + sx * 8.4f, z = nd.p.z + sz * 8.4f;
                dyn.box({x, 2.6f, z}, {0.07f, 2.6f, 0.07f}, 0, steel);
                dyn.box({x, 5.0f, z}, {0.2f, 0.45f, 0.2f}, 0, box);
                dyn.box({x + sx * 0.2f, 5.0f, z}, {0.04f, 0.3f, 0.1f}, 0, cx);
                dyn.box({x, 5.0f, z + sz * 0.2f}, {0.1f, 0.3f, 0.04f}, 0, cz);
            }
    }
}

// ---------------------------------------------------------------- frame
void Game::render(Renderer& r) {
    dyn.clear();
    drawSignals();
    for (const auto& v : vehicles) if (dist2d(v.pos, camPos) < 330.0f) drawVehicle(v);
    for (const auto& p : peds) if (dist2d(p.pos, camPos) < 200.0f) drawPed(p);
    if (!dead || deadTimer > 0) drawPlayerModel();
    for (const auto& k : pickups) {
        float y = 0.55f + std::sin(k.bob * 3.0f) * 0.1f;
        if (k.kind == 0) dyn.box(k.pos + V3{0, y, 0}, {0.26f, 0.19f, 0.19f}, k.bob * 1.5f, RGB(0x3fa84a, 0.55f));
        else dyn.box(k.pos + V3{0, y, 0}, {0.22f, 0.16f, 0.14f}, k.bob * 1.5f, RGB(0xe08a2a, 0.55f));
    }
    for (const auto& t : tracers) dyn.beam(t.a, t.b, 0.02f, t.c);
    for (const auto& q : particles) {
        float s = q.size * clampf(q.life / q.maxLife + 0.3f, 0, 1);
        dyn.box(q.p, {s, s, s}, 0, q.c);
    }
    float aspect = (float)r.W / (float)std::max(1, r.H);
    Mat4 view = mLookAt(camPos, camPos + camDir, {0, 1, 0});
    Mat4 proj = mPerspective(camFov * PI_F / 180.0f, aspect, 0.15f, 1500.0f);
    r.draw3D(view, proj, camPos, dyn.v, Col{0.55f, 0.70f, 0.88f, 1.0f}, 130.0f, 500.0f, 0.75f);
    drawHUD(r);
    r.flush2D();
}

// ---------------------------------------------------------------- HUD
static Col opaque(Col c) { c.a = 1.0f; return c; }

static void starShape(Renderer& r, float cx, float cy, float R, Col c) {
    float px[10], py[10];
    for (int i = 0; i < 10; i++) {
        float a = -PI_F / 2 + i * PI_F / 5, rr = (i % 2 == 0) ? R : R * 0.42f;
        px[i] = cx + std::cos(a) * rr; py[i] = cy + std::sin(a) * rr;
    }
    for (int i = 0; i < 10; i++) r.tri(cx, cy, px[i], py[i], px[(i + 1) % 10], py[(i + 1) % 10], c);
}

void Game::drawMinimap(Renderer& r, float ui) {
    float size = 190.0f * ui, x0 = 18.0f * ui, y0 = r.H - size - 18.0f * ui;
    float cx = x0 + size / 2, cy = y0 + size / 2;
    float scale = (mapZoomFar ? 0.30f : 0.95f) * ui;
    V3 c = pv >= 0 ? vehicles[pv].pos : pos;
    float cy_ = std::cos(camYaw), sy_ = std::sin(camYaw);
    // camera forward = (sin, cos), camera right = (-cos, sin); map up = forward
    auto mp = [&](V3 p, float& mx, float& my) {
        float rx = p.x - c.x, rz = p.z - c.z;
        mx = cx + (rx * -cy_ + rz * sy_) * scale;
        my = cy - (rx * sy_ + rz * cy_) * scale;
    };
    r.rect(x0 - 3 * ui, y0 - 3 * ui, size + 6 * ui, size + 6 * ui, {0, 0, 0, 0.75f});
    r.setScissor(x0, y0, size, size);
    r.rect(x0, y0, size, size, RGB(0x2f4a35, 1.0f));
    for (const auto& mr : W->mapRects) {
        float ax, ay, bx, by, cx2, cy2, dx, dy;
        mp({mr.x, 0, mr.z}, ax, ay); mp({mr.x + mr.w, 0, mr.z}, bx, by);
        mp({mr.x + mr.w, 0, mr.z + mr.d}, cx2, cy2); mp({mr.x, 0, mr.z + mr.d}, dx, dy);
        r.quad(ax, ay, bx, by, cx2, cy2, dx, dy, opaque(mr.c));
    }
    for (const auto& rd : W->mapRoads) {
        float ax, ay, bx, by;
        mp(rd.a, ax, ay); mp(rd.b, bx, by);
        r.line(ax, ay, bx, by, std::max(2.0f * ui, rd.hw * 2.0f * scale), opaque(rd.col));
    }
    for (const auto& v : vehicles) {
        if (!v.police || v.drv != Vehicle::POLICE) continue;
        float mx, my; mp(v.pos, mx, my);
        bool on = ((int)(time * 4.0f) % 2) == 0;
        r.disc(mx, my, 4.0f * ui, on ? Col{1, 0.2f, 0.2f, 1} : Col{0.2f, 0.3f, 1, 1}, 10);
    }
    for (const auto& p : peds) {
        if (p.type != Ped::COP || p.st == Ped::DEAD) continue;
        float mx, my; mp(p.pos, mx, my);
        r.disc(mx, my, 2.5f * ui, Col{0.3f, 0.5f, 1, 1}, 8);
    }
    for (const auto& it : W->interiors) {
        float mx, my; mp(it.registerPos, mx, my);
        r.disc(mx, my, 3.0f * ui, it.kind == Interior::BANK ? Col{1, 0.85f, 0.2f, 1} : Col{0.4f, 1, 0.6f, 1}, 8);
    }
    r.clearScissor();
    // player arrow (map is camera-up; arrow shows body heading relative to the camera)
    float rel = wrapAngle(yaw - camYaw);
    float fx = std::sin(rel) * -1.0f, fy = -std::cos(rel);
    r.tri(cx + fx * 9 * ui, cy + fy * 9 * ui, cx + (-fy) * 5 * ui - fx * 5 * ui, cy + fx * 5 * ui - fy * 5 * ui,
          cx - (-fy) * 5 * ui - fx * 5 * ui, cy - fx * 5 * ui - fy * 5 * ui, Col{1, 1, 1, 1});
}

void Game::drawHUD(Renderer& r) {
    float ui = r.H / 720.0f, Wd = (float)r.W, Hd = (float)r.H;
    auto T = [&](float x, float y, float sc, const char* s, Col c) { r.text(x, y, sc * ui, s, c, true); };
    auto TR = [&](float xr, float y, float sc, const char* s, Col c) { r.text(xr - r.textWidth(s, sc * ui), y, sc * ui, s, c, true); };
    auto TC = [&](float xc, float y, float sc, const char* s, Col c) { r.text(xc - r.textWidth(s, sc * ui) / 2, y, sc * ui, s, c, true); };
    const Col white{1, 1, 1, 1};

    if (dmgFlash > 0) r.rect(0, 0, Wd, Hd, {0.8f, 0, 0, clampf(dmgFlash, 0, 1) * 0.35f});

    if (!dead && pv < 0 && enterTarget < 0 && weapon != W_FISTS) {
        float g = (aiming ? 5.0f : 11.0f) * ui, l = 7.0f * ui, th = 2.0f * ui;
        Col cc{1, 1, 1, aiming ? 0.95f : 0.6f};
        r.rect(Wd / 2 - g - l, Hd / 2 - th / 2, l, th, cc); r.rect(Wd / 2 + g, Hd / 2 - th / 2, l, th, cc);
        r.rect(Wd / 2 - th / 2, Hd / 2 - g - l, th, l, cc); r.rect(Wd / 2 - th / 2, Hd / 2 + g, th, l, cc);
    }

    drawMinimap(r, ui);

    // health / armor
    float bx = 18 * ui + 190 * ui + 16 * ui, bw = 200 * ui, bh = 14 * ui, by = Hd - 18 * ui - bh;
    if (armor > 0) {
        r.rect(bx, by - 22 * ui, bw, bh, {0, 0, 0, 0.6f});
        r.rect(bx, by - 22 * ui, bw * clampf(armor / 100.0f, 0, 1), bh, Col{0.35f, 0.6f, 1, 0.95f});
    }
    r.rect(bx, by, bw, bh, {0, 0, 0, 0.6f});
    r.rect(bx, by, bw * clampf(health / 100.0f, 0, 1), bh, health > 30 ? Col{0.3f, 0.85f, 0.35f, 0.95f} : Col{0.9f, 0.2f, 0.2f, 0.95f});

    // cash, wanted
    char buf[96];
    std::snprintf(buf, sizeof buf, "$%d", cash);
    TR(Wd - 22 * ui, 18 * ui, 4.0f, buf, Col{0.55f, 0.95f, 0.5f, 1});
    bool blink = ((int)(time * 4.0f) % 2) == 0;
    for (int i = 0; i < 5; i++) {
        float sx = Wd - 22 * ui - 14 * ui - i * 30 * ui, sy = 76 * ui;
        bool lit = i < stars;
        Col c = lit ? Col{1, 0.85f, 0.2f, 1} : Col{1, 1, 1, 0.22f};
        if (lit && starFlash > 0 && !blink) c = Col{1, 1, 1, 1};
        if (lit && starFlash <= 0 && evade > 0 && !blink) c = Col{1, 0.85f, 0.2f, 0.25f};
        starShape(r, sx, sy, 12 * ui, c);
    }
    if (stars > 0 && evade > 0) {
        std::snprintf(buf, sizeof buf, "EVADING %d", (int)std::ceil(12.0f - evade));
        TR(Wd - 22 * ui, 100 * ui, 2.0f, buf, white);
    }

    // weapon / vehicle
    if (pv >= 0) {
        std::snprintf(buf, sizeof buf, "%d KM/H", (int)(std::fabs(vehicles[pv].fwdSpeed) * 3.6f));
        TR(Wd - 22 * ui, Hd - 54 * ui, 4.0f, buf, white);
        std::snprintf(buf, sizeof buf, "CAR %d%%", (int)clampf(vehicles[pv].hp, 0, 100));
        TR(Wd - 22 * ui, Hd - 24 * ui, 2.0f, buf, vehicles[pv].hp > 35 ? white : Col{1, 0.4f, 0.3f, 1});
    } else {
        const WeaponDef& wd = WEAPONS[weapon];
        TR(Wd - 22 * ui, Hd - 70 * ui, 3.0f, wd.name, white);
        if (!wd.melee) {
            std::snprintf(buf, sizeof buf, "%d/%d", mag[weapon], reserve[weapon]);
            TR(Wd - 22 * ui, Hd - 40 * ui, 3.0f, buf, Col{1, 0.9f, 0.5f, 1});
        }
        if (reloadT > 0) TC(Wd / 2, Hd / 2 + 60 * ui, 2.5f, "RELOADING", Col{1, 0.9f, 0.5f, 1});
    }

    // messages
    float my = 18 * ui;
    for (const auto& m : msgs) {
        float a = clampf(m.t, 0, 1);
        T(20 * ui, my, 2.5f, m.text.c_str(), Col{1, 1, 1, a});
        my += 26 * ui;
    }
    if (!prompt.empty() && !dead) TC(Wd / 2, Hd * 0.72f, 2.5f, prompt.c_str(), Col{1, 1, 1, 1});
    if (robIdx >= 0) {
        bool bank = W->interiors[robIdx].kind == Interior::BANK;
        float frac = clampf(robT / (bank ? 12.0f : 5.0f), 0, 1), w = 300 * ui, x = Wd / 2 - w / 2, y = Hd * 0.72f + 34 * ui;
        r.rect(x - 2 * ui, y - 2 * ui, w + 4 * ui, 14 * ui + 4 * ui, {0, 0, 0, 0.7f});
        r.rect(x, y, w * frac, 14 * ui, Col{0.95f, 0.8f, 0.2f, 1});
    }
    if (helpT < 25.0f && !paused) {
        float a = clampf((25.0f - helpT) / 3.0f, 0, 1);
        TC(Wd / 2, Hd - 30 * ui, 2.0f, "WASD MOVE  SHIFT SPRINT  SPACE JUMP  F VEHICLE  E ROB  R RELOAD  ESC PAUSE", Col{1, 1, 1, a * 0.8f});
    }

    if (dead) {
        float a = clampf(1.0f - deadTimer / 4.0f, 0, 1);
        r.rect(0, 0, Wd, Hd, {0, 0, 0, a * 0.55f});
        TC(Wd / 2, Hd / 2 - 40 * ui, 10.0f, "WASTED", Col{0.85f, 0.1f, 0.1f, a});
    }
    if (paused) {
        r.rect(0, 0, Wd, Hd, {0, 0, 0, 0.6f});
        float y = Hd / 2 - 150 * ui;
        TC(Wd / 2, y, 8.0f, "PAUSED", white); y += 90 * ui;
        const char* lines[] = {"WASD MOVE   SHIFT SPRINT   SPACE JUMP / HANDBRAKE   C CROUCH",
                               "MOUSE LOOK   LMB FIRE   RMB AIM   R RELOAD   Q / 1-3 WEAPON",
                               "F ENTER / EXIT VEHICLE   E ROB STORES AND THE BANK",
                               "V CAMERA DISTANCE   Z LOOK BEHIND   M MAP ZOOM",
                               "F5 SAVE   F9 LOAD", "",
                               "ESC RESUME     Q QUIT"};
        for (const char* l : lines) { TC(Wd / 2, y, 2.5f, l, Col{0.9f, 0.9f, 0.9f, 1}); y += 30 * ui; }
    }
}
