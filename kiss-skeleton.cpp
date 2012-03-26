#include "kiss-skeleton.h"
#include <GL/glew.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

const unsigned Skeleton::ROOT_PARENT = 255;

Skeleton::Skeleton() :
    renderer_(new SimpleBoneRenderer())
{
}

Skeleton::~Skeleton()
{
    // Clean up bone pointers
    for (size_t i = 0; i < joints_.size(); i++)
        delete joints_[i];
    joints_.clear();
}

void Skeleton::render(const glm::mat4 &transform) const
{
    assert(joints_.size() > 0);

    // don't render root bone
    for (size_t i = 0; i < joints_.size(); i++)
        (*renderer_)(transform, joints_[i], joints_);
}

void Skeleton::readJoint(const std::string &bonestr)
{
    std::stringstream ss(bonestr);
    std::string name;
    float x, y, z, a, rotx, roty, rotz, scale;
    unsigned parent;
    ss >> name >> x >> y >> z >> rotx >> roty >> rotz >> a >> scale >> parent;
    if (!ss)
    {
        std::cerr << "Could not read bone from string: '" << bonestr << "'\n";
        exit(1);
    }

    // Bone must either have a parent already read, or be the first, root bone
    assert(parent < joints_.size()
            || (parent == ROOT_PARENT && joints_.empty()));

    Joint *newbone = new Joint();
    newbone->name = name;
    newbone->scale = scale;
    newbone->pos = glm::vec3(x, y, z);
    newbone->rot = glm::vec4(rotx, roty, rotz, a);
    newbone->parent = parent;

    setWorldTransform(newbone);

    joints_.push_back(newbone);
}

void Skeleton::readSkeleton(const std::string &filename)
{
    std::ifstream file(filename.c_str());

    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty())
            continue;;
        readJoint(line);
    }
}

const Joint* Skeleton::getJoint(const std::string &name) const
{
    for (size_t i = 0; i < joints_.size(); i++)
        if (joints_[i]->name == name)
            return joints_[i];
    assert(false && "Joint not found");
    return NULL;
}

const Joint* Skeleton::getJoint(unsigned index) const
{
    assert(index < joints_.size());
    return joints_[index];
}

void Skeleton::setPose(const std::string &name, const JointPose *pose)
{
    size_t i;
    // first find the joint and update it
    for (i = 0; i < joints_.size(); i++)
    {
        if (joints_[i]->name == name)
        {
            joints_[i]->rot = pose->rot;
            joints_[i]->pos = pose->pos;
            joints_[i]->scale = pose->scale;
            break;
        }
    }
    // Then update the global transforms for it and all joints that might
    // depend on it
    for (; i < joints_.size(); i++)
        setWorldTransform(joints_[i]);
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

void Skeleton::setWorldTransform(Joint *joint)
{
    // Set transform as pa
    glm::mat4 transform(1.f);
    if (joint->parent != ROOT_PARENT)
    {
        assert(joint->parent < joints_.size());
        transform = joints_[joint->parent]->worldTransform;
    }

    // Translate from tip to start of current bone
    transform = glm::translate(transform,
            joint->pos);

    // Rotate...
    transform = glm::rotate(transform,
            joint->rot[3], glm::vec3(joint->rot));

    // Finally, scale
    transform = glm::scale(transform, glm::vec3(joint->scale));

    joint->worldTransform = transform;
}

static GLfloat cube_verts[] = {
    1,1,1,    -1,1,1,   -1,-1,1,  1,-1,1,        // v0-v1-v2-v3
    1,1,1,    1,-1,1,   1,-1,-1,  1,1,-1,        // v0-v3-v4-v5
    1,1,1,    1,1,-1,   -1,1,-1,  -1,1,1,        // v0-v5-v6-v1
    -1,1,1,   -1,1,-1,  -1,-1,-1, -1,-1,1,    // v1-v6-v7-v2
    -1,-1,-1, 1,-1,-1,  1,-1,1,   -1,-1,1,    // v7-v4-v3-v2
    1,-1,-1,  -1,-1,-1, -1,1,-1,  1,1,-1};   // v4-v7-v6-v5

static void renderCube()
{
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, cube_verts);

    glDrawArrays(GL_QUADS, 0, 24);

    glDisableClientState(GL_VERTEX_ARRAY);
}

void SimpleBoneRenderer::operator() (const glm::mat4 &transform, const Joint* joint,
        const std::vector<Joint*> joints)
{
    // Draw cube
    glLoadMatrixf(glm::value_ptr(glm::scale(transform * joint->worldTransform,
                    glm::vec3(0.08f))));
    glColor3f(1,1,1);
    renderCube();

    // No "bone" to draw for root
    if (joint->parent == 255)
        return;

    glLoadMatrixf(glm::value_ptr(transform));

    glm::mat4 parentTransform = joints[joint->parent]->worldTransform;
    glm::mat4 boneTransform = joint->worldTransform;

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

