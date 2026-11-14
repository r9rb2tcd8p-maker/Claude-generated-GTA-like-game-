#include "game.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

const WeaponDef WEAPONS[3] = {
    {"FISTS", 18, 0.45f, 0, 0, 0, true, false},
    {"PISTOL", 24, 0.28f, 0.012f, 1.4f, 12, false, false},
    {"SMG", 11, 0.075f, 0.035f, 2.0f, 30, false, true},
};

static const Col CAR_COLORS[10] = {RGB(0xc23b3b), RGB(0x2e5fa8), RGB(0xe8e8e8), RGB(0x222428), RGB(0xd9a62e),
                                   RGB(0x3f8f5c), RGB(0x8a8f99), RGB(0xd2d0c0), RGB(0x7a3b8f), RGB(0xe07a2e)};
static const Col SKINS[5] = {RGB(0xf2c9a0), RGB(0xd9a06b), RGB(0xa86f46), RGB(0x6b4429), RGB(0xe8b894)};
static const Col SHIRTS[8] = {RGB(0xd94a4a), RGB(0x4a86d9), RGB(0xe8c84a), RGB(0x58b86a), RGB(0xb061d9), RGB(0xf0f0f0), RGB(0xe8884a), RGB(0x3a3f4a)};
static const Col PANTS[4] = {RGB(0x2b3a5c), RGB(0x3a3a3a), RGB(0x6b5a45), RGB(0x4a6b8f)};
static const Col HAIRS[5] = {RGB(0x1a1410), RGB(0x4a3220), RGB(0xc8a050), RGB(0x8a2f1c), RGB(0x777777)};

static V3 playerPosOf(const std::vector<Vehicle>& vs, int pv, V3 pos) { return pv >= 0 ? vs[pv].pos : pos; }

// ---------------------------------------------------------------- setup
void Game::init(World* w) {
    W = w;
    time = 0;
    pos = {9.0f, GROUND, 36.0f};
    yaw = PI_F / 2; camYaw = PI_F / 2; camPitch = -0.18f;
    camPivot = pos + V3{0, 1.5f, 0};
    camPos = camPivot - V3{4, 0, 0};
    for (const auto& ps : W->parked) {
        Vehicle v = makeVehicle(ps.pos, ps.yaw, rng.i(3), CAR_COLORS[rng.i(10)], false);
        vehicles.push_back(v);
    }
    vehicles.push_back(makeVehicle({5.4f, GROUND, 24.0f}, PI_F, 1, RGB(0xd9342b), false));
    for (int i = 0; i < (int)W->interiors.size(); i++) {
        const Interior& in = W->interiors[i];
        for (int k = 0; k < in.clerkCount; k++) {
            Ped c = makePed(Ped::CLERK, in.clerkPos[k]);
            c.yaw = in.clerkYaw; c.st = Ped::IDLE; c.interior = i;
            c.shirt = in.kind == Interior::BANK ? RGB(0x2b3550) : RGB(0x2fa36b);
            peds.push_back(c);
        }
    }
    for (int i = 0; i < 20; i++) spawnTraffic(true);
    for (int i = 0; i < 60; i++) spawnPeds(true);
    message("WELCOME TO PALM HEAT. ROB THE BANK IF YOU DARE.", 5);
}

void Game::message(const std::string& s, float t) {
    msgs.push_back({s, t});
    if (msgs.size() > 4) msgs.erase(msgs.begin());
}
void Game::addStars(int minStars) {
    if (dead) return;
    if (stars < minStars) { stars = std::min(5, minStars); starFlash = 2.0f; evade = 0; }
}
void Game::spawnParticles(V3 p, int n, Col c, float speed, float life, float grav) {
    for (int i = 0; i < n && particles.size() < 400; i++) {
        Particle q;
        q.p = p;
        q.v = {rng.range(-1, 1) * speed, rng.range(0.2f, 1.2f) * speed, rng.range(-1, 1) * speed};
        q.life = q.maxLife = life * rng.range(0.6f, 1.2f);
        q.size = rng.range(0.04f, 0.09f); q.c = c; q.grav = grav;
        particles.push_back(q);
    }
}
void Game::noise(V3 p, float radius) {
    for (auto& q : peds) {
        if (q.st == Ped::DEAD || dist2d(q.pos, p) > radius) continue;
        if (q.type == Ped::CIVIL && q.st != Ped::CHASE) { q.st = Ped::FLEE; q.fleeFrom = p; q.timer = rng.range(5, 9); }
        else if (q.type == Ped::CLERK && q.st == Ped::IDLE) { q.st = Ped::HANDSUP; q.timer = 6; }
    }
}

