// Player control, weapons, camera, interactions, persistence and the main update loop.
#include "game.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

static const float PLAYER_R = 0.38f;

static int clampi(int v, int a, int b) { return v < a ? a : (v > b ? b : v); }
static bool finite3(V3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

// ---------------------------------------------------------------- ray casting
bool Game::castRay(V3 o, V3 d, float maxT, HitInfo& h) const {
    h = HitInfo();
    float best = maxT, tw = 0;
    if (W->raycast(o, d, maxT, tw)) { best = tw; h.any = true; h.t = tw; h.point = o + d * tw; }
    // vehicles: oriented boxes
    for (int i = 0; i < (int)vehicles.size(); i++) {
        if (i == pv) continue;
        const Vehicle& v = vehicles[i];
        if (dist2d(v.pos, o) > best + 6.0f) continue;
        V3 rel = o - v.pos, f = fwdOf(v.yaw), rt = rightOf(v.yaw);
        const float lo[3] = {dot(rel, rt), rel.y, dot(rel, f)};
        const float ld[3] = {dot(d, rt), d.y, dot(d, f)};
        const float mn[3] = {-v.halfWid, 0.0f, -v.halfLen}, mx[3] = {v.halfWid, 1.5f, v.halfLen};
        float t0 = 0, t1 = best;
        bool ok = true;
        for (int a = 0; a < 3 && ok; a++) {
            if (std::fabs(ld[a]) < 1e-8f) { if (lo[a] < mn[a] || lo[a] > mx[a]) ok = false; }
            else {
                float ta = (mn[a] - lo[a]) / ld[a], tb = (mx[a] - lo[a]) / ld[a];
                if (ta > tb) std::swap(ta, tb);
                t0 = std::max(t0, ta); t1 = std::min(t1, tb);
                if (t0 > t1) ok = false;
            }
        }
        if (ok && t0 < best) { best = t0; h.any = true; h.t = t0; h.point = o + d * t0; h.veh = i; h.ped = -1; h.head = false; }
    }
    // pedestrians: upright cylinders
    for (int i = 0; i < (int)peds.size(); i++) {
        const Ped& p = peds[i];
        if (p.st == Ped::DEAD) continue;
        float ox = o.x - p.pos.x, oz = o.z - p.pos.z;
        float a = d.x * d.x + d.z * d.z;
        if (a < 1e-8f) continue;
        float b = ox * d.x + oz * d.z, c = ox * ox + oz * oz - 0.45f * 0.45f;
        float disc = b * b - a * c;
        if (disc < 0) continue;
        float t = (-b - std::sqrt(disc)) / a;
        if (t < 0 || t >= best) continue;
        float y = o.y + d.y * t;
        if (y < p.pos.y || y > p.pos.y + 1.85f) continue;
        best = t; h.any = true; h.t = t; h.point = o + d * t; h.ped = i; h.veh = -1; h.head = y > p.pos.y + 1.5f;
    }
    return h.any;
}

// ---------------------------------------------------------------- weapons
V3 Game::muzzlePos() const {
    float h = 1.35f - crouchT * 0.45f;
    return pos + V3{0, h, 0} + rightOf(yaw) * 0.37f + fwdOf(yaw) * 0.75f;
}

void Game::startReload() {
    const WeaponDef& wd = WEAPONS[weapon];
    if (wd.melee || reloadT > 0 || mag[weapon] >= wd.mag || reserve[weapon] <= 0) return;
    reloadT = wd.reload;
}

void Game::meleeAttack(float dmg) {
    meleeT = 0.3f;
    V3 f = fwdOf(yaw);
    int best = -1;
    float bd = 2.0f;
    for (int i = 0; i < (int)peds.size(); i++) {
        const Ped& p = peds[i];
        if (p.st == Ped::DEAD) continue;
        V3 d = p.pos - pos;
        float L = len2d(d);
        if (L > bd) continue;
        if (L > 0.4f && dot(norm2d(d), f) < 0.35f) continue;
        bd = L; best = i;
    }
    if (best >= 0) {
        hurtPed(peds[best], dmg, true, pos);
        if (peds[best].st != Ped::DEAD) peds[best].pos += f * 0.4f;
    }
    noise(pos, 12);
}

void Game::fireWeapon() {
    const WeaponDef& wd = WEAPONS[weapon];
    if (wd.melee) { meleeAttack(wd.dmg); fireCD = wd.cd; return; }
    if (reloadT > 0) return;
    if (mag[weapon] <= 0) {
        if (reserve[weapon] > 0) startReload();
        else { fireCD = 0.35f; message("OUT OF AMMO", 1.5f); }
        return;
    }
    mag[weapon]--;
    fireCD = wd.cd; muzzleFlash = 0.06f;
    shake = std::min(0.5f, shake + 0.02f);
    V3 m = muzzlePos();
    V3 dir = aimPoint - m;
    if (len(dir) < 2.0f) dir = camDir;
    dir = norm(dir);
    float sp = wd.spread * (aiming ? 0.45f : 1.0f) * (moving ? 1.6f : 1.0f) * (crouch ? 0.7f : 1.0f);
    dir = norm(dir + V3{rng.range(-1, 1) * sp, rng.range(-1, 1) * sp, rng.range(-1, 1) * sp});
    HitInfo h;
    castRay(m, dir, 180.0f, h);
    V3 end = h.any ? h.point : m + dir * 120.0f;
    tracers.push_back({m + dir * 0.5f, end, 0.06f, RGB(0xffe9a0, 1)});
    noise(pos, 55);
    // any officer who sees the shot reports it
    if (stars < 1) {
        V3 eye = pos + V3{0, 1.2f, 0};
        for (const auto& p : peds)
            if (p.type == Ped::COP && p.st != Ped::DEAD && dist2d(p.pos, pos) < 50 && W->lineOfSight(p.pos + V3{0, 1.5f, 0}, eye)) { addStars(1); break; }
    }
    if (h.ped >= 0) {
        hurtPed(peds[h.ped], wd.dmg * (h.head ? 3.0f : 1.0f), true, pos);
    } else if (h.veh >= 0) {
        Vehicle& v = vehicles[h.veh];
        damageVehicle(v, wd.dmg * 0.5f);
        spawnParticles(end, 4, RGB(0xffd060, 1), 3.0f, 0.3f);
        if (v.drv == Vehicle::TRAFFIC || v.drv == Vehicle::POLICE) addStars(v.drv == Vehicle::POLICE ? 2 : 1);
    } else if (h.any) {
        spawnParticles(end, 3, RGB(0xd8d0b0), 2.5f, 0.3f);
    }
}

// ---------------------------------------------------------------- vehicles (player side)
void Game::enterVehicle(int i) {
    if (i < 0 || i >= (int)vehicles.size() || pv >= 0 || enterTarget >= 0) return;
    Vehicle& v = vehicles[i];
    if (v.hp <= 0) { message("VEHICLE IS WRECKED", 2); return; }
    V3 door = v.pos - rightOf(v.yaw) * (v.halfWid + 0.9f);
    if (v.drv == Vehicle::TRAFFIC || v.drv == Vehicle::POLICE) {
        bool cop = v.drv == Vehicle::POLICE;
        Ped c = makePed(cop ? Ped::COP : Ped::CIVIL, door);
        if (cop) { c.st = Ped::CHASE; c.hostile = true; c.cd = 1.0f; }
        else { c.st = Ped::FLEE; c.fleeFrom = pos; c.timer = 7; }
        c.yaw = v.yaw;
        peds.push_back(c);
        addStars(cop ? 3 : 2);
        message("CARJACKED", 2);
    }
    v.drv = Vehicle::NONE;
    enterTarget = i; enterT = 0.4f; enterFrom = pos;
}

void Game::exitVehicle() {
    if (pv < 0) return;
    Vehicle& v = vehicles[pv];
    V3 f = fwdOf(v.yaw), rt = rightOf(v.yaw);
    V3 cand[4] = {v.pos - rt * (v.halfWid + 0.9f), v.pos + rt * (v.halfWid + 0.9f), v.pos - f * (v.halfLen + 1.0f), v.pos + f * (v.halfLen + 1.0f)};
    V3 chosen = cand[0];
    bool found = false;
    for (int k = 0; k < 4 && !found; k++) {
        V3 c = cand[k]; c.y = GROUND;
        V3 pc = W->pushCircle(c, PLAYER_R, GROUND + 0.4f, GROUND + 1.7f);
        if (dist2d(pc, c) < 0.05f) { chosen = c; found = true; }
    }
    if (!found) chosen = W->pushCircle(cand[0], PLAYER_R, GROUND + 0.4f, GROUND + 1.7f);
    float sp = std::fabs(v.fwdSpeed);
    v.drv = Vehicle::NONE; v.inThr = 0; v.inBrk = 0.4f; v.inSteer = 0; v.inHand = false;
    pos = chosen; pos.y = GROUND; vel = {0, 0, 0}; vy = 0; onGround = true;
    pv = -1; exitT = 0.5f; yaw = v.yaw;
    if (sp > 9.0f) damagePlayer((sp - 9.0f) * 1.2f);  // after pv is cleared: die() is safe
}

void Game::updatePlayerDrive(float dt, Input& in) {
    (void)dt;
    Vehicle& v = vehicles[pv];
    float thr = 0;
    if (in.down[K_W] || in.down[K_UP]) thr += 1;
    if (in.down[K_S] || in.down[K_DOWN]) thr -= 1;
    float st = 0;
    if (in.down[K_A] || in.down[K_LEFT]) st += 1;    // positive steer = turn left (yaw increases)
    if (in.down[K_D] || in.down[K_RIGHT]) st -= 1;
    v.inThr = thr; v.inBrk = 0; v.inSteer = st;
    v.inHand = in.down[K_SPACE];
    pos = v.pos; yaw = v.yaw; moving = std::fabs(v.fwdSpeed) > 0.5f;
    if (v.hp <= 0) {
        message("VEHICLE WRECKED", 2);
        exitVehicle();
    }
}

// ---------------------------------------------------------------- on foot
void Game::updatePlayerFoot(float dt, Input& in) {
    int nw = weapon;
    if (in.pressed[K_1]) nw = 0;
    if (in.pressed[K_2]) nw = 1;
    if (in.pressed[K_3]) nw = 2;
    if (in.pressed[K_Q]) nw = (weapon + 1) % 3;
    if (nw != weapon) { weapon = nw; reloadT = 0; fireCD = 0.25f; }

    crouch = in.down[K_C];
    crouchT += ((crouch ? 1.0f : 0.0f) - crouchT) * smoothT(12, dt);
    aiming = in.rmb && weapon != W_FISTS;

    V3 f = fwdOf(camYaw), r = rightOf(camYaw);
    float fw = ((in.down[K_W] || in.down[K_UP]) ? 1.0f : 0.0f) - ((in.down[K_S] || in.down[K_DOWN]) ? 1.0f : 0.0f);
    float rt = ((in.down[K_D] || in.down[K_RIGHT]) ? 1.0f : 0.0f) - ((in.down[K_A] || in.down[K_LEFT]) ? 1.0f : 0.0f);
    V3 wish = f * fw + r * rt;
    moving = len2d(wish) > 0.1f;
    wish = norm2d(wish);
    bool sprint = in.down[K_SHIFT] && !crouch && !aiming;
    float sp = crouch ? 2.0f : (aiming ? 3.0f : (sprint ? 7.2f : 4.2f));
    float k = smoothT(onGround ? 14.0f : 3.0f, dt);
    vel.x += (wish.x * sp - vel.x) * k;
    vel.z += (wish.z * sp - vel.z) * k;

    V3 np = pos;
    np.x += vel.x * dt; np.z += vel.z * dt;
    np = W->pushCircle(np, PLAYER_R, pos.y + 0.4f, pos.y + 1.7f);
    for (const auto& v : vehicles)
        if (pos.y < v.pos.y + 1.5f) pushOutOfVehicle(np, PLAYER_R, v);
    np = W->pushCircle(np, PLAYER_R, pos.y + 0.4f, pos.y + 1.7f);
    np.x = clampf(np.x, -440.0f, 820.0f); np.z = clampf(np.z, -330.0f, 830.0f);

    float gh = W->groundHeight(np.x, np.z, pos.y);
    if (in.pressed[K_SPACE] && onGround && !crouch) { vy = 6.2f; onGround = false; }
    vy -= 20.0f * dt;
    float ny = pos.y + vy * dt;
    if (ny <= gh) {
        if (vy < -12.0f) damagePlayer((-vy - 12.0f) * 3.0f);
        ny = gh; vy = 0; onGround = true;
    } else onGround = false;
    pos = {np.x, ny, np.z};

    walkPhase += len2d(vel) * dt * 2.3f;
    aimT = std::max(0.0f, aimT - dt);
    if (aiming || aimT > 0) yaw = lerpAngle(yaw, camYaw, smoothT(20, dt));
    else if (moving) yaw = lerpAngle(yaw, std::atan2(wish.x, wish.z), smoothT(12, dt));

    // weapon use
    const WeaponDef& wd = WEAPONS[weapon];
    bool trig = (wd.melee || wd.automatic) ? in.lmb : in.lmbPressed;
    if (trig && fireCD <= 0 && exitT <= 0 && reloadT <= 0) { fireWeapon(); aimT = 0.8f; }
    if (in.pressed[K_R]) startReload();
    if (!wd.melee && mag[weapon] <= 0 && reserve[weapon] > 0 && reloadT <= 0) startReload();
}

// ---------------------------------------------------------------- interactions
void Game::updateInteractions(float dt, Input& in) {
    (void)dt;
    prompt.clear();
    if (pv >= 0) {
        prompt = "F: EXIT VEHICLE";
        if (in.pressed[K_F] && exitT <= 0) exitVehicle();
        return;
    }
    if (enterTarget >= 0) return;

    int ni = nearestVehicle(2.6f);
    if (ni >= 0 && vehicles[ni].hp > 0) {
        prompt = (vehicles[ni].drv == Vehicle::NONE) ? "F: ENTER VEHICLE" : "F: CARJACK";
        if (in.pressed[K_F] && exitT <= 0) { enterVehicle(ni); return; }
    }

    int idx = W->insideInterior(pos);
    if (robIdx >= 0) {
        Interior& it = W->interiors[robIdx];
        bool bank = it.kind == Interior::BANK;
        float need = bank ? 12.0f : 5.0f, rad = bank ? 4.0f : 3.2f;
        if (idx != robIdx || dist2d(pos, it.registerPos) > rad) {
            message("ROBBERY ABORTED", 2.5f);
            robIdx = -1; robT = 0; heist = false;
        } else {
            robT += dt;
            evade = 0;  // cops cannot lose you mid-robbery
            prompt = bank ? "CLEANING OUT THE VAULT" : "EMPTYING THE REGISTER";
            if (robT >= need) {
                int take = bank ? 4000 + rng.i(3000) : 300 + rng.i(400);
                cash += take;
                it.cooldown = bank ? 300.0f : 180.0f;
                message("ROBBERY COMPLETE: $" + std::to_string(take), 4);
                if (bank) message("NOW ESCAPE THE POLICE", 4);
                robIdx = -1; robT = 0; heist = bank;
            }
        }
        return;
    }
    if (idx >= 0) {
        Interior& it = W->interiors[idx];
        bool bank = it.kind == Interior::BANK;
        float rad = bank ? 4.0f : 3.2f;
        if (dist2d(pos, it.registerPos) < rad) {
            if (it.cooldown > 0) prompt = "NOTHING LEFT TO TAKE";
            else if (weapon == W_FISTS) prompt = "EQUIP A WEAPON TO ROB";
            else {
                prompt = bank ? "E: ROB THE VAULT" : "E: ROB THE REGISTER";
                if (in.pressed[K_E]) {
                    robIdx = idx; robT = 0; heist = false;
                    addStars(bank ? 4 : 2);
                    noise(it.registerPos, 30);
                    for (auto& p : peds)
                        if (p.type == Ped::CLERK && p.interior == idx && p.st != Ped::DEAD) { p.st = Ped::HANDSUP; p.timer = 40; }
                    message(bank ? "BANK ROBBERY IN PROGRESS" : "ROBBERY IN PROGRESS", 3);
                }
            }
        }
    }
}

// ---------------------------------------------------------------- camera
void Game::updateCamera(float dt, Input& in) {
    bool driving = pv >= 0;
    if (!dead && enterTarget < 0) {
        float sens = 0.0024f * (aiming ? 0.7f : 1.0f);
        camYaw = wrapAngle(camYaw - in.mdx * sens);
        camPitch -= in.mdy * sens;
        if (std::fabs(in.mdx) + std::fabs(in.mdy) > 0.5f) lastMouse = 0; else lastMouse += dt;
    }
    camPitch = clampf(camPitch, driving ? -0.6f : -1.15f, driving ? 0.5f : 0.9f);
    if (driving) {
        const Vehicle& v = vehicles[pv];
        float target = v.yaw;
        if (in.down[K_Z]) { target = v.yaw + PI_F; lastMouse = 0; }
        else if (v.fwdSpeed < -1.0f) target = v.yaw + PI_F;
        if ((lastMouse > 1.2f && std::fabs(v.fwdSpeed) > 2.0f) || in.down[K_Z])
            camYaw = lerpAngle(camYaw, target, smoothT(in.down[K_Z] ? 8.0f : 2.2f, dt));
        if (lastMouse > 1.2f) camPitch += (-0.16f - camPitch) * smoothT(2.0f, dt);
    }
    float tDist, tShoulder, tFov;
    if (driving) { tDist = (camFar ? 11.0f : 7.2f) + std::fabs(vehicles[pv].fwdSpeed) * 0.08f; tShoulder = 0; tFov = 68.0f + clampf(std::fabs(vehicles[pv].fwdSpeed) * 0.35f, 0, 12); }
    else if (aiming) { tDist = 2.3f; tShoulder = 0.75f; tFov = 52.0f; }
    else { tDist = camFar ? 6.0f : 3.7f; tShoulder = 0.5f; tFov = 66.0f; }
    float k = smoothT(9, dt);
    curDist += (tDist - curDist) * k;
    curShoulder += (tShoulder - curShoulder) * k;
    curFov += (tFov - curFov) * k;
    camFov = curFov;

    V3 pivotT = driving ? vehicles[pv].pos + V3{0, 1.5f, 0} : pos + V3{0, 1.55f - crouchT * 0.5f, 0};
    camPivot = pivotT;
    float cp = std::cos(camPitch);
    camDir = {std::sin(camYaw) * cp, std::sin(camPitch), std::cos(camYaw) * cp};
    V3 desired = camPivot - camDir * curDist + rightOf(camYaw) * curShoulder;
    V3 toCam = desired - camPivot;
    float L = len(toCam);
    if (L > 0.01f) {
        V3 dn = toCam * (1.0f / L);
        float t;
        if (W->raycast(camPivot, dn, L + 0.3f, t)) desired = camPivot + dn * std::max(0.35f, t - 0.3f);
    }
    if (desired.y < GROUND + 0.25f) desired.y = GROUND + 0.25f;
    shake = std::max(0.0f, shake - dt * 1.8f);
    if (shake > 0.001f) desired += V3{rng.range(-1, 1), rng.range(-1, 1), rng.range(-1, 1)} * (shake * 0.12f);
    camPos = desired;

    // aim point: ray through the screen centre, started just beyond the player
    V3 o = camPos + camDir * (curDist + 0.6f);
    HitInfo h;
    if (castRay(o, camDir, 200.0f, h)) aimPoint = h.point; else aimPoint = o + camDir * 200.0f;
}

// ---------------------------------------------------------------- pickups & effects
void Game::updatePickups(float dt) {
    V3 pc = pv >= 0 ? vehicles[pv].pos : pos;
    for (int i = (int)pickups.size() - 1; i >= 0; i--) {
        Pickup& p = pickups[i];
        p.bob += dt;
        if (!dead && dist2d(p.pos, pc) < 1.6f) {
            if (p.kind == 0) { cash += p.value; message("+$" + std::to_string(p.value), 2); }
            else { reserve[W_PISTOL] += 12; reserve[W_SMG] += 20; message("AMMO COLLECTED", 2); }
            pickups.erase(pickups.begin() + i);
        }
    }
}

void Game::updateEffects(float dt) {
    for (int i = (int)msgs.size() - 1; i >= 0; i--) { msgs[i].t -= dt; if (msgs[i].t <= 0) msgs.erase(msgs.begin() + i); }
    for (int i = (int)tracers.size() - 1; i >= 0; i--) { tracers[i].t -= dt; if (tracers[i].t <= 0) tracers.erase(tracers.begin() + i); }
    for (int i = (int)particles.size() - 1; i >= 0; i--) {
        Particle& q = particles[i];
        q.life -= dt;
        if (q.life <= 0) { particles.erase(particles.begin() + i); continue; }
        q.v.y -= q.grav * dt;
        q.p += q.v * dt;
        if (q.p.y < GROUND) { q.p.y = GROUND; q.v = q.v * 0.3f; q.v.y = 0; }
    }
}

// ---------------------------------------------------------------- persistence
std::string Game::savePath() const {
    const char* home = std::getenv("HOME");
    std::string dir = std::string(home ? home : ".") + "/Library/Application Support/PalmHeat";
    return dir + "/save.txt";
}

void Game::save() {
    const char* home = std::getenv("HOME");
    std::string base = std::string(home ? home : ".") + "/Library";
    mkdir(base.c_str(), 0755);
    base += "/Application Support"; mkdir(base.c_str(), 0755);
    base += "/PalmHeat"; mkdir(base.c_str(), 0755);
    FILE* f = std::fopen(savePath().c_str(), "w");
    if (!f) { message("SAVE FAILED", 2); return; }
    V3 p = pv >= 0 ? vehicles[pv].pos - rightOf(vehicles[pv].yaw) * (vehicles[pv].halfWid + 1.2f) : pos;
    std::fprintf(f, "PALMHEAT1 %.3f %.3f %.3f %.4f %.2f %.2f %d %d %d %d %d %d %d %d\n", p.x, p.y, p.z, yaw, health, armor, cash, weapon,
                 mag[0], mag[1], mag[2], reserve[0], reserve[1], reserve[2]);
    std::fclose(f);
}

void Game::load() {
    FILE* f = std::fopen(savePath().c_str(), "r");
    if (!f) { message("NO SAVE FOUND", 2); return; }
    char tag[16] = {};
    float x, y, z, yw, hp, ar;
    int c, w, m0, m1, m2, r0, r1, r2;
    int n = std::fscanf(f, "%15s %f %f %f %f %f %f %d %d %d %d %d %d %d %d", tag, &x, &y, &z, &yw, &hp, &ar, &c, &w, &m0, &m1, &m2, &r0, &r1, &r2);
    std::fclose(f);
    if (n != 15 || std::strcmp(tag, "PALMHEAT1") != 0 || !std::isfinite(x + y + z + yw + hp + ar)) { message("SAVE FILE INVALID", 2); return; }
    if (pv >= 0) { vehicles[pv].drv = Vehicle::NONE; vehicles[pv].inThr = 0; pv = -1; }
    enterTarget = -1; enterT = 0;
    pos = {clampf(x, -440, 820), GROUND, clampf(z, -330, 830)};
    pos = W->pushCircle(pos, PLAYER_R, GROUND + 0.4f, GROUND + 1.7f);
    vel = {0, 0, 0}; vy = 0; yaw = yw; camYaw = yw;
    health = clampf(hp, 1, 100); armor = clampf(ar, 0, 100); cash = std::max(0, c);
    weapon = std::min(2, std::max(0, w));
    (void)m0; (void)r0;
    mag[0] = 0; mag[1] = clampi(m1, 0, 12); mag[2] = clampi(m2, 0, 30);
    reserve[0] = 0; reserve[1] = std::max(0, r1); reserve[2] = std::max(0, r2);
    dead = false; stars = 0; evade = 0; robIdx = -1; robT = 0; reloadT = 0;
    policeStandDown();
    message("GAME LOADED", 2);
}

// ---------------------------------------------------------------- main update
void Game::update(float dt, Input& in) {
    dt = clampf(dt, 0.0f, 0.05f);
    if (in.pressed[K_ESC]) paused = !paused;
    if (paused) {
        if (in.pressed[K_Q]) quitRequested = true;
        if (in.pressed[K_F5]) save();
        if (in.pressed[K_F9]) load();
        return;
    }
    time += dt; helpT += dt;
    fireCD -= dt; meleeT -= dt; muzzleFlash -= dt;
    dmgFlash = std::max(0.0f, dmgFlash - dt * 1.4f);
    if (exitT > 0) exitT -= dt;
    if (reloadT > 0) {
        reloadT -= dt;
        if (reloadT <= 0) {
            reloadT = 0;
            int n = std::min(WEAPONS[weapon].mag - mag[weapon], reserve[weapon]);
            mag[weapon] += n; reserve[weapon] -= n;
        }
    }
    for (auto& it : W->interiors) if (it.cooldown > 0) it.cooldown -= dt;
    if (in.pressed[K_M]) mapZoomFar = !mapZoomFar;
    if (in.pressed[K_V]) camFar = !camFar;
    if (in.pressed[K_F5] && !dead) { save(); message("GAME SAVED", 2); }
    if (in.pressed[K_F9]) load();

    if (dead) {
        deadTimer -= dt;
        if (deadTimer <= 0) respawn();
    } else if (enterTarget >= 0) {
        enterT -= dt;
        Vehicle& v = vehicles[enterTarget];
        V3 door = v.pos - rightOf(v.yaw) * (v.halfWid + 0.1f);
        float k = 1.0f - clampf(enterT / 0.4f, 0, 1);
        pos = lerp(enterFrom, door, k); pos.y = GROUND;
        if (enterT <= 0) {
            pv = enterTarget; enterTarget = -1;
            v.drv = Vehicle::PLAYER; v.inThr = 0; v.inBrk = 0; v.inSteer = 0;
            pos = v.pos; vel = {0, 0, 0};
            camYaw = v.yaw;
        }
    } else if (pv >= 0) {
        updatePlayerDrive(dt, in);
    } else {
        updatePlayerFoot(dt, in);
    }
    if (!dead) updateInteractions(dt, in);

    updateVehicles(dt);
    if (pv >= 0 && !dead) { pos = vehicles[pv].pos; yaw = vehicles[pv].yaw; }
    updatePeds(dt);
    updatePolice(dt);
    updatePickups(dt);
    updateEffects(dt);
    updateCamera(dt, in);

    if (!finite3(pos)) { pos = {9.0f, GROUND, 36.0f}; vel = {0, 0, 0}; vy = 0; }

    autosaveT += dt;
    if (autosaveT > 90.0f) {
        autosaveT = 0;
        if (!dead && stars == 0 && pv < 0 && robIdx < 0) save();
    }
}
