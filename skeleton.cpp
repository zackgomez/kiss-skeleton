#include "skeleton.h"
#include <GL/glew.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdio>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "zgfx.h"

const unsigned Skeleton::ROOT_PARENT = 255;

Skeleton::Skeleton()
{
    // Create a root node with the identity transform @ the origin
    Joint *root = new Joint;
    root->rot = glm::vec4(0,0,1,0);
    root->pos = glm::vec3(0.f);
    root->scale = 1.f;
    root->index = 0;
    root->parent = ROOT_PARENT;
    root->worldTransform = glm::mat4(1.f);
    root->inverseBindTransform = glm::mat4(1.f);
    joints_.push_back(root);
}

Skeleton::~Skeleton()
{
    clearSkeleton();
}

Skeleton * Skeleton::readSkeleton(const std::string &filename)
{
    // TODO
    return 0;
}

Skeleton * Skeleton::readSkeleton(const char *text, size_t len)
{
    // TODO
    return 0;
}

void Skeleton::clearSkeleton()
{
    // Clean up joints
    for (size_t i = 0; i < joints_.size(); i++)
        delete joints_[i];
    joints_.clear();

    // clean up bones
    for (size_t i = 0; i < bones_.size(); i++)
        delete bones_[i];
    bones_.clear();
}

size_t Skeleton::numJoints() const
{
    return joints_.size();
}

size_t Skeleton::numBones() const
{
    return bones_.size();
}

const std::vector<Joint*> Skeleton::getJoints() const
{
    return joints_;
}

const std::vector<Bone*> Skeleton::getBones() const
{
    return bones_;
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
    if (!sp) return;

    for (size_t i = 0; i < sp->poses.size(); i++)
        delete sp->poses[i];
}

glm::vec3 Bone::p0() const
{
    glm::vec3 parentpos = applyMatrix(joint->worldTransform, glm::vec3(0.f));
    return parentpos;
}

glm::vec3 Bone::p1() const
{
    if (childjoint)
        return applyMatrix(childjoint->worldTransform, glm::vec3(0.f));
    else
        return applyMatrix(joint->worldTransform, endpt);
}

float Bone::length() const
{
    return glm::length(p1() - p0());
}