void Game::damagePlayer(float d) {
    if (dead || d <= 0) return;
    dmgFlash = std::min(1.0f, dmgFlash + d * 0.04f + 0.25f);
    shake = std::min(0.5f, shake + d * 0.01f);
    if (armor > 0) { float a = std::min(armor, d); armor -= a; d -= a; }
    health -= d;
    if (health <= 0) die();
}
void Game::die() {
    health = 0; dead = true; deadTimer = 4.0f; robIdx = -1; robT = 0;
    if (pv >= 0) { vehicles[pv].drv = Vehicle::NONE; vehicles[pv].inThr = 0; pv = -1; }
    message("WASTED", 4);
}
void Game::respawn() {
    dead = false; health = 100; armor = 0;
    int lost = cash / 10; cash -= lost;
    stars = 0; evade = 0; policeStandDown();
    pos = {9.0f, GROUND, -12.0f}; vel = {0, 0, 0}; vy = 0; yaw = PI_F / 2; camYaw = PI_F / 2;
    pv = -1; exitT = 0; enterTarget = -1; enterT = 0;
    message("HOSPITAL BILL: $" + std::to_string(lost), 4);
}

// ---------------------------------------------------------------- vehicles
Vehicle Game::makeVehicle(V3 p, float yaw, int style, Col c, bool police) {
    Vehicle v;
    v.pos = {p.x, GROUND, p.z}; v.yaw = yaw; v.style = style; v.color = c; v.police = police;
    if (police) { v.style = 0; v.color = RGB(0xf2f2f2); }
    switch (v.style) {
        case 0: v.halfLen = 2.3f; v.halfWid = 0.95f; v.maxSpeed = 36; break;
        case 1: v.halfLen = 2.2f; v.halfWid = 0.95f; v.maxSpeed = 44; break;
        default: v.halfLen = 2.7f; v.halfWid = 1.05f; v.maxSpeed = 30; break;
    }
    if (police) v.maxSpeed = 36;
    return v;
}

int Game::nearestVehicle(float maxDist) const {
    int best = -1; float bd = maxDist;
    for (int i = 0; i < (int)vehicles.size(); i++) {
        const Vehicle& v = vehicles[i];
        V3 d = pos - v.pos;
        float lf = std::fabs(dot(d, fwdOf(v.yaw))) - v.halfLen, lr = std::fabs(dot(d, rightOf(v.yaw))) - v.halfWid;
        float dist = std::sqrt(std::max(0.0f, lf) * std::max(0.0f, lf) + std::max(0.0f, lr) * std::max(0.0f, lr));
        if (dist < bd) { bd = dist; best = i; }
    }
    return best;
}

bool Game::pushOutOfVehicle(V3& p, float r, const Vehicle& v) const {
    V3 d = p - v.pos;
    V3 f = fwdOf(v.yaw), rt = rightOf(v.yaw);
    float lf = dot(d, f), lr = dot(d, rt);
    float ex = v.halfWid + r, ez = v.halfLen + r;
    if (std::fabs(lr) >= ex || std::fabs(lf) >= ez) return false;
    float px = ex - std::fabs(lr), pz = ez - std::fabs(lf);
    if (px < pz) p += rt * ((lr >= 0 ? 1.0f : -1.0f) * px);
    else p += f * ((lf >= 0 ? 1.0f : -1.0f) * pz);
    return true;
}

void Game::damageVehicle(Vehicle& v, float d) {
    if (d <= 0) return;
    v.hp = std::max(0.0f, v.hp - d);
    if (v.hp <= 0 && (v.drv == Vehicle::TRAFFIC || v.drv == Vehicle::POLICE)) { v.drv = Vehicle::NONE; }
}

