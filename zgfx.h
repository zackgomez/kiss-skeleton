#ifndef _INCLUDE_ZGFX_H_
#define _INCLUDE_ZGFX_H_

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <string>

struct vert
{
    float pos[4];
};

struct face
{
    int verts[3];
};

struct rawmesh
{
    vert *verts;
    int  *joints; // the joint vertices are bound to
    size_t nverts;

    face *faces;
    size_t nfaces;
};

// Mesh functions
// assumes .obj file has triangulated faces
rawmesh * loadRawMesh(const char *filename);
void freeRawMesh(rawmesh *rmesh);

#endif
