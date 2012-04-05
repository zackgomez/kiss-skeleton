#include "zgfx.h"
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cstring>

static int parseFaceVert(const char *vdef);

rawmesh* loadRawMesh(const char *filename)
{
    FILE *f = fopen(filename, "r");
    if (!f)
    {
        fprintf(stderr, "Unable to open %s for reading\n", filename);
        return NULL;
    }

    // First count the number of vertices and faces
    int nverts = 0, nfaces = 0;
    // lines should never be longer than 1024 in a .obj...
    char buf[1024];

    while (fgets(buf, sizeof(buf), f))
    {
        char *cmd = strtok(buf, " ");
        if (strcmp(cmd, "v") == 0)
            nverts++;
        if (strcmp(cmd, "f") == 0)
            nfaces++;
    }

    // Allocate space
    vert *verts = (vert*) malloc(sizeof(vert) * nverts);
    int  *joints = (int*) malloc(sizeof(int)  * nverts);
    face *faces = (face*) malloc(sizeof(face) * nfaces);
    // XXX replace this will some real checks
    assert(faces && verts);

    // reset to beginning of file
    fseek(f,  0, SEEK_SET);

    int verti = 0, facei = 0;
    // Read each line and act on it as necessary
    while (fgets(buf, sizeof(buf), f))
    {
        char *cmd = strtok(buf, " ");

        if (strcmp(cmd, "v") == 0)
        {
            // read the 3 floats, and pad with a fourth 1
            verts[verti].pos[0] = atof(strtok(NULL, " "));
            verts[verti].pos[1] = atof(strtok(NULL, " "));
            verts[verti].pos[2] = atof(strtok(NULL, " "));
            verts[verti].pos[3] = 1.f; // homogenous coords
            ++verti;
        }
        else if (strcmp(cmd, "f") == 0)
        {
            // This assumes the faces are triangles
            faces[facei].verts[0] = parseFaceVert(strtok(NULL, " "));
            faces[facei].verts[1] = parseFaceVert(strtok(NULL, " "));
            faces[facei].verts[2] = parseFaceVert(strtok(NULL, " "));
            printf("face: %d %d %d\n", faces[facei].verts[0],
                    faces[facei].verts[1], faces[facei].verts[2]);
            ++facei;
        }
    }

    // XXX this just binds all the verts to the root joint
    for (size_t i = 0; i < nverts; i++)
        joints[i] = 0;

    assert(verti == nverts);
    assert(facei == nfaces);
    // Done with file
    fclose(f);

    rawmesh *rmesh = (rawmesh*) malloc(sizeof(rawmesh));
    // XXX again better error checking
    assert(rmesh);
    rmesh->verts  = verts;
    rmesh->joints = joints;
    rmesh->faces  = faces;
    rmesh->nverts = nverts;
    rmesh->nfaces = nfaces;

    return rmesh;
}

int parseFaceVert(const char *vdef)
{
    assert(vdef);
    char buf[1024];
    strncpy(buf, vdef, sizeof(buf));
    char *saveptr;

    int v = atoi(strtok_r(buf, "/", &saveptr));
    int vn = atoi(strtok_r(buf, "/", &saveptr));
    int vt = atoi(strtok_r(buf, "/", &saveptr));

    // 1 indexed to 0 indexed
    return v - 1;
}

void freeRawMesh(rawmesh *rmesh)
{
    if (!rmesh) return;

    free(rmesh->verts);
    free(rmesh->faces);
    free(rmesh);
}