void Game::stepVehicle(Vehicle& v, float dt) {
    V3 f = fwdOf(v.yaw), r = rightOf(v.yaw);
    float fwd = dot(v.vel, f), lat = dot(v.vel, r);
    float vmax = v.maxSpeed;
    float thr = v.hp > 0 ? v.inThr : 0;
    if (thr > 0) { if (fwd < -1.0f) fwd += thr * 26 * dt; else fwd += thr * 15 * (1 - clampf(fwd / vmax, 0, 1)) * dt; }
    else if (thr < 0) { if (fwd > 1.0f) fwd -= (-thr) * 26 * dt; else fwd += thr * 7 * (1 - clampf(-fwd / 12, 0, 1)) * dt; }
    if (v.inBrk > 0) { float d = v.inBrk * 28 * dt; if (std::fabs(fwd) < d) fwd = 0; else fwd -= (fwd > 0 ? d : -d); }
    if (thr == 0 && v.inBrk == 0) { float d = 2.2f * dt; if (std::fabs(fwd) < d) fwd = 0; else fwd -= (fwd > 0 ? d : -d); }
    fwd *= 1 - 0.08f * dt;
    fwd = clampf(fwd, -vmax * 0.35f, vmax);
    float grip = v.inHand ? 1.3f : 10.0f - clampf(std::fabs(fwd) / 40.0f, 0, 1) * 4.0f;
    lat *= std::exp(-grip * dt);
    if (v.inHand) fwd *= 1 - 0.7f * dt;
    v.steer += (v.inSteer - v.steer) * smoothT(9, dt);
    float sf = 1.0f / (1.0f + std::fabs(fwd) / 13.0f);
    float steerAng = v.steer * 0.62f * sf * (v.inHand ? 1.6f : 1.0f);
    float yr = clampf(fwd / 2.7f * std::tan(steerAng), -2.6f, 2.6f);
    v.yawRate += (yr - v.yawRate) * smoothT(12, dt);
    v.vel = f * fwd + r * lat;
    v.yaw = wrapAngle(v.yaw + v.yawRate * dt);
    v.pos += v.vel * dt;
    v.pos.y = GROUND;
    v.fwdSpeed = fwd;
    v.wheelRot += fwd * dt / 0.35f;
    // collisions against static geometry (three circles along the body)
    V3 f2 = fwdOf(v.yaw);
    float rad = v.halfWid + 0.08f, off[3] = {v.halfLen - rad, 0, -(v.halfLen - rad)};
    V3 nTot{0, 0, 0};
    bool hit = false;
    for (int k = 0; k < 3; k++) {
        V3 c = v.pos + f2 * off[k], n;
        V3 c2 = W->pushCircle(c, rad, 0.35f, 1.8f, &n);
        V3 d = c2 - c;
        if (len2d(d) > 1e-4f) { v.pos += d; nTot += n; hit = true; }
    }
    float lim = 3000.0f;
    v.pos.x = clampf(v.pos.x, -440, 820); v.pos.z = clampf(v.pos.z, -330, 830);
    (void)lim;
    if (hit) {
        V3 n = norm2d(nTot);
        float vn = dot(v.vel, n);
        if (vn < 0) {
            float impact = -vn;
            v.vel -= n * vn * 1.4f;
            v.vel = v.vel * 0.92f;
            if (impact > 4) {
                damageVehicle(v, (impact - 3) * 3.0f);
                spawnParticles(v.pos + f2 * 1.5f + V3{0, 0.5f, 0}, 6, RGB(0xffd060, 1), 3.0f, 0.4f);
                if (v.drv == Vehicle::PLAYER && impact > 8) { damagePlayer((impact - 8) * 2.0f); shake = std::min(0.6f, impact * 0.03f); }
            }
        }
    }
    if (v.hp < 35 && rng.chance(dt * 12)) spawnParticles(v.pos + f2 * v.halfLen * 0.8f + V3{0, 0.9f, 0}, 1, RGB(0x555555), 0.8f, 1.4f, -1.5f);
}

V3 Game::lanePoint(int from, int to, float s, float off) const {
    V3 a = W->nodes[from].p, b = W->nodes[to].p, d = norm2d(b - a);
    V3 rt{-d.z, 0, d.x};
    return a + d * s + rt * off;
}
int Game::signalState(int node, bool axisX) const {
    float t = std::fmod(time + node * 3.7f, 22.0f);
    if (axisX) { if (t < 8) return 0; if (t < 9.5f) return 1; return 2; }
    if (t >= 11 && t < 19) return 0;
    if (t >= 19 && t < 20.5f) return 1;
    return 2;
}
int Game::pickNext(int node, int prev) {
    std::vector<int> opts;
    for (int e : W->nodes[node].edges) {
        int o = W->edges[e].a == node ? W->edges[e].b : W->edges[e].a;
        if (o != prev) opts.push_back(o);
    }
    if (opts.empty()) return prev >= 0 ? prev : node;
    return opts[rng.i((int)opts.size())];
}

void Game::spawnTraffic(bool initial) {
    int count = 0;
    for (const auto& v : vehicles) if (v.drv == Vehicle::TRAFFIC) count++;
    if (count >= 22) return;
    V3 pc = playerPosOf(vehicles, pv, pos);
    for (int tries = 0; tries < 40; tries++) {
        int e = rng.i((int)W->edges.size());
        const RoadEdge& ed = W->edges[e];
        float s = rng.range(4, std::max(5.0f, ed.length - 4));
        bool fw = rng.chance(0.5f);
        int from = fw ? ed.a : ed.b, to = fw ? ed.b : ed.a;
        float sd = fw ? s : ed.length - s;
        V3 p = lanePoint(from, to, sd, ed.hw * 0.5f);
        float d = dist2d(p, pc);
        if (initial ? (d < 40 || d > 320) : (d < 170 || d > 320)) continue;
        bool clash = false;
        for (const auto& v : vehicles) if (dist2d(v.pos, p) < 9) clash = true;
        if (clash) continue;
        V3 dir = norm2d(W->nodes[to].p - W->nodes[from].p);
        Vehicle v = makeVehicle(p, std::atan2(dir.x, dir.z), rng.i(3), CAR_COLORS[rng.i(10)], false);
        v.drv = Vehicle::TRAFFIC; v.from = from; v.to = to; v.next = pickNext(to, from);
        v.vel = dir * 8.0f;
        vehicles.push_back(v);
        return;
    }
}

