#pragma once
#include "math3d.h"
#include "renderer.h"
#include <string>
#include <vector>

constexpr float GROUND = 0.12f;  // walking height everywhere (lot slab top)

struct AABB { V3 mn, mx; };

struct RoadNode {
    V3 p;
    std::vector<int> edges;
    bool signal = false;
};
struct RoadEdge {
    int a, b;
    float hw;       // half width of asphalt
    bool rural;     // country road (no sidewalks, higher speed)
    float length;
    V3 dir;         // unit vector a -> b
};

struct Interior {
    enum Kind { STORE, BANK } kind = STORE;
    std::string name;
    AABB bounds;          // footprint (xz) used for "inside" tests
    V3 registerPos;       // store: register, bank: vault trigger
    V3 clerkPos[2];
    int clerkCount = 1;
    float clerkYaw = 0;
    V3 doorPos;
    float cooldown = 0;   // seconds until it can be robbed again
};

struct ParkedSpawn { V3 pos; float yaw; };
struct MapRoad { V3 a, b; float hw; Col col; };
struct MapRect { float x, z, w, d; Col c; };

class World {
public:
    void build();
    void query(float x0, float z0, float x1, float z1, std::vector<int>& out) const;
    V3 pushCircle(V3 p, float r, float y0, float y1, V3* hitNormal = nullptr) const;
    float groundHeight(float x, float z, float feetY) const;
    bool raycast(V3 o, V3 d, float maxT, float& tHit) const;
    bool lineOfSight(V3 a, V3 b) const;
    int nearestNode(V3 p) const;
    int edgeBetween(int a, int b) const;
    int insideInterior(V3 p) const;  // index or -1

    MeshBuilder statics;
    std::vector<AABB> solids;
    std::vector<RoadNode> nodes;
    std::vector<RoadEdge> edges;
    std::vector<std::vector<int>> nextHop;  // nextHop[from][to] = neighbour node to move to
    std::vector<Interior> interiors;
    std::vector<ParkedSpawn> parked;
    std::vector<MapRoad> mapRoads;
    std::vector<MapRect> mapRects;
    std::vector<V3> lampPosts;
    static constexpr float CITY_HALF = 167.0f;

private:
    static constexpr float CELL = 24.0f, OX = -480.0f, OZ = -360.0f;
    static constexpr int NXC = 58, NZC = 56;
    std::vector<std::vector<int>> cells;
    mutable std::vector<unsigned> stamp;
    mutable unsigned stampCur = 0;
    Rng rng{20240611};

    void addBoxMin(V3 mn, V3 mx, Col c, bool solid);
    void addSolid(V3 mn, V3 mx);
    int addNode(float x, float z, bool signal = false);
    void addEdge(int a, int b, float hw, bool rural, bool dirt);
    void buildRoads();
    void buildCity();
    void buildVillages();
    void buildTrees();
    void buildGraph();
    void buildGrid();
    void roadStrip(V3 a, V3 b, float hw, Col c, bool dashed, bool edgeLines);
    void lampPost(float x, float z, float yawToRoad);
    void building(float cx, float cz, float w, float d, float h, Col c, bool windows, int roofKind);
    void midRise(float cx, float cz, float w, float d, float h, Col c);
    void house(float cx, float cz, float yaw, Col wall, Col roof);
    void store(float cx, float cz, float yaw, const std::string& name);
    void bank(float cx, float cz, float yaw);
    void tree(float x, float z, float scale);
    void parkedCar(float x, float z, float yaw);
    bool nearRoad(float x, float z, float margin) const;
};
