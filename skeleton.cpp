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

    Bone *rbone = new Bone;
    rbone->name = "root";
    rbone->joint = root;
    rbone->tipPos = glm::vec3(1, 0, 0);

    bones_.push_back(rbone);
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

Joint* Skeleton::getJoint(unsigned index)
{
    assert(index < joints_.size());
    return joints_[index];
}

Bone* Skeleton::getBone(const std::string name)
{
    for (size_t i = 0; i < bones_.size(); i++)
        if (bones_[i]->name == name)
            return bones_[i];

    printf("WARNING: bone '%s' not found.\n", name.c_str());
    return NULL;
}

void Skeleton::setBoneHeadPos(Bone *b, const glm::vec3 &worldPos)
{
    glm::mat4 parentTransform = b->joint->parent == ROOT_PARENT ?
        glm::mat4(1.f) :
        joints_[b->joint->parent]->worldTransform;

    glm::vec3 parentPos = applyMatrix(glm::inverse(parentTransform), worldPos);

    b->joint->pos = parentPos;

    // TODO update the parent tailPos to parentPos as well
    // TODO what if that affects other children??

    updateTransforms();
}

void Skeleton::setBoneMidPos(Bone *b, const glm::vec3 &worldDelta)
{
    // TODO
}

void Skeleton::setBoneTailPos(Bone *b, const glm::vec3 &worldPos)
{
    glm::vec3 pos = applyMatrix(glm::inverse(b->joint->worldTransform), worldPos);
    b->tipPos = pos;

    // TODO update all child bones->joint to have correct position

    updateTransforms();
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

void Skeleton::updateTransforms()
{
    for (size_t i = 0; i < joints_.size(); i++)
        setWorldTransform(joints_[i]);
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

