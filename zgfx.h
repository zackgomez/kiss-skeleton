#ifndef _INCLUDE_ZGFX_H_
#define _INCLUDE_ZGFX_H_

#include <GL/glew.h>
#include <glm/glm.hpp>

struct vert
{
    float pos[4];
};

struct face
{
    int verts[3];
};

struct facevert { int v, vt, vn; };
struct fullface
{
    facevert fverts[3];
};

struct rawmesh
{
    vert *verts;
    int  *joints; // the joint vertices are bound to
    size_t nverts;

    // just for render convenience, full face is more useful in general
    face *faces;
    fullface *ffaces;
    size_t nfaces;

    glm::vec2 *coords;
    size_t ncoords;

    glm::vec3 *norms;
    size_t nnorms;
};

// Mesh functions
// assumes .obj file has triangulated faces
rawmesh * loadRawMesh(const char *filename);
void freeRawMesh(rawmesh *rmesh);

void writeRawMesh(rawmesh *rmesh, const char *filename);

#endif
