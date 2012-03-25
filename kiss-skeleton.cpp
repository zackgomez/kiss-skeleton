#include "kiss-skeleton.h"
#include <GL/glew.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

const int Skeleton::ROOT_PARENT = 255;

Skeleton::Skeleton() :
    renderer_(new SimpleBoneRenderer())
{
}

Skeleton::~Skeleton()
{
    // Clean up bone pointers
    for (size_t i = 0; i < bones_.size(); i++)
        delete bones_[i];
    bones_.clear();
}

void Skeleton::render(const glm::mat4 &transform) const
{
    assert(bones_.size() > 0);

    // don't render root bone
    for (size_t i = 1; i < bones_.size(); i++)
        (*renderer_)(transform, bones_[i], bones_);
}

void Skeleton::readBone(const std::string &bonestr)
{
    std::stringstream ss(bonestr);
    std::string name;
    float x, y, z, a, rotx, roty, rotz, scale;
    int parent;
    ss >> name >> x >> y >> z >> rotx >> roty >> rotz >> a >> scale >> parent;
    if (!ss)
    {
        std::cerr << "Could not read bone from string: '" << bonestr << "'\n";
        exit(1);
    }

    // Bone must either have a parent already read, or be the first, root bone
    assert(parent < bones_.size()
            || (parent == ROOT_PARENT && bones_.empty()));

    Bone *newbone = new Bone();
    newbone->name = name;
    newbone->scale = scale;
    newbone->pos = glm::vec3(x, y, z);
    newbone->rot = glm::vec4(rotx, roty, rotz, a);
    newbone->parent = parent;

    setWorldTransform(newbone);

    bones_.push_back(newbone);
}

void Skeleton::readSkeleton(const std::string &filename)
{
    std::ifstream file(filename.c_str());

    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty())
            continue;;
        readBone(line);
    }
}

void Skeleton::setBoneRenderer(BoneRenderer *br)
{
    delete renderer_;
    renderer_ = br;
}

void Skeleton::setDefaultRenderer()
{
    delete renderer_;
    renderer_ = new SimpleBoneRenderer();
}

void Skeleton::setWorldTransform(Bone *bone)
{
    // Set transform as pa
    glm::mat4 transform(1.f);
    if (bone->parent != ROOT_PARENT)
    {
        assert(bone->parent < bones_.size());
        transform = bones_[bone->parent]->worldTransform;
    }

    // Translate from tip to start of current bone
    transform = glm::translate(transform,
            bone->pos);

    // Rotate...
    transform = glm::rotate(transform,
            bone->rot[3], glm::vec3(bone->rot));

    // Finally, scale
    transform = glm::scale(transform, glm::vec3(bone->scale));

    bone->worldTransform = transform;
}

GLfloat cube_verts[] = {
    1,1,1,    -1,1,1,   -1,-1,1,  1,-1,1,        // v0-v1-v2-v3
    1,1,1,    1,-1,1,   1,-1,-1,  1,1,-1,        // v0-v3-v4-v5
    1,1,1,    1,1,-1,   -1,1,-1,  -1,1,1,        // v0-v5-v6-v1
    -1,1,1,   -1,1,-1,  -1,-1,-1, -1,-1,1,    // v1-v6-v7-v2
    -1,-1,-1, 1,-1,-1,  1,-1,1,   -1,-1,1,    // v7-v4-v3-v2
    1,-1,-1,  -1,-1,-1, -1,1,-1,  1,1,-1};   // v4-v7-v6-v5


void renderCube()
{
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, cube_verts);

    glDrawArrays(GL_QUADS, 0, 24);

    glDisableClientState(GL_VERTEX_ARRAY);
}

void SimpleBoneRenderer::operator() (const glm::mat4 &transform, const Bone* bone,
        const std::vector<Bone*> bones)
{
    // Draw cube
    glLoadMatrixf(glm::value_ptr(glm::scale(transform * bone->worldTransform, glm::vec3(0.1f))));
    glColor3f(1,1,1);
    renderCube();

    // No "bone" to draw for root
    if (bone->parent == 255)
        return;

    glLoadMatrixf(glm::value_ptr(transform));

    glm::mat4 parentTransform = bones[bone->parent]->worldTransform;
    glm::mat4 boneTransform = bone->worldTransform;

    glm::vec4 start(0, 0, 0, 1), end(0, 0, 0, 1);
    start = parentTransform * start;
    start /= start.w;
    end = boneTransform * end;
    end /= end.w;

    glBegin(GL_LINES);
    glColor3f(1, 0, 0);
    glVertex4fv(&start.x);

    glColor3f(0, 1, 0);
    glVertex4fv(&end.x);
    glEnd();
}

