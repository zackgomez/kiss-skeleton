#include "zgfx.h"
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <glm/gtc/type_ptr.hpp>

#ifdef _MSC_VER
#define strtok_r strtok_s
#endif

static facevert parseFaceVert(const char *vdef);
static void show_info_log( GLuint object, PFNGLGETSHADERIVPROC glGet__iv,
        PFNGLGETSHADERINFOLOGPROC glGet__InfoLog);
static void *file_contents(const char *filename, GLint *length);

rawmesh * loadRawMesh(const char *filename, bool &skinned)
{
    int len = 0;
    char *contents = (char *) file_contents(filename, &len);
    rawmesh *ret =  readRawMesh(contents, len, skinned);
    free(contents);
    return ret;
}

rawmesh * readRawMesh(const char *contents, size_t len, bool &skinned)
{
    // Read file contents into streamstream object
    std::stringstream cs(std::string(contents, len), std::stringstream::in);

    // First count the number of vertices and faces
    unsigned nverts = 0, nfaces = 0, nnorms = 0, ncoords = 0;
    // Are we reading an objfile with geosmash extensions?
    bool extended = false;
    // lines should never be longer than 1024 in a .obj...
    char buf[1024];

    while (cs.getline(buf, sizeof buf))
    {
        char *cmd = strtok(buf, " ");
        char *arg = strtok(NULL, " ");
        if (strcmp(cmd, "v") == 0)
            nverts++;
        if (strcmp(cmd, "f") == 0)
            nfaces++;
        if (strcmp(cmd, "vn") == 0)
            nnorms++;
        if (strcmp(cmd, "vt") == 0)
            ncoords++;
        if (strcmp(cmd, "ext") == 0 && strcmp(arg, "geosmash") == 0)
            extended = true;
    }

    printf("verts: %d, faces: %d, norms: %d, coords: %d, extended: %d\n",
            nverts, nfaces, nnorms, ncoords, extended);

    // Allocate space
    vert      *verts   = (vert*) malloc(sizeof(vert) * nverts);
    fullface  *ffaces  = (fullface*) malloc(sizeof(fullface) * nfaces);
    glm::vec2 *coords  = (glm::vec2*) malloc(sizeof(glm::vec2) * ncoords);
    glm::vec3 *norms   = (glm::vec3*) malloc(sizeof(glm::vec3) * nnorms);
    assert(verts && norms && coords && ffaces);
    // Now allocate the geosmash skinning data if requested
    int       *joints  = skinned ? (int*) malloc(sizeof(int) * nverts * 4) : NULL;
    float     *weights = skinned ? (float*) malloc(sizeof(float) * nverts * 4) : NULL;
    assert(!skinned || (joints && weights));
    // TODO replace asserts will some real memory checks

    // reset to beginning of file
    cs.clear();
    cs.seekg(0, std::ios::beg);

    size_t verti = 0, facei = 0, normi = 0, coordi = 0;
    // Read each line and act on it as necessary
    while (cs.getline(buf, sizeof buf))
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
            if (skinned)
            {
                // If we're reading the extended format, then this data is present
                // in the file, otherwise we should just set all joints to 0 and
                // the first joint weight to 1 and others to 0
                if (!extended)
                {
                    joints[4*verti + 0] = 0;
                    joints[4*verti + 1] = 0;
                    joints[4*verti + 2] = 0;
                    joints[4*verti + 3] = 0;
                    weights[4*verti + 0] = 1.f;
                    weights[4*verti + 1] = 0.f;
                    weights[4*verti + 2] = 0.f;
                    weights[4*verti + 3] = 0.f;
                }
                // Read the joint/weight pairs, assume there are 4 pairs
                else
                {
                    for (size_t i = 0; i < 4; i++)
                    {
                        const char *jointstr = strtok(NULL, " ");
                        assert(jointstr);
                        joints[4*verti + i] = atoi(jointstr);
                        const char *weightstr = strtok(NULL, " ");
                        assert(weightstr);
                        weights[4*verti + i] = atof(weightstr);
                    }
                }
            }
            ++verti;
        }
        else if (strcmp(cmd, "f") == 0)
        {
            // This assumes the faces are triangles
            ffaces[facei].fverts[0] = parseFaceVert(strtok(NULL, " "));
            ffaces[facei].fverts[1] = parseFaceVert(strtok(NULL, " "));
            ffaces[facei].fverts[2] = parseFaceVert(strtok(NULL, " "));
            ++facei;
        }
        else if (strcmp(cmd, "vn") == 0)
        {
            norms[normi][0] = atof(strtok(NULL, " "));
            norms[normi][1] = atof(strtok(NULL, " "));
            norms[normi][2] = atof(strtok(NULL, " "));
            ++normi;
        }
        else if (strcmp(cmd, "vt") == 0)
        {
            coords[coordi][0] = atof(strtok(NULL, " "));
            coords[coordi][1] = atof(strtok(NULL, " "));
            ++coordi;
        }
    }

    assert(verti == nverts);
    assert(facei == nfaces);
    assert(normi == nnorms);
    assert(coordi == ncoords);

    rawmesh *rmesh = (rawmesh*) malloc(sizeof(rawmesh));
    // XXX again better error checking
    assert(rmesh);
    rmesh->verts   = verts;
    rmesh->joints  = joints;
    rmesh->weights = weights;
    rmesh->ffaces  = ffaces;
    rmesh->norms   = norms;
    rmesh->coords  = coords;
    rmesh->nverts  = nverts;
    rmesh->nfaces  = nfaces;
    rmesh->nnorms  = nnorms;
    rmesh->ncoords = ncoords;

	// if the file contained skinning data, let them know
	skinned = extended;
    return rmesh;
}

