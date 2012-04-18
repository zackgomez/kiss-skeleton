#include "meshops.h"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <queue>
#include "skeleton.h"
#include "zgfx.h"

graph_node *
make_graph_node(size_t v, const glm::vec4 &pos)
{
    graph_node *ret = new graph_node;

    ret->pos = pos;
    ret->v = v;
    ret->norm = glm::vec3(0.f);
    ret->flag = 0;

    return ret;
}

/* Creates an undirected edge (i.e. both ways) between the two passed nodes
 */
void
add_neighbor(graph_node *node, graph_node *neighbor)
{
    node->neighbors.insert(neighbor);
    neighbor->neighbors.insert(node);
}

graph *
make_graph(const rawmesh *mesh)
{
    graph *g = new graph;

    // Loop over all input faces
    for (size_t fi = 0; fi < mesh->nfaces; fi++)
    {
        const fullface &face = mesh->ffaces[fi];

        // Create any verts necessary
        for (size_t vi = 0; vi < 3; vi++)
        {
            const size_t v = face.fverts[vi].v;

            if (g->nodes.count(v) == 0)
                g->nodes[v] = make_graph_node(v, glm::make_vec4(mesh->verts[v].pos));

            // connect up nodes
            for (size_t k = 0; k < vi; k++)
                add_neighbor(g->nodes[face.fverts[k].v], g->nodes[v]);

            // accumulate normal
            g->nodes[v]->norm += mesh->norms[face.fverts[vi].vn];
        }
    }

    // normalize all the norms
    for (auto git = g->nodes.begin(); git != g->nodes.end(); git++)
        git->second->norm = glm::normalize(git->second->norm);

    // Done
    return g;
}

void
free_graph(graph *g)
{
    if (!g) return;

    for (auto git = g->nodes.begin(); git != g->nodes.end(); git++)
        delete git->second;
    delete g;
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

int
findValidPoint(const graph *g, const Skeleton *skeleton, size_t joint)
{
    int v = -1;
    float best = -HUGE_VAL;
    // Just do the simple thing and find the vert with the highest score...
    for (auto it = g->nodes.begin(); it != g->nodes.end(); it++)
    {
        // TODO
        // Instead of weight use distance to bone line (pointLineDist)
        // be careful of multiple bones emanating from a single joint
        if (it->second->weights[joint] > best)
        {
            best = it->second->weights[joint];
            v = it->first;
        }
    }

    return v;
}

void
clearFlag(graph *g)
{
    for (auto it = g->nodes.begin(); it != g->nodes.end(); it++)
        it->second->flag = 0;
}

void
rogueRemoval(graph *g, const Skeleton *skeleton)
{
    printf("Beginning rogueRemoval\n");

    // Go over each bone
    for (size_t ji = 0; ji < skeleton->getJoints().size(); ji++)
    {
        size_t nodes_flagged = 0;
        // Find a "model point"
        int startV = findValidPoint(g, skeleton, ji);
        if (startV == -1)
            continue;

        // Traverse the graph pushing the valid out to nodes connected to the
        // model point that have weighting for this bone
        clearFlag(g);
        std::queue<graph_node *> q;
        q.push(g->nodes[startV]);
        while (!q.empty())
        {
            graph_node *n = q.front();
            q.pop();

            // early out
            if (n->flag) continue;

            n->flag = 1;
            ++nodes_flagged;
            // add neighbors
            for (auto nit = n->neighbors.begin(); nit != n->neighbors.end(); nit++)
            {
                graph_node *nn = *nit;
                if (!nn->flag && n->weights[ji] != -HUGE_VAL)
                    q.push(nn);
            }
        }
        printf("(%zu) %zu marked nodes in rogue removal.\n", ji, nodes_flagged);

        // Now go through all nodes, if they are weighted to the current joint,
        // make sure they're valid.
        size_t nodes_affected = 0;
        for (auto git = g->nodes.begin(); git != g->nodes.end(); git++)
        {
            graph_node *n = git->second;

            // if joint is weighted but not valid, remove weighting
            if (n->weights[ji] != -HUGE_VAL && !n->flag)
            {
                ++nodes_affected;
                n->weights[ji] = -HUGE_VAL;
            }
        }
        printf("(%zu) %zu nodes affected in rogue removal.\n", ji, nodes_affected);
    }

    printf("rogueRemoval completed.\n");
}

void
gapFill(graph *g)
{
    size_t nodes_attached = 1;
    while (nodes_attached > 0)
    {
        nodes_attached = 0;
        // find completely unassigned verts
        for (auto git = g->nodes.begin(); git != g->nodes.end(); git++)
        {
            graph_node *n = git->second;
            // see if the vert is completely unassigned
            size_t i;
            for (i = 0; i < n->weights.size(); i++)
                if (n->weights[i] != -HUGE_VAL)
                    break;
            if (i != n->weights.size())
                continue;

            ++nodes_attached;

            // average surrounding verts
            for (auto nit = n->neighbors.begin(); nit != n->neighbors.end(); nit++)
            {
                const graph_node *nn = *nit;
                for (i = 0; i < n->weights.size(); i++)
                {
                    if (nn->weights[i] != -HUGE_VAL)
                    {
                        if (n->weights[i] == -HUGE_VAL)
                            n->weights[i] = nn->weights[i];
                        else
                            n->weights[i] += nn->weights[i];
                    }
                }
            }
            for (i = 0; i < n->weights.size(); i++)
                n->weights[i] /= n->weights.size();
        }
        printf("Connected %zu unconnected verts.\n", nodes_attached);
    }
}

void
smoothWeights(graph *g)
{
    // just get the size of the weights
    size_t nweights = g->nodes[0]->weights.size();

    // for every node, set newweights to the simple average of all the
    // surrounding vertex weights
    for (auto git = g->nodes.begin(); git != g->nodes.end(); git++)
    {
        graph_node *cur = git->second;

        // start with current weights
        cur->newweights = cur->weights;

        // loop over neighbors
        int count = 1; // be sure to include ourself
        for (auto nit = cur->neighbors.begin(); nit != cur->neighbors.end(); nit++)
        {
            graph_node *nn = *nit;
            assert(nn->weights.size() == nweights);
            ++count;

            // now sum
            for (size_t i = 0; i < nweights; i++)
            {
                if (cur->newweights[i] == -HUGE_VAL && nn->weights[i] != -HUGE_VAL)
                    cur->newweights[i] = nn->weights[i];
                else if (nn->weights[i] != -HUGE_VAL)
                    cur->newweights[i] += nn->weights[i];
            }
        }

        // Average and assign
        for (size_t i = 0; i < nweights; i++)
            cur->weights[i] = cur->newweights[i] / count;
    }
}

void assignWeights(rawmesh *rmesh, graph *g)
{
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
            // Find the highest score
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
        /*
           printf("(vert %zu) ", vi);
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
        */
    }
}