void Game::aiTraffic(Vehicle& v, float dt) {
    if (v.from < 0 || v.to < 0 || W->edgeBetween(v.from, v.to) < 0) {
        v.from = W->nearestNode(v.pos); v.to = pickNext(v.from, -1); v.next = pickNext(v.to, v.from);
    }
    int e = W->edgeBetween(v.from, v.to);
    V3 a = W->nodes[v.from].p, b = W->nodes[v.to].p, d = norm2d(b - a);
    float s = dot(v.pos - a, d), L = W->edges[e].length;
    if (s >= L - 1.0f) {
        v.from = v.to; v.to = v.next; v.next = pickNext(v.to, v.from);
        e = W->edgeBetween(v.from, v.to);
        if (e < 0) return;
        a = W->nodes[v.from].p; b = W->nodes[v.to].p; d = norm2d(b - a);
        s = dot(v.pos - a, d); L = W->edges[e].length;
    }
    const RoadEdge& ed = W->edges[e];
    float off = ed.hw * 0.5f, remain = L - s;
    float look = clampf(6 + std::fabs(v.fwdSpeed) * 0.5f, 7, 20);
    V3 target;
    int e2 = W->edgeBetween(v.to, v.next);
    if (look < remain || e2 < 0) target = lanePoint(v.from, v.to, std::min(s + look, L), off);
    else target = lanePoint(v.to, v.next, look - remain, W->edges[e2].hw * 0.5f);
    float desired = std::atan2(target.x - v.pos.x, target.z - v.pos.z);
    float err = angleDiff(desired, v.yaw);
    v.inSteer = clampf(err * 1.7f, -1, 1);
    float base = ed.rural ? 17.0f : 10.5f;
    if (e2 >= 0 && remain < 32) {
        V3 d2 = W->edges[e2].dir;
        if (W->edges[e2].a != v.to) d2 = d2 * -1.0f;
        if (dot(d, d2) < 0.7f) base = std::min(base, 5.5f);
    }
    if (std::fabs(err) > 0.6f) base = std::min(base, 6.0f);
    const RoadNode& nn = W->nodes[v.to];
    if (nn.signal && remain < 60) {
        bool axisX = std::fabs(d.x) > std::fabs(d.z);
        if (signalState(v.to, axisX) != 0) {
            float stopDist = remain - 9.5f;
            if (stopDist > -2.0f) base = std::min(base, std::sqrt(std::max(0.0f, 2 * 4.5f * std::max(0.0f, stopDist))));
            if (stopDist < 0.5f && stopDist > -2.0f) base = 0;
        }
    }
    V3 f = fwdOf(v.yaw), r = rightOf(v.yaw);
    for (const auto& o : vehicles) {
        if (&o == &v) continue;
        V3 rel = o.pos - v.pos;
        float ahead = dot(rel, f), lat = std::fabs(dot(rel, r));
        if (ahead > 0 && ahead < 26 && lat < 2.5f) {
            float gap = ahead - (v.halfLen + o.halfLen);
            base = std::min(base, std::max(0.0f, (gap - 3.0f) * 0.9f));
        }
    }
    for (const auto& p : peds) {
        if (p.st == Ped::DEAD) continue;
        V3 rel = p.pos - v.pos;
        float ahead = dot(rel, f), lat = std::fabs(dot(rel, r));
        if (ahead > 0 && ahead < 16 && lat < 2.0f) base = std::min(base, std::max(0.0f, (ahead - v.halfLen - 1.5f) * 1.1f));
    }
    if (pv < 0 && !dead) {
        V3 rel = pos - v.pos;
        float ahead = dot(rel, f), lat = std::fabs(dot(rel, r));
        if (ahead > 0 && ahead < 16 && lat < 2.0f) base = std::min(base, std::max(0.0f, (ahead - v.halfLen - 1.5f) * 1.1f));
    }
    float spErr = base - v.fwdSpeed;
    v.inThr = 0; v.inBrk = 0;
    if (spErr > 0.5f) v.inThr = clampf(spErr * 0.35f, 0.15f, 0.85f);
    else if (spErr < -1.0f) v.inBrk = clampf(-spErr * 0.22f, 0, 1);
    if (v.reverseT > 0) { v.reverseT -= dt; v.inThr = -1; v.inBrk = 0; v.inSteer = -v.inSteer; }
    else if (base > 2 && std::fabs(v.fwdSpeed) < 0.5f) { v.stuck += dt; if (v.stuck > 3) { v.reverseT = 1.4f; v.stuck = 0; } }
    else v.stuck = std::max(0.0f, v.stuck - dt);
}

