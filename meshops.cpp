#include "meshops.h"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <set>
#include <unordered_map>
#include "kiss-skeleton.h"
#include "zgfx.h"

struct graph_node
{
    std::set<graph_node *> neighbors;

    size_t v; // index
    glm::vec4 pos;
    std::vector<float> weights; // joint weights
};

struct graph
{
    std::unordered_map<int, graph_node *> nodes;
};

graph_node *
make_graph_node(size_t v, const glm::vec4 &pos)
{
    graph_node *ret = new graph_node;

    ret->pos = pos;
    ret->v = v;

    return ret;
}

void
add_neighbor(graph_node *node, graph_node *neighbor)
{
    node->neighbors.insert(neighbor);
}

graph *
make_graph(const rawmesh *mesh)
{
    graph *g = new graph;

    // Loop over all input faces
    for (size_t fi = 0; fi < mesh->nfaces; fi++)
    {
        const fullface &face = mesh->ffaces[fi];
        glm::vec4 verts[3];

        // Create any verts necessary
        for (size_t vi = 0; vi < 3; vi++)
        {
            size_t v = face.fverts[vi].v;
            glm::vec4 vert = glm::make_vec4(mesh->verts[v].pos);
            verts[vi] = vert;

            if (g->nodes.count(v) == 0)
                g->nodes[v] = make_graph_node(v, verts[vi]);

            // add as a neighbor to previous nodes
            for (size_t k = 0; k < vi; k++)
                add_neighbor(g->nodes[face.fverts[k].v], g->nodes[v]);
        }
    }

    return g;
}

int
pointVisibleToPoint(const glm::vec3 &refpt, const glm::vec3 &pt,
        const rawmesh *mesh)
{
    // A point is visible to another point if the line between them does not 
    // intersect ANY triangles
    const glm::vec3 endpt = refpt + (pt - refpt) * 0.99f;

    // Loop over all triangles, see if there is an intersection
    vert* verts = mesh->verts;
    glm::vec3 tri[3];
    for (size_t i = 0; i < mesh->nfaces; i++)
    {
        const fullface &face = mesh->ffaces[i];
        tri[0] = glm::make_vec3(verts[face.fverts[0].v].pos);
        tri[1] = glm::make_vec3(verts[face.fverts[1].v].pos);
        tri[2] = glm::make_vec3(verts[face.fverts[2].v].pos);

        if (segIntersectsTriangle(refpt, endpt, tri) != glm::vec3(HUGE_VAL))
            return i;
    }

    // Doesn't intersect any triangles
    return -1;
}

