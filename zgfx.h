#ifndef _INCLUDE_ZGFX_H_
#define _INCLUDE_ZGFX_H_

#include <GL/glew.h>
#include <glm/glm.hpp>

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
rawmesh * loadRawMesh(const char *filename, bool skinned);
void freeRawMesh(rawmesh *rmesh);

void writeRawMesh(rawmesh *rmesh, const char *filename);

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


#endif