void Game::aiPolice(Vehicle& v, float dt) {
    V3 pp = playerPosOf(vehicles, pv, pos);
    float dist = dist2d(v.pos, pp);
    v.siren += dt;
    v.maxSpeed = 30.0f + stars * 2.8f;
    v.repath -= dt;
    if (v.repath <= 0) { v.repath = 0.3f; v.curNode = W->nearestNode(v.pos); }
    bool los = playerSeen && dist < 90;
    if (dist < 60) los = W->lineOfSight(v.pos + V3{0, 1.2f, 0}, pp + V3{0, 1.2f, 0});
    V3 target; float tspeed = v.maxSpeed;
    bool onFoot = pv < 0;
    if (onFoot && dist < 26 && los) { target = pp; tspeed = 0; }
    else if ((dist < 55 && los) || dist < 16) { target = pp + (pv >= 0 ? vehicles[pv].vel * 0.6f : V3{0, 0, 0}); }
    else {
        int n0 = v.curNode;
        if (n0 == playerNode) target = pp;
        else target = W->nodes[W->nextHop[n0][playerNode]].p;
    }
    float desired = std::atan2(target.x - v.pos.x, target.z - v.pos.z);
    float err = angleDiff(desired, v.yaw);
    v.inSteer = clampf(err * 2.0f, -1, 1);
    if (std::fabs(err) > 0.9f) tspeed = std::min(tspeed, 8.0f);
    V3 f = fwdOf(v.yaw), r = rightOf(v.yaw);
    for (const auto& o : vehicles) {
        if (&o == &v || o.drv == Vehicle::PLAYER) continue;
        V3 rel = o.pos - v.pos;
        float ahead = dot(rel, f), lat = dot(rel, r);
        if (ahead > 0 && ahead < 11 && std::fabs(lat) < 2.4f) { tspeed = std::min(tspeed, 9.0f); v.inSteer = clampf(v.inSteer - (lat >= 0 ? 0.9f : -0.9f), -1, 1); }
    }
    float spErr = tspeed - v.fwdSpeed;
    v.inThr = 0; v.inBrk = 0;
    if (tspeed <= 0.1f) v.inBrk = 0.9f;
    else if (spErr > 0.5f) v.inThr = 1.0f;
    else if (spErr < -1.5f) v.inBrk = clampf(-spErr * 0.2f, 0, 1);
    if (v.reverseT > 0) { v.reverseT -= dt; v.inThr = -1; v.inBrk = 0; v.inSteer = -v.inSteer; }
    else if (tspeed > 3 && std::fabs(v.fwdSpeed) < 1.0f) { v.stuck += dt; if (v.stuck > 1.5f) { v.reverseT = 1.3f; v.stuck = 0; } }
    else v.stuck = std::max(0.0f, v.stuck - dt);
}

void Game::updateVehicles(float dt) {
    for (int i = 0; i < (int)vehicles.size(); i++) {
        Vehicle& v = vehicles[i];
        if (v.drv != Vehicle::PLAYER) v.inHand = false;
        if (v.drv == Vehicle::TRAFFIC) aiTraffic(v, dt);
        else if (v.drv == Vehicle::POLICE) aiPolice(v, dt);
        else if (v.drv == Vehicle::NONE) { v.inThr = 0; v.inBrk = std::fabs(v.fwdSpeed) > 0.1f ? 0.5f : 0; v.inSteer = 0; }
        stepVehicle(v, dt);
        if (v.police) v.siren += (v.drv == Vehicle::POLICE || stars > 0) ? dt : 0;
    }
    // vehicle <-> vehicle collisions
    for (size_t i = 0; i < vehicles.size(); i++)
        for (size_t j = i + 1; j < vehicles.size(); j++) {
            Vehicle &a = vehicles[i], &b = vehicles[j];
            if (dist2d(a.pos, b.pos) > 6.5f) continue;
            float ra = a.halfWid + 0.08f, rb = b.halfWid + 0.08f;
            for (int ka = 0; ka < 3; ka++)
                for (int kb = 0; kb < 3; kb++) {
                    V3 ca = a.pos + fwdOf(a.yaw) * ((ka - 1) * (a.halfLen - ra));
                    V3 cb = b.pos + fwdOf(b.yaw) * ((kb - 1) * (b.halfLen - rb));
                    V3 d = ca - cb; d.y = 0;
                    float dist = len2d(d), rr = ra + rb;
                    if (dist >= rr || dist < 1e-4f) continue;
                    V3 n = d * (1.0f / dist);
                    float pen = rr - dist;
                    a.pos += n * (pen * 0.5f); b.pos -= n * (pen * 0.5f);
                    float rv = dot(a.vel - b.vel, n);
                    if (rv < 0) {
                        a.vel += n * (-rv * 0.65f); b.vel -= n * (-rv * 0.65f);
                        float impact = -rv;
                        if (impact > 4) {
                            damageVehicle(a, (impact - 3) * 2.2f); damageVehicle(b, (impact - 3) * 2.2f);
                            spawnParticles((ca + cb) * 0.5f + V3{0, 0.6f, 0}, 5, RGB(0xffd060, 1), 3.0f, 0.35f);
                            if ((a.drv == Vehicle::PLAYER || b.drv == Vehicle::PLAYER) && impact > 7) damagePlayer((impact - 7) * 1.6f);
                        }
                    }
                }
        }
    // cleanup: far traffic and stuck cars
    V3 pc = playerPosOf(vehicles, pv, pos);
    for (int i = (int)vehicles.size() - 1; i >= 0; i--) {
        Vehicle& v = vehicles[i];
        if (i == pv || i == enterTarget) continue;
        bool far = dist2d(v.pos, pc) > (v.drv == Vehicle::NONE && !v.police ? 700.0f : 360.0f);
        if (far && v.drv != Vehicle::NONE) removeVehicle(i);
        else if (far && v.drv == Vehicle::NONE && (v.hp <= 0 || v.police)) removeVehicle(i);
    }
    spawnT -= dt;
    if (spawnT <= 0) { spawnT = 0.8f; spawnTraffic(false); spawnPeds(false); }
}

void Game::removeVehicle(int i) {
    vehicles.erase(vehicles.begin() + i);
    if (pv > i) pv--;
    if (enterTarget > i) enterTarget--;
}

