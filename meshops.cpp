#include "meshops.h"
#include "zgfx.h"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include "kiss-skeleton.h"

struct graph_node
{
    graph_node **neighbors;
    size_t num_neighbors;

    glm::vec4 pos;
};

struct graph
{
    size_t num_nodes;
    graph_node **all_nodes;
};

graph_node *
make_graph_node(const glm::vec4 &pos)
{
    graph_node *ret = (graph_node *) malloc(sizeof(graph_node));

    ret->neighbors = 0;
    ret->num_neighbors = 0;
    ret->pos = pos;

    return ret;
}

void
add_neighbor(graph_node *node, graph_node *neighbor)
{
    node->num_neighbors++;
    node->neighbors = (graph_node **) realloc(node->neighbors,
            sizeof(graph_node *) * node->num_neighbors);
    node->neighbors[node->num_neighbors - 1] = neighbor;
}

bool pointVisibleToPoint(const glm::vec3 &refpt, const glm::vec3 &pt,
        const rawmesh *mesh)
{
    // A point is visible to another point if the line between them does not 
    // intersect ANY triangles
    const glm::vec3 raystart = pt;
    const glm::vec3 raydir = refpt - pt;

    // Loop over all triangles, see if there is an intersection
    vert* verts = mesh->verts;
    glm::vec3 tri[3];
    for (size_t i = 0; i < mesh->nfaces; i++)
    {
        const fullface &face = mesh->ffaces[i];
        tri[0] = glm::make_vec3(verts[face.fverts[0].v].pos);
        tri[1] = glm::make_vec3(verts[face.fverts[1].v].pos);
        tri[2] = glm::make_vec3(verts[face.fverts[2].v].pos);

        if (rayIntersectsTriangle(raystart, raydir, tri))
            return false;
    }

    // Doesn't intersect any triangles
    return true;
}

// Returns 't' of intersection, or -HUGE_VAL for no intersection
bool rayIntersectsTriangle(const glm::vec3 &raystart, const glm::vec3 &raydir,
        const glm::vec3 triangle[3])
{
    // From http://softsurfer.com/Archive/algorithm_0105/algorithm_0105.htm#intersect_RayTriangle()
    const float small_val = 0.001f;

    glm::vec3 triu = triangle[1] - triangle[0];
    glm::vec3 triv = triangle[2] - triangle[0];
    glm::vec3 trinorm = glm::cross(triu, triv);
    if (trinorm == glm::vec3(0))
        return false;

    // The ray and plane are coincident or parallel
    // no intersection
    float a = glm::dot(trinorm, raydir);
    if (fabs(a) < small_val)
        return false;

    // Get intersect of ray and plane
    const glm::vec3 w0 = raystart - triangle[0];
    float b = -glm::dot(w0, trinorm);

    // time of intersection with plane
    float t = b / a;
    if (t <= 0.f || t > 1.f)
        return false;

    // intersection pt
    const glm::vec3 I = raystart + t * raydir;

    // Use barycentric coordinates to test for I in triangle
    // see http://www.blackpawn.com/texts/pointinpoly/default.html
    float uu, uv, uw, vv, vw;
    const glm::vec3 triw = I - triangle[0];
    uu = glm::dot(triu, triu);
    uv = glm::dot(triu, triv);
    uw = glm::dot(triu, triw);
    vv = glm::dot(triv, triv);
    vw = glm::dot(triv, triw);

    float invDenom = 1 / (uu * vv - uv * uv);
    float bu = (vv * uw - uv * vw) * invDenom;
    float bv = (uu * vw - uv * uw) * invDenom;

    // test for interior
    if (bu < 0 || bv < 0 || bu + bv > 1)
        return false;

    // Collision
    return true;
}


void autoSkinMeshNearest(rawmesh *rmesh, const Skeleton *skeleton)
{
    // Requires a mesh with skinning data
    assert(rmesh && rmesh->joints && rmesh->weights);

    // First get all the joint world positions for easy access
    std::vector<glm::vec3> jointPos;
    {
        const std::vector<Joint*> joints = skeleton->getJoints();
        jointPos = std::vector<glm::vec3>(joints.size());
        for (size_t i = 0; i < joints.size(); i++)
            jointPos[i] = applyMatrix(joints[i]->worldTransform, glm::vec3(0.f));
    }

    // For each vertex, find the closest joint and bind only to that vertex
    // to distance
    vert  *verts   = rmesh->verts;
    int   *joints  = rmesh->joints;
    float *weights = rmesh->weights;
    for (size_t i = 0; i < rmesh->nverts; i++)
    {
        printf("i: %zu\n", i);
        float bestdist = HUGE_VAL;
        size_t best = 0;
        for (size_t j = 0; j < jointPos.size(); j++)
        {
            const glm::vec3 diff = jointPos[j] - glm::make_vec3(verts[i].pos);
            float dist = glm::dot(diff, diff);
            if (dist < bestdist)
            {
                bestdist = dist;
                best = j;
            }
        }

        // Fill in best 1, 0 0, 0 0, 0 0
        joints[4*i + 0]  = best;
        joints[4*i + 1]  = 0;
        joints[4*i + 2]  = 0;
        joints[4*i + 3]  = 0;
        weights[4*i + 0] = 1.f;
        weights[4*i + 1] = 0.f;
        weights[4*i + 2] = 0.f;
        weights[4*i + 3] = 0.f;
    }
}

