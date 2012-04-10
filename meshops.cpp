#include "zgfx.h"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
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

graph *
build_graph(const rawmesh *mesh)
{
    return NULL;
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
        float bestdist = HUGE_VAL;
        size_t best = 0;
        for (size_t j = 0; j < jointPos.size(); i++)
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