graph *
autoSkinMeshBest(rawmesh *rmesh, const Skeleton *skeleton)
{
    // Requires a mesh with skinning data
    assert(rmesh && rmesh->joints && rmesh->weights);

    // Make a graph representation of the mesh
    printf("Creating graph structure.\n");
    graph *g = make_graph(rmesh);
    printf("nodes in graph: %zu\n", g->nodes.size());

    const std::vector<Joint*> joints = skeleton->getJoints();
    const std::vector<Bone*>  bones  = skeleton->getBones();

    printf("Skinning against %zu joints in %zu bones.\n", joints.size(),
            bones.size());


    // For each vertex, find the closest joint and bind only to that vertex
    // to distance
    for (size_t i = 0; i < g->nodes.size(); i++)
    {
        // cache vert position
        graph_node *node = g->nodes[i];
        const glm::vec3 vert = glm::vec3(node->pos);
        const glm::vec3 norm = node->norm;
        // Reset joint scores
        std::vector<float> jointScores(joints.size(), -HUGE_VAL);

        // fill in the weights
        for (size_t j = 0; j < bones.size(); j++)
        {
            const Bone *bone = bones[j];
            const Joint *joint = bones[j]->joint;
            // points defining the bone line
            glm::vec3 p0 = applyMatrix(joint->worldTransform, glm::vec3(0.f));
            glm::vec3 p1 = applyMatrix(joint->worldTransform, bone->tipPos);
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
                if (pointVisibleToPoint(cur, vert, rmesh) != -1) continue;

                float term = 1.f;
                // Model the proportion of illuminated line (sine of the angle)
                // T^i_j(\lambda) = || d^i_j x a^i ||
                term *= glm::length(glm::cross(dir, pdir));
                // Proportion of recieved light (lambertian), includes angle of
                // incidence and an r^2 falloff
                // R^i_j(\lambda) = max(d^i_\lambda . n_j, 0) / ||d_\lambda||^2
                term *= glm::max(glm::dot(dir, norm), 0.f) / mag2dir;

                // accumulate integral
                score += term * dlambda;
            }

            // l^i term in front of integral, (bone length)
            score *= glm::length(p1 - p0);

            // assign if non zero score
            if (score > 0.f)
                jointScores[joint->index] = score;
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

    // Fixes completely unassigned verts
    //gapFill(g);
    //smoothWeights(g);

    assignWeights(rmesh, g);

    return g;
}

