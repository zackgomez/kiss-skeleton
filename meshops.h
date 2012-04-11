#pragma once
#include <glm/glm.hpp>

struct rawmesh;
class Skeleton;

void autoSkinMeshNearest(rawmesh *rmesh, const Skeleton *skeleton);
void autoSkinMeshBest(rawmesh *rmesh, const Skeleton *skeleton);

// returns the index of the intersecting triangle, or -1
int pointVisibleToPoint(const glm::vec3 &refpt, const glm::vec3 &pt,
        const rawmesh *mesh);
glm::vec3 segIntersectsTriangle(const glm::vec3 &seg0, const glm::vec3 &seg1,
        const glm::vec3 triangle[3]);

