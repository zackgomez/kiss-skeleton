#include "skeleton.h"
#include <GL/glew.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdio>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
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
    rbone->parent = NULL;

    bones_.push_back(rbone);
}

Skeleton::~Skeleton()
{
    clearSkeleton();
}

Skeleton * Skeleton::readSkeleton(const std::string &filename)
{
    std::ifstream f(filename.c_str());
    if (!f) return 0;
    return readSkeleton(f);
}

Skeleton * Skeleton::readSkeleton(const char *text, size_t len)
{
    std::stringstream ss(std::string(text, len));
    return readSkeleton(ss);
}

Skeleton *Skeleton::readSkeleton(std::istream &is)
{
    // start from scratch
    Skeleton *ret = new Skeleton();
    ret->clearSkeleton();

    std::string line;
    while (std::getline(is, line))
    {
        if (line.empty()) continue;
        std::string cmd;
        std::stringstream ss(line);

        ss >> cmd;
        if (cmd == "j")
        {
            Joint *j = new Joint;
            // j index parent pos rot scale
            ss >> j->index >> j->parent >> j->pos.x >> j->pos.y >> j->pos.z
                >> j->rot.x >> j->rot.y >> j->rot.z >> j->rot[3] >> j->scale;
            if (!ss)
            {
                // Print a message and try to continue
                printf("Unable to parse joint command '%s'\n", line.c_str());
                continue;
            }
            // Assumes the joints are in order
            assert(j->index == ret->joints_.size());
            assert(j->parent < ret->joints_.size() || j->parent == ROOT_PARENT);

            // Done, just add it
            ret->joints_.push_back(j);
        }
        else if (cmd == "b")
        {
            // b name jointidx tipPos
            Bone *b = new Bone;
            int jointIdx;
            ss >> b->name >> jointIdx >> b->tipPos.x >> b->tipPos.y >> b->tipPos.z;
            assert(jointIdx < ret->joints_.size() && jointIdx >= 0);
            b->joint = ret->joints_[jointIdx];

            // now deal with the parent
            b->parent = NULL;
            if (b->joint->parent != ROOT_PARENT)
            {
                // Find the bone with the same tipPos as our joint
                for (size_t i = 0; i < ret->bones_.size(); i++)
                {
                    Bone *cur = ret->bones_[i];
                    // Bone is parent if it's joint is our joints parent and
                    // it's tip is at our joint (they are same space then)
                    if (cur->joint->index == b->joint->parent
                        && cur->tipPos == b->joint->pos)
                    {
                        // connect up
                        b->parent = cur;
                        cur->children.push_back(b);
                        break;
                    }
                }
                assert(b->parent);
            }

            // Done, add it
            ret->bones_.push_back(b);
        }
        else
        {
            printf("Unable to parse line '%s' for skeleton\n",
                    line.c_str());
        }

    }

    printf("Read %zu joints for %zu bones.\n", ret->joints_.size(), ret->bones_.size());

    ret->updateTransforms();
    return ret;
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

Bone * Skeleton::newBoneHead(Bone *parent, const std::string &name)
{
    Bone *bone = new Bone;
    bone->name = name;
    bone->joint = parent->joint;
    bone->tipPos = glm::vec3(1, 1, 1); // TODO something better
    bone->parent = parent->parent;

    bones_.push_back(bone);

    updateTransforms();

    return bone;
}

Bone * Skeleton::newBoneTail(Bone *parent, const std::string &name)
{
    // Come up with the head joint
    Joint *parentJoint = NULL;
    if (parent->children.empty())
    {
        // make a new one
        parentJoint = new Joint;
        parentJoint->rot = glm::vec4(0,0,1,0);
        parentJoint->pos = parent->tipPos;
        parentJoint->scale = 1.f;
        parentJoint->index = joints_.size();
        parentJoint->parent = parent->joint->index;
        joints_.push_back(parentJoint);
    }
    else
    {
        // If there are children, then they should all share the same parent
        // joint just pick one 
        parentJoint = parent->children[0]->joint;
    }

    Bone *bone = new Bone;
    bone->name = name;
    bone->joint = parentJoint;
    bone->tipPos = glm::vec3(1, 0, 0); // TODO think about this a bit more
    bone->parent = parent;
    // no need to adjust children

    parent->children.push_back(bone);
    bones_.push_back(bone);

    updateTransforms();

    return bone;
}

void Skeleton::removeBone(Bone *bone)
{
    if (!bone) return;

    // remove from the list of children above us
    if (bone->parent)
    {
        size_t i;
        for (i = 0; i < bone->parent->children.size(); i++)
            if (bone->parent->children[i] == bone)
                break;
        bone->parent->children.erase(bone->parent->children.begin() + i);
    }

    // if we were the last child
    if (bone->parent && bone->parent->children.empty())
    {
        // clean up our joint
        joints_[bone->joint->index] = 0;
        delete bone->joint;
    }

    // recursively remove children
    removeBoneHelper(bone);

    compactJoints();
}

void Skeleton::removeBoneHelper(Bone *bone)
{
    // This bone is gone, so the "child" joint is gone too, all child bones
    // share the same child joint, so only delete the first one
    if (!bone->children.empty())
    {
        // zero out joint in array
        joints_[bone->children[0]->joint->index] = 0;
        // now delete it
        delete bone->children[0]->joint;
    }
    // Remove all children
    for (size_t i = 0; i < bone->children.size(); i++)
        removeBoneHelper(bone->children[i]);

    // remove from list, could be faster w/ swap trick
    bones_.erase(std::find(bones_.begin(), bones_.end(), bone));

    // and delete the bone
    delete bone;
}

void Skeleton::setBoneHeadPos(Bone *b, const glm::vec3 &worldPos)
{
    glm::mat4 parentTransform = b->joint->parent == ROOT_PARENT ?
        glm::mat4(1.f) :
        joints_[b->joint->parent]->worldTransform;

    glm::vec3 parentPos = applyMatrix(glm::inverse(parentTransform), worldPos);

    b->joint->pos = parentPos;

    // update the parent tailPos to parentPos as well
    if (b->parent)
    {
        b->parent->tipPos = parentPos;
    }

    updateTransforms();
}

void Skeleton::setBoneTailPos(Bone *b, const glm::vec3 &worldPos)
{
    glm::vec3 pos = applyMatrix(glm::inverse(b->joint->worldTransform), worldPos);
    b->tipPos = pos;

    // update all child bones->joint to have correct position
    for (size_t i = 0; i < b->children.size(); i++)
        b->children[i]->joint->pos = pos;

    updateTransforms();
}
void Skeleton::setBoneRotation(Bone *b, const glm::vec4 &rot)
{
    b->joint->rot = rot;
    updateTransforms();
}

void Skeleton::setBoneScale(Bone *b, float scale)
{
    b->joint->scale = scale;
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
    if (!joint) return;

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

void Skeleton::compactJoints()
{
    size_t spaces_filled = 0;
    // Find an empty spot
    for (size_t i = 0; i < joints_.size(); )
    {
        if (joints_[i] == NULL)
        {
            // Found an empty spot @ i, now shift i+1 to the end left one position
            // j iterates over target position
            for (size_t j = i; j < joints_.size() - 1; j++)
            {
                joints_[j] = joints_[j + 1];
                // update index/parent
                if (!joints_[j]) continue;
                joints_[j]->index = j;
                // if the parent is being moved, decrement (moved by 1)
                if (joints_[j]->parent > i)
                    joints_[j]->parent--;
            }
            // The end is now garbage, get rid of it
            joints_.pop_back();
            spaces_filled++;
        }
        else
            i++;
    }
    printf("Compacted %zu spaces.  Number of joints: %zu.\n", spaces_filled,
            joints_.size());
}

void freeSkeletonPose(SkeletonPose *sp)
{
    if (!sp) return;

    for (size_t i = 0; i < sp->poses.size(); i++)
        delete sp->poses[i];
}

int findBoneIndex(const std::vector<Bone*> &bones, const Bone *b)
{
    for (size_t i = 0; i < bones.size(); i++)
        if (bones[i] == b)
            return i;
    return -1;
}

void writeSkeleton(const Skeleton *skeleton, std::ostream &os)
{
    // Start by writing the joints
    const std::vector<Joint*> joints = skeleton->getJoints();
    for (size_t i = 0; i < joints.size(); i++)
    {
        const Joint *j = joints[i];
        // j index parent pos rot scale
        os  << "j " << j->index << '\t' << j->parent << '\t' << j->pos << '\t'
            << j->rot << '\t' << j->scale << '\n';
    }

    os << "\n\n";

    // Now the bones
    const std::vector<Bone*> bones = skeleton->getBones();
    for (size_t i = 0; i < bones.size(); i++)
    {
        const Bone *b = bones[i];
        // b name jointidx tipPos
        os << "b " << b->name << ' ' << b->joint->index << ' ' << b->tipPos << '\n';
    }
}