// ---------------------------------------------------------------- pedestrians
Ped Game::makePed(Ped::Type t, V3 p) {
    Ped q;
    q.type = t; q.pos = {p.x, GROUND, p.z};
    q.skin = SKINS[rng.i(5)]; q.hair = HAIRS[rng.i(5)]; q.shirt = SHIRTS[rng.i(8)]; q.pants = PANTS[rng.i(4)];
    q.speed = rng.range(1.2f, 1.7f); q.side = rng.chance(0.5f) ? 1.0f : -1.0f; q.hp = 60;
    if (t == Ped::COP) { q.shirt = RGB(0x22345f); q.pants = RGB(0x1a2238); q.hp = 90; }
    return q;
}

static float pedOffset(const RoadEdge& e) { return e.rural ? e.hw + 1.7f : e.hw + 2.2f; }

void Game::spawnPeds(bool initial) {
    int count = 0;
    for (const auto& p : peds) if (p.type == Ped::CIVIL && p.st != Ped::DEAD) count++;
    if (count >= 55) return;
    V3 pc = playerPosOf(vehicles, pv, pos);
    for (int tries = 0; tries < 40; tries++) {
        int e = rng.i((int)W->edges.size());
        const RoadEdge& ed = W->edges[e];
        float s = rng.range(2, std::max(3.0f, ed.length - 2));
        bool fw = rng.chance(0.5f);
        int from = fw ? ed.a : ed.b, to = fw ? ed.b : ed.a;
        float side = rng.chance(0.5f) ? 1.0f : -1.0f;
        V3 p = lanePoint(from, to, fw ? s : ed.length - s, side * pedOffset(ed));
        float d = dist2d(p, pc);
        if (initial ? (d < 15 || d > 190) : (d < 80 || d > 190)) continue;
        Ped q = makePed(Ped::CIVIL, p);
        q.from = from; q.to = to; q.next = pickNext(to, from); q.side = side;
        peds.push_back(q);
        return;
    }
}

void Game::snapPedToGraph(Ped& p) {
    float best = 1e9f; int be = -1;
    for (int i = 0; i < (int)W->edges.size(); i++) {
        const RoadEdge& e = W->edges[i];
        V3 a = W->nodes[e.a].p, d = e.dir;
        float s = clampf(dot(p.pos - a, d), 0, e.length);
        V3 q = a + d * s;
        float dd = dist2d(q, p.pos);
        if (dd < best) { best = dd; be = i; }
    }
    if (be < 0) return;
    const RoadEdge& e = W->edges[be];
    bool fw = rng.chance(0.5f);
    p.from = fw ? e.a : e.b; p.to = fw ? e.b : e.a; p.next = pickNext(p.to, p.from);
    V3 a = W->nodes[p.from].p, d = norm2d(W->nodes[p.to].p - a);
    p.side = dot(p.pos - a, V3{-d.z, 0, d.x}) >= 0 ? 1.0f : -1.0f;
}

void Game::hurtPed(Ped& p, float dmg, bool byPlayer, V3 from) {
    if (p.st == Ped::DEAD) return;
    p.hp -= dmg;
    spawnParticles(p.pos + V3{0, 1.2f, 0}, 5, RGB(0x9a1010), 2.2f, 0.5f);
    if (byPlayer) {
        if (p.type == Ped::COP) addStars(2);
        else if (p.type == Ped::CIVIL) addStars(1);
        else if (p.type == Ped::CLERK) addStars(2);
    }
    if (p.hp <= 0) {
        p.st = Ped::DEAD; p.deadT = 12;
        if (byPlayer) {
            if (p.type == Ped::COP) { addStars(std::min(5, std::max(stars + 1, 3))); pickups.push_back({p.pos, 1, 0, 0}); }
            else addStars(2);
        }
        if (p.type == Ped::CIVIL && rng.chance(0.8f)) pickups.push_back({p.pos, 0, rng.i(60) + 8, 0});
        if (p.type == Ped::CLERK && p.interior >= 0) {
            Interior& in = W->interiors[p.interior];
            if (in.cooldown <= 0) { pickups.push_back({in.registerPos, 0, 150 + rng.i(250), 0}); in.cooldown = 120; }
        }
    } else if (p.type == Ped::CIVIL) {
        p.st = Ped::FLEE; p.fleeFrom = from; p.timer = rng.range(5, 9);
    } else if (p.type == Ped::COP && p.st != Ped::CHASE && byPlayer) {
        p.st = Ped::CHASE; p.hostile = true;
    }
}

void Game::officerShoot(Ped& p) {
    V3 tgt = playerPosOf(vehicles, pv, pos) + V3{0, 1.1f, 0};
    V3 from = p.pos + V3{0, 1.4f, 0};
    V3 d = tgt - from;
    float dist = len(d);
    d = norm(d);
    float chance = clampf(0.55f - dist * 0.008f + (stars >= 4 ? 0.1f : 0.0f), 0.12f, 0.6f) * (pv >= 0 ? 0.6f : 1.0f) * (crouch ? 0.8f : 1.0f);
    bool hit = rng.chance(chance);
    V3 end = hit ? tgt : tgt + V3{rng.range(-1.6f, 1.6f), rng.range(-0.8f, 0.8f), rng.range(-1.6f, 1.6f)};
    tracers.push_back({from + d * 0.8f, end, 0.07f, RGB(0xffe070, 1)});
    if (hit) damagePlayer(3.5f + stars * 0.9f);
}

