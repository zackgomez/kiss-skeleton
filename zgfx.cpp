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
    unsigned nverts = 0, nfaces = 0, nnorms = 0, ncoords = 0;
    // lines should never be longer than 1024 in a .obj...
    char buf[1024];

    while (fgets(buf, sizeof(buf), f))
    {
        char *cmd = strtok(buf, " ");
        if (strcmp(cmd, "v") == 0)
            nverts++;
        if (strcmp(cmd, "f") == 0)
            nfaces++;
        if (strcmp(cmd, "vn") == 0)
            nnorms++;
        if (strcmp(cmd, "vt") == 0)
            ncoords++;
    }

    printf("verts: %d, faces: %d, norms: %d, coords: %d\n",
            nverts, nfaces, nnorms, ncoords);

    // Allocate space
    vert *verts = (vert*) malloc(sizeof(vert) * nverts);
    int  *joints = (int*) malloc(sizeof(int)  * nverts);
    face *faces = (face*) malloc(sizeof(face) * nfaces);
    glm::vec2 *coords = (glm::vec2*) malloc(sizeof(glm::vec2) * ncoords);
    glm::vec3 *norms = (glm::vec3*) malloc(sizeof(glm::vec3) * ncoords);

    // XXX replace this will some real checks
    assert(faces && verts && joints && norms && coords);

    // reset to beginning of file
    fseek(f,  0, SEEK_SET);

    int verti = 0, facei = 0, normi = 0, coordi = 0;
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

            // GEOSMASH EXTENSION for skinning
            const char *jointstr = strtok(NULL, " ");
            //printf("jointstr: %s\n", jointstr);
            if (jointstr)
                joints[verti] = atoi(jointstr);
            else
                joints[verti] = 0;

            ++verti;
        }
        else if (strcmp(cmd, "f") == 0)
        {
            // This assumes the faces are triangles
            faces[facei].verts[0] = parseFaceVert(strtok(NULL, " "));
            faces[facei].verts[1] = parseFaceVert(strtok(NULL, " "));
            faces[facei].verts[2] = parseFaceVert(strtok(NULL, " "));
            //printf("face: %d %d %d\n", faces[facei].verts[0],
                    //faces[facei].verts[1], faces[facei].verts[2]);
            ++facei;
        }
        else if (strcmp(cmd, "vn") == 0)
        {
            //norms[normi][0] = atof(strtok(NULL, " "));
            //norms[normi][1] = atof(strtok(NULL, " "));
            //norms[normi][2] = atof(strtok(NULL, " "));
            ++normi;
        }
        else if (strcmp(cmd, "vt") == 0)
        {
            //coords[coordi][0] = atof(strtok(NULL, " "));
            //coords[coordi][1] = atof(strtok(NULL, " "));
            ++coordi;
        }
    }

    assert(verti == nverts);
    assert(facei == nfaces);
    assert(normi == nnorms);
    assert(coordi == ncoords);
    // Done with file
    fclose(f);

    rawmesh *rmesh = (rawmesh*) malloc(sizeof(rawmesh));
    // XXX again better error checking
    assert(rmesh);
    rmesh->verts  = verts;
    rmesh->joints = joints;
    rmesh->faces  = faces;
    rmesh->norms  = norms;
    rmesh->coords = coords;
    rmesh->nverts = nverts;
    rmesh->nfaces = nfaces;
    rmesh->nnorms = nnorms;
    rmesh->ncoords= ncoords;

    return rmesh;
}

int parseFaceVert(const char *vdef)
{
    // TODO update this function to support texcoords and normals
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
    free(rmesh->norms);
    free(rmesh->coords);
    free(rmesh);
}

void writeRawMesh(rawmesh *rmesh, const char *filename)
{
    FILE *f = fopen(filename, "w");
    if (!f)
    {
        fprintf(stderr, "Unable to open %s for writing.\n", filename);
        return;
    }

    // Write a comment saying kiss-skeleton did it
    fprintf(f, "# kiss-skeleton v0.00 OBJ File: '%s'\n", filename);

    // verts
    const vert* verts = rmesh->verts;
    const int* joints = rmesh->joints;
    for (size_t i = 0; i < rmesh->nverts; i++)
        fprintf(f, "v %f %f %f %d\n",
                verts[i].pos[0], verts[i].pos[1], verts[i].pos[2], joints[i]);
    // coords
    const glm::vec2 *coords = rmesh->coords;
    for (size_t i = 0; i < rmesh->ncoords; i++)
        fprintf(f, "vt %f %f\n",
                coords[i][0], coords[i][1]);
    // norms
    const glm::vec3 *norms = rmesh->norms;
    for (size_t i = 0; i < rmesh->ncoords; i++)
        fprintf(f, "vn %f %f %f\n",
                norms[i][0], norms[i][1], norms[i][2]);
    // faces
    const face *faces = rmesh->faces;
    for (size_t i = 0; i < rmesh->ncoords; i++)
        fprintf(f, "f %d// %d// %d//\n",
               faces[i].verts[0], faces[i].verts[1], faces[i].verts[2]);

    fclose(f);
}
