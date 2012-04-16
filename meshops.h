#pragma once
#include <glm/glm.hpp>
#include <unordered_map>
#include <set>
#include <vector>

struct rawmesh;
class Skeleton;

// Data structures
struct graph_node
{
    std::set<graph_node *> neighbors;

    size_t v; // index
    glm::vec4 pos;
    glm::vec3 norm; // this normal is calculated as the average of all normals used w/ this vert
    std::vector<float> weights; // joint weights
    std::vector<float> newweights; // use for smoothing

    // used for some computations
    int flag;
};
struct graph
{
    std::unordered_map<int, graph_node *> nodes;
};

// Graph functions
graph * make_graph(const rawmesh *mesh);

graph_node * make_graph_node(size_t v, const glm::vec4 &pos);

void free_graph(graph *g);


graph * autoSkinMeshBest(rawmesh *rmesh, const Skeleton *skeleton);

// returns the index of the intersecting triangle, or -1
int pointVisibleToPoint(const glm::vec3 &refpt, const glm::vec3 &pt,
        const rawmesh *mesh);
glm::vec3 segIntersectsTriangle(const glm::vec3 &seg0, const glm::vec3 &seg1,
        const glm::vec3 triangle[3]);