// Returns intersection point or vec3(HUGE_VAL) for no intersection
glm::vec3
segIntersectsTriangle(const glm::vec3 &seg0, const glm::vec3 &seg1,
        const glm::vec3 triangle[3])
{
    const glm::vec3 NO_INTERSECTION = glm::vec3(HUGE_VAL);
    // From http://softsurfer.com/Archive/algorithm_0105/algorithm_0105.htm#intersect_RayTriangle()
    const float small_val = 0.0001f;

    const glm::vec3 raystart = seg0;
    const glm::vec3 raydir = seg1 - seg0;
    glm::vec3 triu = triangle[1] - triangle[0];
    glm::vec3 triv = triangle[2] - triangle[0];
    glm::vec3 trinorm = glm::cross(triu, triv);
    // Degenerate triangle
    if (trinorm == glm::vec3(0))
        return NO_INTERSECTION;

    // check for parallel or coincident
    if (fabs(glm::dot(trinorm, raydir)) < small_val)
        return NO_INTERSECTION;

    // in P . N + d = 0 plane equation
    float d = -glm::dot(trinorm, triangle[0]);
    // time of intersection from :
    // http://www.cs.princeton.edu/courses/archive/fall00/cs426/lectures/raycast/sld017.htm
    float t = -(glm::dot(seg0, trinorm) + d) / glm::dot(raydir, trinorm);

    // Check to make sure the intersected point is within our line segment
    // if not, no intersection
    if (t <= 0.f || t >= 1.f)
        return NO_INTERSECTION;

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
    // This including of small value here handles the cases where the ray
    // intersects a vertex of the triangle
    if (bu < 0 || bv < 0 || bu + bv > 1)
        return NO_INTERSECTION;

    // Collision
    return I;
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

void rogueRemoval(graph *g, const Skeleton *skeleton)
{
    printf("Beginning rogueRemoval\n");
    const std::vector<Joint *> &joints = skeleton->getJoints();

    // looping over all the joints, finding a valid point, and then expanding
    // out from there marking all connected verts as valid.  Then find all 
    // points not marked as valid, and remove their weights
    for (size_t ji = 0; ji < joints.size(); ji++)
    {
        // Find a valid point

    }

    printf("rogueRemoval completed.\n");
}

void autoSkinMeshBest(rawmesh *rmesh, const Skeleton *skeleton)
{
    // Requires a mesh with skinning data
    assert(rmesh && rmesh->joints && rmesh->weights);

    // Make a graph representation of the mesh
    printf("Creating graph structure.\n");
    graph *g = make_graph(rmesh);
    printf("nodes in graph: %zu\n", g->nodes.size());

    // Get all the joint world positions for easy access
    std::vector<glm::vec3> jointPos;
    const std::vector<Joint*> skeljoints = skeleton->getJoints();
    jointPos = std::vector<glm::vec3>(skeljoints.size());
    for (size_t i = 0; i < skeljoints.size(); i++)
        jointPos[i] = applyMatrix(skeljoints[i]->worldTransform, glm::vec3(0.f));


    // For each vertex, find the closest joint and bind only to that vertex
    // to distance
    vert  *verts   = rmesh->verts;
    for (size_t i = 0; i < rmesh->nverts; i++)
    {
        // cache vert position
        const glm::vec3 vert = glm::make_vec3(verts[i].pos);
        // Reset joint scores
        std::vector<float> jointScores(jointPos.size(), -HUGE_VAL);

        // fill in the weights
        for (size_t j = 0; j < jointPos.size(); j++)
        {
            int targetJoint = -1;
            // Figure out the end points of the bone
            if (skeljoints[j]->parent == Skeleton::ROOT_PARENT)
                continue;
            const Joint *parent = skeljoints[skeljoints[j]->parent];
            targetJoint = parent->index;
            // points defining the bone line
            glm::vec3 p0 = jointPos[targetJoint];
            glm::vec3 p1 = jointPos[j];
            glm::vec3 pdir = glm::normalize(p1 - p0); // a^i (direction of bone)

            // Calculate the integral
            float score = 0.f;
            const float dlambda = 0.1f;
            for (float lambda = 0; lambda <= 1.f; lambda += dlambda)
            {
                // current point along bone (b^i_\lambda)
                glm::vec3 cur = lambda * (p1 - p0) + p0;
                // Direction from point to vert (d^i_\lambda)
                glm::vec3 dij = vert - cur;
                float mag2dir = glm::dot(dij, dij);
                glm::vec3 dir = glm::normalize(dij);
                // visibility test, term goes to zero if not visible
                if (pointVisibleToPoint(jointPos[j], vert, rmesh) != -1) continue;

                float term = 1.f;
                // Model the proportion of illuminated line (sine of the angle)
                // T^i_j(\lambda) = || d^i_j x a^i ||
                term *= glm::length(glm::cross(dir, pdir));
                // TODO calculate R^i_j(lambda)
                // R^i_j(\lambda) = max(d^i_\lambda . n_j, 0) / ||d_\lambda||^2
                term *= 1.f / mag2dir; // just fall of with distance for now

                score += term * dlambda;
            }

            // l^i term in front of integral, (bone length)
            score *= glm::length(p1 - p0);

            if (score > 0.f)
                jointScores[targetJoint] = score;

        }

        // Assign to vertex for smoothing
        g->nodes[i]->weights = jointScores;

        // Done assigning initial values to this vert...
        printf("i: %zu / %zu\n", i + 1, rmesh->nverts);
    }
    printf("Finished assigning initial weights.\n");

    // TODO smooth weights
    // First remove "rogues" i.e. patches of weightings that don't make any
    // sense given the graph structure
    rogueRemoval(g, skeleton);


    // Finally, assign the weights to the mesh, only take top 4, and apply some
    // thresholding
    int   *joints  = rmesh->joints;
    float *weights = rmesh->weights;
    for (size_t vi = 0; vi < rmesh->nverts; vi++)
    {
        assert(g->nodes.count(vi));
        std::vector<float> jointScores = g->nodes[vi]->weights;
        for (size_t wi = 0; wi < 4; wi++)
        {
            float bestscore = -HUGE_VAL;
            int bestjoint = -1;
            // For now just find highest score
            for (size_t j = 0; j < jointScores.size(); j++)
            {
                if (jointScores[j] > bestscore)
                {
                    bestjoint = j;
                    bestscore = jointScores[j];
                }
            }

            // No more bones?
            if (bestjoint == -1) // TODO possible threshold on weight here
            {
                // zero out the rest
                for (; wi < 4; wi++)
                {
                    joints[4*vi + wi]  = -1;
                    weights[4*vi + wi] = 0.f;
                }
                // no more work for this vert
                break;
            }
            // Clear best score
            jointScores[bestjoint] = -HUGE_VAL;

            joints[4*vi + wi]  = bestjoint;
            weights[4*vi + wi] = bestscore;
        }

        // normalize scores
        float sum = 0.f;
        for (size_t wi = 0; wi < 4; wi++)
            sum += weights[4*vi + wi];
        for (size_t wi = 0; wi < 4; wi++)
            weights[4*vi + wi] /= sum;
        
        // Some prints..
        for (size_t wi = 0; wi < 4; wi++)
        {
            const int joint = joints[4*vi + wi];
            const float weight = weights[4*vi + wi];
            if (joint == -1)
                printf("[ ] ");
            else
                printf("[%d %s %f] ", joint, skeljoints[joint]->name.c_str(), weight);
        }
        printf("\n");
    }
}