void autoSkinMeshTwoNearest(rawmesh *rmesh, const Skeleton *skeleton)
{
    // Requires a mesh with skinning data
    assert(rmesh && rmesh->joints && rmesh->weights);

    // First get all the joint world positions for easy access
    std::vector<glm::vec3> jointPos;
    {
        const std::vector<Joint*> joints = skeleton->getJoints();
        jointPos = std::vector<glm::vec3>(joints.size());
        for (size_t i = 0; i < joints.size(); i++)
            jointPos[i] = applyMatrix(joints[i]->worldTransform, glm::vec3(0.f));
    }

    // For each vertex, find the closest 2 joints and set weights according
    // to distance
    std::vector<float> dists;
    vert  *verts   = rmesh->verts;
    int   *joints  = rmesh->joints;
    float *weights = rmesh->weights;
    for (size_t i = 0; i < rmesh->nverts; i++)
    {
        dists.assign(jointPos.size(), HUGE_VAL);
        for (size_t j = 0; j < dists.size(); j++)
        {
            glm::vec3 v = glm::make_vec3(verts[i].pos);
            dists[j] = glm::length(jointPos[j] - v);
        }

        // only take best two
        for (size_t j = 0; j < 2; j++)
        {
            // Find best joint/dist pair
            float best = HUGE_VAL;
            int bestk = -1;
            for (size_t k = 0; k < dists.size(); k++)
            {
                if (dists[k] < best)
                {
                    best = dists[k];
                    bestk = k;
                }
            }

            joints[4*i + j]  = bestk;
            weights[4*i + j] = best;
            dists[bestk] = HUGE_VAL;
        }
        // Set the rest to 0
        joints[4*i + 2] = 0;
        joints[4*i + 3] = 0;
        weights[4*i + 2] = 0.f;
        weights[4*i + 3] = 0.f;

        // normalize weights
        float sum = 0.f;
        for (size_t j = 0; j < 4; j++)
            sum += weights[4*i + j];
        for (size_t j = 0; j < 4; j++)
            if (weights[4*i + j] != 0.f)
                weights[4*i + j] = 1 - (weights[4*i + j] / sum);
    }
}

void autoSkinMeshEnvelope(rawmesh *rmesh, const Skeleton *skeleton)
{
    // Requires a mesh with skinning data
    assert(rmesh && rmesh->joints && rmesh->weights);

    // First get all the joint world positions for easy access
    std::vector<glm::vec3> jointPos;
    {
        const std::vector<Joint*> joints = skeleton->getJoints();
        jointPos = std::vector<glm::vec3>(joints.size());
        for (size_t i = 0; i < joints.size(); i++)
            jointPos[i] = applyMatrix(joints[i]->worldTransform, glm::vec3(0.f));
    }

    std::vector<float> dists;
    vert  *verts   = rmesh->verts;
    int   *joints  = rmesh->joints;
    float *weights = rmesh->weights;
    for (size_t i = 0; i < rmesh->nverts; i++)
    {
        dists.assign(jointPos.size(), HUGE_VAL);
        for (size_t j = 0; j < dists.size(); j++)
        {
            glm::vec3 v = glm::make_vec3(verts[i].pos);
            dists[j] = glm::length(jointPos[j] - v);
        }

        // only take best two
        for (size_t j = 0; j < 2; j++)
        {
            // Find best joint/dist pair
            float best = HUGE_VAL;
            int bestk = -1;
            for (size_t k = 0; k < dists.size(); k++)
            {
                if (dists[k] < best)
                {
                    best = dists[k];
                    bestk = k;
                }
            }

            joints[4*i + j]  = bestk;
            weights[4*i + j] = best;
            dists[bestk] = HUGE_VAL;
        }
        // Set the rest to 0
        joints[4*i + 2] = 0;
        joints[4*i + 3] = 0;
        weights[4*i + 2] = 0.f;
        weights[4*i + 3] = 0.f;

        // normalize weights
        float sum = 0.f;
        for (size_t j = 0; j < 4; j++)
            sum += weights[4*i + j];
        for (size_t j = 0; j < 4; j++)
            if (weights[4*i + j] != 0.f)
                weights[4*i + j] = 1 - (weights[4*i + j] / sum);
    }
}