void Game::updatePeds(float dt) {
    V3 pc = playerPosOf(vehicles, pv, pos);
    for (auto& p : peds) {
        p.anim += dt;
        p.cd -= dt;
        if (p.st == Ped::DEAD) { p.deadT -= dt; if (p.deadT <= 0) p.remove = true; continue; }
        if (dist2d(p.pos, pc) > 380 && p.type != Ped::CLERK) { p.remove = true; continue; }
        // vehicles: flee from fast approaching cars, take damage when hit
        if (p.type != Ped::CLERK) {
            for (auto& v : vehicles) {
                float sp = std::fabs(v.fwdSpeed);
                if (sp < 2.0f) continue;
                V3 rel = p.pos - v.pos;
                float d = len2d(rel);
                if (d > 14) continue;
                if (sp > 6 && p.type == Ped::CIVIL && p.st != Ped::FLEE && dot(v.vel, rel) > 0) { p.st = Ped::FLEE; p.fleeFrom = v.pos; p.timer = rng.range(3, 5); }
                V3 pp = p.pos;
                if (pushOutOfVehicle(pp, 0.1f, v) && sp > 2.5f) {
                    hurtPed(p, sp * 6.0f, v.drv == Vehicle::PLAYER, v.pos);
                    p.pos += norm2d(rel) * 0.6f;
                    spawnParticles(p.pos + V3{0, 1, 0}, 4, RGB(0x9a1010), 3.0f, 0.5f);
                    if (p.st == Ped::DEAD) break;
                }
            }
            if (p.st == Ped::DEAD) continue;
        }
        switch (p.st) {
            case Ped::WALK: {
                int e = W->edgeBetween(p.from, p.to);
                if (e < 0) { snapPedToGraph(p); break; }
                const RoadEdge& ed = W->edges[e];
                V3 a = W->nodes[p.from].p, d = norm2d(W->nodes[p.to].p - a);
                float s = dot(p.pos - a, d), L = ed.length;
                if (s >= L - 0.4f) {
                    int prev = p.from; p.from = p.to; p.to = p.next;
                    p.next = pickNext(p.to, p.from);
                    if (p.to == prev) p.side = -p.side;
                    e = W->edgeBetween(p.from, p.to);
                    if (e < 0) { snapPedToGraph(p); break; }
                    a = W->nodes[p.from].p; d = norm2d(W->nodes[p.to].p - a);
                    s = dot(p.pos - a, d);
                }
                const RoadEdge& ed2 = W->edges[e];
                float remain = ed2.length - s, look = 1.8f;
                V3 target;
                int e2 = W->edgeBetween(p.to, p.next);
                if (look < remain || e2 < 0) target = lanePoint(p.from, p.to, std::min(s + look, ed2.length), p.side * pedOffset(ed2));
                else target = lanePoint(p.to, p.next, look - remain, p.side * pedOffset(W->edges[e2]));
                V3 dir = norm2d(target - p.pos);
                p.pos += dir * p.speed * dt;
                p.pos.y = GROUND;
                p.yaw = lerpAngle(p.yaw, std::atan2(dir.x, dir.z), smoothT(8, dt));
                break;
            }
            case Ped::FLEE: {
                V3 away = norm2d(p.pos - p.fleeFrom);
                if (len2d(away) < 0.01f) away = fwdOf(p.yaw);
                p.pos += away * 5.6f * dt;
                p.pos = W->pushCircle(p.pos, 0.35f, GROUND + 0.4f, GROUND + 1.7f);
                p.pos.y = GROUND;
                p.yaw = lerpAngle(p.yaw, std::atan2(away.x, away.z), smoothT(10, dt));
                p.timer -= dt;
                if (p.timer <= 0) { p.st = Ped::WALK; snapPedToGraph(p); }
                break;
            }
            case Ped::HANDSUP:
                p.timer -= dt;
                if (p.timer <= 0) p.st = Ped::IDLE;
                break;
            case Ped::CHASE: {
                if (dead) break;
                V3 tgt = pc;
                V3 d = tgt - p.pos; d.y = 0;
                float dist = len2d(d);
                V3 dir = norm2d(d);
                p.yaw = lerpAngle(p.yaw, std::atan2(dir.x, dir.z), smoothT(10, dt));
                float spd = 4.2f + stars * 0.4f;
                bool los = dist < 60 && W->lineOfSight(p.pos + V3{0, 1.5f, 0}, tgt + V3{0, 1.2f, 0});
                float keep = stars >= 3 ? 10.0f : 7.0f;
                if (dist > keep || !los) { p.pos += dir * spd * dt; }
                p.pos = W->pushCircle(p.pos, 0.38f, GROUND + 0.4f, GROUND + 1.7f);
                p.pos.y = GROUND;
                if (los && dist < 45 && p.cd <= 0) { p.cd = stars >= 4 ? 0.28f : 0.7f; officerShoot(p); }
                if (dist < 1.8f && p.cd <= 0) { p.cd = 0.8f; damagePlayer(8); }
                if (dist > 260) p.remove = true;
                break;
            }
            default: break;
        }
    }
    // clerks that are still near dead peds etc. are untouched; purge removed
    for (int i = (int)peds.size() - 1; i >= 0; i--)
        if (peds[i].remove) peds.erase(peds.begin() + i);
}

