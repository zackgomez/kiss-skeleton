#ifndef _INCLUDE_ZGFX_H_
#define _INCLUDE_ZGFX_H_

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdio>
#include <iostream>

struct vert
{
    float pos[4];
};

struct facevert { int v, vt, vn; };
struct fullface
{
    facevert fverts[3];
};

struct vert_p4t2n3
{
    glm::vec4 pos;
    glm::vec3 norm;
    glm::vec2 coord;
};

struct vert_p4t2n3j8
{
    glm::vec4 pos;
    glm::vec3 norm;
    glm::vec2 coord;
    // TODO compress these down to 1 byte each
    int joints[4];     // 4 4byte joint indexes
    float weights[4]; // 4 4byte joint weights
};

struct rawmesh
{
    vert *verts;
    int   *joints;  // the joint vertices are bound to 4 per vertex
    float *weights; // the weight each vertex is bound 4 per vertex
    size_t nverts;

    // just for render convenience, full face is more useful in general
    fullface *ffaces;
    size_t nfaces;

    glm::vec2 *coords;
    size_t ncoords;

    glm::vec3 *norms;
    size_t nnorms;
};

// Mesh functions
// assumes .obj file has triangulated faces
// skinned=true means allocate space for skinning info
// writes out skinned=true if file contained skin data
rawmesh * loadRawMesh(const char *filename, bool &skinned);
rawmesh * readRawMesh(const char *contents, size_t len, bool &skinned);
void freeRawMesh(rawmesh *rmesh);

void writeRawMesh(rawmesh *rmesh, const char *filename);
void writeRawMesh(rawmesh *rmesh, FILE* f);

vert_p4t2n3* createVertArray(const rawmesh *mesh, size_t *nverts);
vert_p4t2n3j8* createSkinnedVertArray(const rawmesh *mesh, size_t *nverts);


// shader support
struct shader
{
    GLuint program;
    GLuint vertex_shader;
    GLuint fragment_shader;
    // perhaps add a geometry_shader here as well
};
GLuint  make_shader(GLenum type, const char *filename);
shader* make_program(const char *vertfile, const char *fragfile);
void    free_program(shader *s);

// utility functions
std::ostream& operator<< (std::ostream& os, const glm::vec2 &v);
std::ostream& operator<< (std::ostream& os, const glm::vec3 &v);
std::ostream& operator<< (std::ostream& os, const glm::vec4 &v);
std::ostream& operator<< (std::ostream& os, const glm::quat &q);
glm::quat axisAngleToQuat(const glm::vec4 &a);
glm::vec4 quatToAxisAngle(const glm::quat &q);
glm::vec3 applyMatrix(const glm::mat4 &mat, const glm::vec3 &vec, bool homo = true);

#endif

