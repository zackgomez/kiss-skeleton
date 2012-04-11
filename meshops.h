#pragma once
#include <glm/glm.hpp>

struct rawmesh;
class Skeleton;

void autoSkinMeshNearest(rawmesh *rmesh, const Skeleton *skeleton);

// returns true if there is no mesh inbetween points
bool pointVisibleToPoint(const glm::vec3 &refpt, const glm::vec3 &pt,
        const rawmesh *mesh);
// Returns 't' of intersection, or -HUGE_VAL for no intersection
float rayIntersectsTriangle(const glm::vec3 &raystart, const glm::vec3 &raydir,
        const glm::vec3 triangle[3]);