// ---------------------------------------------------------------- police
void Game::spawnPoliceCar() {
    V3 pc = playerPosOf(vehicles, pv, pos);
    for (int pass = 0; pass < 2; pass++)
        for (int tries = 0; tries < 60; tries++) {
            int e = rng.i((int)W->edges.size());
            const RoadEdge& ed = W->edges[e];
            float s = rng.range(3, std::max(4.0f, ed.length - 3));
            bool fw = rng.chance(0.5f);
            int from = fw ? ed.a : ed.b, to = fw ? ed.b : ed.a;
            V3 p = lanePoint(from, to, fw ? s : ed.length - s, ed.hw * 0.5f);
            float d = dist2d(p, pc);
            if (pass == 0 ? (d < 170 || d > 300) : (d < 100 || d > 600)) continue;
            bool clash = false;
            for (const auto& v : vehicles) if (dist2d(v.pos, p) < 8) clash = true;
            if (clash) continue;
            V3 dir = norm2d(W->nodes[to].p - W->nodes[from].p);
            Vehicle v = makeVehicle(p, std::atan2(dir.x, dir.z), 0, RGB(0xf2f2f2), true);
            v.drv = Vehicle::POLICE; v.from = from; v.to = to; v.vel = dir * 10.0f;
            v.curNode = from;
            vehicles.push_back(v);
            return;
        }
}

void Game::policeStandDown() {
    for (auto& v : vehicles)
        if (v.police && v.drv == Vehicle::POLICE) {
            v.drv = Vehicle::TRAFFIC;
            v.from = W->nearestNode(v.pos); v.to = pickNext(v.from, -1); v.next = pickNext(v.to, v.from);
        }
    for (auto& p : peds)
        if (p.type == Ped::COP && p.st == Ped::CHASE) { p.st = Ped::WALK; p.hostile = false; snapPedToGraph(p); }
}

void Game::updatePolice(float dt) {
    if (starFlash > 0) starFlash -= dt;
    if (stars <= 0) return;
    V3 pc = playerPosOf(vehicles, pv, pos);
    losTimer -= dt;
    if (losTimer <= 0) {
        losTimer = 0.25f;
        bool seen = false;
        V3 eye = pc + V3{0, 1.2f, 0};
        for (const auto& p : peds)
            if (p.type == Ped::COP && p.st != Ped::DEAD && dist2d(p.pos, pc) < 95 && W->lineOfSight(p.pos + V3{0, 1.5f, 0}, eye)) { seen = true; break; }
        if (!seen)
            for (const auto& v : vehicles)
                if (v.police && v.drv == Vehicle::POLICE && dist2d(v.pos, pc) < 95 && W->lineOfSight(v.pos + V3{0, 1.2f, 0}, eye)) { seen = true; break; }
        playerSeen = seen;
        playerNode = W->nearestNode(pc);
    }
    if (playerSeen) evade = 0; else evade += dt;
    if (evade >= 12.0f) {
        evade = 0; stars--;
        message(stars > 0 ? "WANTED LEVEL DECREASED" : "WANTED LEVEL CLEARED", 3);
        if (stars == 0) { policeStandDown(); return; }
    }
    int cars = 0;
    for (const auto& v : vehicles) if (v.police && v.drv == Vehicle::POLICE) cars++;
    if (cars < stars) {
        policeSpawnT -= dt;
        if (policeSpawnT <= 0) { policeSpawnT = std::max(1.2f, 3.2f - stars * 0.4f); spawnPoliceCar(); }
    }
    // officers step out when a cruiser reaches a player on foot
    for (int i = 0; i < (int)vehicles.size(); i++) {
        Vehicle& v = vehicles[i];
        if (!v.police || v.drv != Vehicle::POLICE || v.officersOut) continue;
        if (pv < 0 && dist2d(v.pos, pc) < 30 && std::fabs(v.fwdSpeed) < 3.0f) {
            v.officersOut = true; v.drv = Vehicle::NONE;
            int officers = 0;
            for (const auto& p : peds) if (p.type == Ped::COP && p.st == Ped::CHASE) officers++;
            for (int k = 0; k < 2 && officers + k < 4 + stars * 2; k++) {
                V3 side = rightOf(v.yaw) * ((k == 0 ? -1.0f : 1.0f) * (v.halfWid + 0.9f));
                Ped o = makePed(Ped::COP, v.pos + side);
                o.st = Ped::CHASE; o.hostile = true; o.cd = rng.range(0.4f, 1.2f);
                peds.push_back(o);
            }
        }
    }
    // patrol officers nearby join the chase
    for (auto& p : peds)
        if (p.type == Ped::COP && p.st == Ped::WALK && dist2d(p.pos, pc) < 90) { p.st = Ped::CHASE; p.hostile = true; }
}
