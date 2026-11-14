#pragma once
#include "renderer.h"
#include "world.h"
#include <string>
#include <vector>

enum Key {
    K_W, K_A, K_S, K_D, K_UP, K_DOWN, K_LEFT, K_RIGHT, K_SHIFT, K_SPACE, K_C, K_E, K_F, K_R, K_V,
    K_1, K_2, K_3, K_Z, K_X, K_M, K_F5, K_F9, K_ESC, K_Q, K_COUNT
};
struct Input {
    bool down[K_COUNT] = {};
    bool pressed[K_COUNT] = {};
    float mdx = 0, mdy = 0;
    bool lmb = false, rmb = false, lmbPressed = false;
};

enum WeaponId { W_FISTS = 0, W_PISTOL = 1, W_SMG = 2 };
struct WeaponDef { const char* name; float dmg, cd, spread, reload; int mag; bool melee, automatic; };
extern const WeaponDef WEAPONS[3];

struct Vehicle {
    enum Driver { NONE, PLAYER, TRAFFIC, POLICE };
    V3 pos, vel;
    float yaw = 0, yawRate = 0, steer = 0, hp = 100, fwdSpeed = 0, wheelRot = 0;
    int style = 0;               // 0 sedan, 1 coupe, 2 van
    bool police = false;
    Col color{1, 1, 1, 0};
    Driver drv = NONE;
    float inThr = 0, inBrk = 0, inSteer = 0;
    bool inHand = false;
    float maxSpeed = 32, halfLen = 2.3f, halfWid = 0.95f;
    int from = -1, to = -1, next = -1;
    float stuck = 0, reverseT = 0, siren = 0, repath = 0, noseen = 0;
    int curNode = 0;
    bool officersOut = false, remove = false;
};

struct Ped {
    enum Type { CIVIL, COP, CLERK };
    enum State { WALK, FLEE, DEAD, IDLE, CHASE, HANDSUP };
    V3 pos, vel, fleeFrom;
    float yaw = 0, hp = 60, anim = 0, timer = 0, cd = 0, deadT = 0, speed = 1.4f, side = 1;
    Type type = CIVIL;
    State st = WALK;
    Col shirt{1, 1, 1, 0}, pants{0, 0, 1, 0}, skin{1, 0.8f, 0.6f, 0}, hair{0.2f, 0.1f, 0.05f, 0};
    int from = -1, to = -1, next = -1;
    int interior = -1;
    bool hostile = false;
    bool remove = false;
};

struct Tracer { V3 a, b; float t; Col c; };
struct Particle { V3 p, v; float life, maxLife, size; Col c; float grav; };
struct Pickup { V3 pos; int kind; int value; float bob; };  // kind 0 = cash bag, 1 = ammo
struct Msg { std::string text; float t; };

struct HitInfo { float t = 0; int ped = -1, veh = -1; bool head = false; V3 point; bool any = false; };

class Game {
public:
    void init(World* w);
    void update(float dt, Input& in);
    void render(Renderer& r);
    bool paused = false, quitRequested = false;
    bool mouseCaptured() const { return !paused; }

private:
    World* W = nullptr;
    float time = 0;
    // player
    V3 pos, vel;
    float yaw = 0, camYaw = 0, camPitch = -0.2f, health = 100, armor = 0, vy = 0;
    int cash = 250, weapon = W_PISTOL, mag[3] = {0, 12, 30}, reserve[3] = {0, 48, 90};
    bool onGround = true, crouch = false, aiming = false, moving = false;
    float fireCD = 0, reloadT = 0, meleeT = 0, muzzleFlash = 0, crouchT = 0, dmgFlash = 0, deadTimer = 0, lastMouse = 0, shake = 0;
    bool dead = false;
    int pv = -1;                 // vehicle the player is driving
    float enterT = 0; int enterTarget = -1; V3 enterFrom;
    float exitT = 0;
    // wanted
    int stars = 0;
    float evade = 0, losTimer = 0, policeSpawnT = 0, starFlash = 0;
    bool playerSeen = false;
    int playerNode = 0;
    // robbery
    int robIdx = -1; float robT = 0; bool heist = false;
    std::string prompt;
    // camera
    V3 camPos, camPivot, aimPoint, camDir;
    float curDist = 4, curFov = 66, curShoulder = 0.45f, camFov = 66;
    // world state
    std::vector<Vehicle> vehicles;
    std::vector<Ped> peds;
    std::vector<Tracer> tracers;
    std::vector<Particle> particles;
    std::vector<Pickup> pickups;
    std::vector<Msg> msgs;
    MeshBuilder dyn;
    Rng rng{777};
    bool mapZoomFar = false;
    float spawnT = 0, autosaveT = 0;
    bool camFar = false;
    float walkPhase = 0, helpT = 0, aimT = 0;

    // helpers
    void message(const std::string& s, float t = 3.0f);
    void addStars(int minStars);
    void damagePlayer(float d);
    void die();
    void respawn();
    void noise(V3 p, float radius);
    V3 muzzlePos() const;
    void updatePickups(float dt);
    void updateEffects(float dt);
    void drawSignals();
    // vehicles
    Vehicle makeVehicle(V3 p, float yaw, int style, Col c, bool police);
    int nearestVehicle(float maxDist) const;
    void stepVehicle(Vehicle& v, float dt);
    void aiTraffic(Vehicle& v, float dt);
    void aiPolice(Vehicle& v, float dt);
    void updateVehicles(float dt);
    void spawnTraffic(bool initial);
    int pickNext(int node, int prev);
    V3 lanePoint(int from, int to, float s, float off) const;
    int signalState(int node, bool axisX) const;
    void damageVehicle(Vehicle& v, float d);
    void removeVehicle(int i);
    void enterVehicle(int i);
    void exitVehicle();
    // peds
    void updatePeds(float dt);
    void spawnPeds(bool initial);
    void snapPedToGraph(Ped& p);
    void hurtPed(Ped& p, float dmg, bool byPlayer, V3 from);
    Ped makePed(Ped::Type t, V3 p);
    void officerShoot(Ped& p);
    // police
    void updatePolice(float dt);
    void spawnPoliceCar();
    void policeStandDown();
    // player
    void updatePlayerFoot(float dt, Input& in);
    void updatePlayerDrive(float dt, Input& in);
    void updateCamera(float dt, Input& in);
    void updateInteractions(float dt, Input& in);
    void fireWeapon();
    void meleeAttack(float dmg);
    void startReload();
    bool castRay(V3 o, V3 d, float maxT, HitInfo& h) const;
    void spawnParticles(V3 p, int n, Col c, float speed, float life, float grav = 6.0f);
    bool pushOutOfVehicle(V3& p, float r, const Vehicle& v) const;
    // persistence
    void save();
    void load();
    std::string savePath() const;
    // drawing
    void drawVehicle(const Vehicle& v);
    void drawPed(const Ped& p);
    void drawPlayerModel();
    void drawHUD(Renderer& r);
    void drawMinimap(Renderer& r, float ui);
};