facevert parseFaceVert(const char *vdef)
{
    facevert fv;
    assert(vdef);

    if (sscanf(vdef, "%d/%d/%d", &fv.v, &fv.vt, &fv.vn) == 3)
    {
        // 1 indexed to 0 indexed
        fv.v -= 1; fv.vn -= 1; fv.vt -= 1;
        return fv;
    }
    else if (sscanf(vdef, "%d//%d", &fv.v, &fv.vn) == 2)
    {
        fv.v -= 1; fv.vn -= 1; fv.vt = 0;
        return fv;
    }
    else
        assert(false && "unable to read unfull .obj");
}

void freeRawMesh(rawmesh *rmesh)
{
    if (!rmesh) return;

    free(rmesh->verts);
    free(rmesh->norms);
    free(rmesh->coords);
    free(rmesh->joints);
    free(rmesh->weights);
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

    writeRawMesh(rmesh, f);

    fclose(f);
}

void writeRawMesh(rawmesh *rmesh, FILE *f)
{
    assert(f);

    // Write a comment saying kiss-skeleton did it
    fprintf(f, "# kiss-skeleton v0.00 OBJ File\n");
    // if the mesh contains skinning data, write out extension command
    bool extended = rmesh->joints && rmesh->weights;
    if (extended)
        fprintf(f, "ext geosmash\n");

    // verts
    const vert    *verts = rmesh->verts;
    const int    *joints = rmesh->joints;
    const float *weights = rmesh->weights;
    for (size_t i = 0; i < rmesh->nverts; i++)
    {
        fprintf(f, "v %f %f %f",
                verts[i].pos[0], verts[i].pos[1], verts[i].pos[2]);
        // Print out skinning data v x y z j0 w0 j1 w1 ... j3 w3
        for (size_t j = 0; extended && j < 4; j++)
            fprintf(f, " %d %f", joints[i*4 + j], weights[i*4 + j]);
        fprintf(f, "\n");
    }
    // coords
    const glm::vec2 *coords = rmesh->coords;
    for (size_t i = 0; i < rmesh->ncoords; i++)
        fprintf(f, "vt %f %f\n",
                coords[i][0], coords[i][1]);
    // norms
    const glm::vec3 *norms = rmesh->norms;
    for (size_t i = 0; i < rmesh->nnorms; i++)
        fprintf(f, "vn %f %f %f\n",
                norms[i][0], norms[i][1], norms[i][2]);
    // faces
    // be sure to convert from 0 based to 1 based indexing
    const fullface *ffaces = rmesh->ffaces;
    for (size_t i = 0; i < rmesh->nfaces; i++)
        fprintf(f, "f %d/%d/%d %d/%d/%d %d/%d/%d\n",
               ffaces[i].fverts[0].v+1, ffaces[i].fverts[0].vn+1, ffaces[i].fverts[0].vt+1,
               ffaces[i].fverts[1].v+1, ffaces[i].fverts[1].vn+1, ffaces[i].fverts[1].vt+1,
               ffaces[i].fverts[2].v+1, ffaces[i].fverts[2].vn+1, ffaces[i].fverts[2].vt+1);
}

vert_p4t2n3 * createVertArray(const rawmesh *mesh, size_t *nverts)
{
    *nverts = mesh->nfaces * 3;
    vert_p4t2n3 *ret = (vert_p4t2n3 *) malloc(*nverts * sizeof(vert_p4t2n3));
    if (!ret)
    {
        fprintf(stderr, "Unable to allocate memory for vert array\n");
        *nverts = 0;
        return NULL;
    }

    size_t vi = 0;
    for (size_t i = 0; i < mesh->nfaces; i++)
    {
        const fullface &ff = mesh->ffaces[i];

        for (size_t j = 0; j < 3; j++)
        {
            ret[vi].pos   = glm::make_vec4(mesh->verts[ff.fverts[j].v].pos);
            ret[vi].norm  = mesh->norms[ff.fverts[j].vn];
            ret[vi].coord = mesh->coords[ff.fverts[j].vt];
            ++vi;
        }
    }
    assert(vi == *nverts);

    return ret;
}

