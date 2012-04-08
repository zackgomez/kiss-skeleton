#include "kiss-skeleton.h"
#include <GL/glew.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

const unsigned Skeleton::ROOT_PARENT = 255;

Skeleton::Skeleton()
{
}

Skeleton::~Skeleton()
{
    // Clean up bone pointers
    for (size_t i = 0; i < joints_.size(); i++)
        delete joints_[i];
    joints_.clear();
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
    newbone->index  = joints_.size();
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

    // the read position is the bind position
    setBindPose();
}

const std::vector<Joint*> Skeleton::getJoints() const
{
    return joints_;
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

void Skeleton::setPose(unsigned index, const JointPose *pose)
{
    // first find the joint and update it
    joints_[index]->rot = pose->rot;
    joints_[index]->pos = pose->pos;
    joints_[index]->scale = pose->scale;
    // Then update the global transforms for it and all joints that might
    // depend on it
    for (size_t i = index; i < joints_.size(); i++)
        setWorldTransform(joints_[i]);
}

void Skeleton::setPose(const SkeletonPose *sp)
{
    assert(sp->poses.size() == joints_.size());

    for (size_t i = 0; i < joints_.size(); i++)
    {
        Joint *j = joints_[i];
        const JointPose *p = sp->poses[i];

        j->pos = p->pos;
        j->rot = p->rot;
        j->scale = p->scale;
        setWorldTransform(j);
    }
}

void Skeleton::setBindPose()
{
    for (size_t i = 0; i < joints_.size(); i++)
        joints_[i]->inverseBindTransform = glm::inverse(joints_[i]->worldTransform);
}

SkeletonPose* Skeleton::currentPose() const
{
    SkeletonPose *pose = new SkeletonPose();
    pose->poses = std::vector<JointPose*>(joints_.size());

    for (size_t i = 0; i < joints_.size(); i++)
    {
        JointPose *p = new JointPose();
        const Joint *j = joints_[i];
        p->pos = j->pos;
        p->rot = j->rot;
        p->scale = j->scale;
        pose->poses[i] = p;
    }

    return pose;
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

void freeSkeletonPose(SkeletonPose *sp)
{
    for (size_t i = 0; i < sp->poses.size(); i++)
        delete sp->poses[i];
}