vert_p4t2n3j8 * createSkinnedVertArray(const rawmesh *mesh, size_t *nverts)
{
    // This requires the extended vertex data
    assert(mesh->joints && mesh->weights);

    *nverts = mesh->nfaces * 3;
    vert_p4t2n3j8 *ret = (vert_p4t2n3j8 *) malloc(*nverts * sizeof(vert_p4t2n3j8));
    if (!ret)
    {
        fprintf(stderr, "Unable to allocate memory for vert array\n");
        *nverts = 0;
        return NULL;
    }

    size_t vi = 0;
    for (size_t i = 0; i < mesh->nfaces; i++)
    {
        const fullface &ff = mesh->ffaces[i];

        for (size_t j = 0; j < 3; j++)
        {
            size_t v = ff.fverts[j].v;
            size_t vn = ff.fverts[j].vn;
            size_t vt = ff.fverts[j].vt;
            ret[vi].pos   = glm::make_vec4(mesh->verts[v].pos);
            ret[vi].norm  = mesh->norms[vn];
            ret[vi].coord = mesh->coords[vt];
            for (size_t k = 0; k < 4; k++)
            {
                ret[vi].joints[k] = mesh->joints[4*v + k];
                ret[vi].weights[k] = mesh->weights[4*v + k];
            }
            ++vi;
        }
    }
    assert(vi == *nverts);

    return ret;
}

void show_info_log(
        GLuint object,
        PFNGLGETSHADERIVPROC glGet__iv,
        PFNGLGETSHADERINFOLOGPROC glGet__InfoLog
        )
{
    GLint log_length;
    char *log;

    glGet__iv(object, GL_INFO_LOG_LENGTH, &log_length);
    log = (char*) malloc(log_length);
    glGet__InfoLog(object, log_length, NULL, log);
    fprintf(stderr, "%s\n", log);
    free(log);
}


GLuint make_shader(GLenum type, const char *filename)
{
    GLint length;
    GLchar *source = (GLchar *)file_contents(filename, &length);
    GLuint shader;
    GLint shader_ok;

    if (!source)
        return 0;

    shader = glCreateShader(type);
    glShaderSource(shader, 1, (const GLchar**)&source, &length);
    free(source);
    glCompileShader(shader);

    glGetShaderiv(shader, GL_COMPILE_STATUS, &shader_ok);
    if (!shader_ok) {
        fprintf(stderr, "Failed to compile %s\n", filename);
        show_info_log(shader, glGetShaderiv, glGetShaderInfoLog);
        glDeleteShader(shader);
        return 0;
    }
    return shader;

}

GLuint make_program(GLuint vertex_shader, GLuint fragment_shader, GLuint geometry_shader)
{
    GLuint program = glCreateProgram();

    glAttachShader(program, geometry_shader);
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    GLint program_ok;
    glGetProgramiv(program, GL_LINK_STATUS, &program_ok);
    if (!program_ok) {
        fprintf(stderr, "Failed to link shader program:\n");
        show_info_log(program, glGetProgramiv, glGetProgramInfoLog);
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

shader* make_program(const char *vertfile, const char *fragfile)
{
    shader *ret;

    GLuint vert = make_shader(GL_VERTEX_SHADER, vertfile);
    GLuint frag = make_shader(GL_FRAGMENT_SHADER, fragfile);

    if (!vert || !frag)
    {
        fprintf(stderr, "Unable to create shader parts.\n");
        return NULL;
    }

    GLuint prog = make_program(vert, frag, 0 /*no geo shader*/);
    if (!prog)
    {
        glDeleteShader(vert);
        glDeleteShader(frag);
        return NULL;
    }

    ret = new shader;
    ret->vertex_shader = vert;
    ret->fragment_shader = frag;
    ret->program = prog;
    return ret;
}

void *file_contents(const char *filename, GLint *length)
{
    FILE *f = fopen(filename, "r");
    void *buffer;

    if (!f) {
        fprintf(stderr, "Unable to open %s for reading\n", filename);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    *length = ftell(f);
    fseek(f, 0, SEEK_SET);

    buffer = malloc(*length+1);
    *length = fread(buffer, 1, *length, f);
    fclose(f);
    ((char*)buffer)[*length] = '\0';

    return buffer;
}

glm::vec3 applyMatrix(const glm::mat4 &mat, const glm::vec3 &vec, bool homo)
{
    glm::vec4 pt(vec, homo ? 1 : 0);
    pt = mat * pt;
    if (homo) pt /= pt.w;
    return glm::vec3(pt);
}

