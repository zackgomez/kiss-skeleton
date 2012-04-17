#pragma once
#include <string>
#include <map>
#include <vector>
#include <glm/glm.hpp>
#include <cstdio>

struct Bone;
struct Joint
{
    glm::vec4 rot; // x,y,z, angle
    glm::vec3 pos; // relative to head of last bone
    float scale;

    unsigned index;
    unsigned parent;

    glm::mat4 worldTransform;
    glm::mat4 inverseBindTransform;
};

struct Bone
{
    std::string name;
    Joint *joint; // the joint at the head of this bone, controls the verts
    glm::vec3 tipPos; // position of the tip ibonen bone space

    //parent
    //children
};

struct JointPose
{
    glm::vec4 rot;
    glm::vec3 pos;
    float scale;
};

struct SkeletonPose
{
    std::vector<JointPose*> poses;
};


class Skeleton
{
public:
    // Returns null on error
    static Skeleton * readSkeleton(const std::string &filename);
    // Returns null on error
    static Skeleton * readSkeleton(const char *text, size_t len);

    // Creates a skeleton with only a root node
    Skeleton();
    ~Skeleton();

    size_t numJoints() const;
    size_t numBones() const;

    const std::vector<Joint*> getJoints() const;
    const std::vector<Bone*> getBones() const;

    Bone* getBone(const std::string name);
    Joint* getJoint(unsigned index);

    void setBoneHeadPos(Bone *b, const glm::vec3 &worldPos);
    void translateBone(Bone *b, const glm::vec3 &worldDelta);
    void translateTail(Bone *b, const glm::vec3 &worldDelta);

    void setPose(const SkeletonPose *sp);

    // Sets the current pose as the bind pose
    void setBindPose();

    // Returns the current pose.
    // returned value MUST be freed with freeSkeletonPose
    SkeletonPose *currentPose() const;

    // The parent index of the root bone
    static const unsigned ROOT_PARENT;

private:
    std::vector<Joint*> joints_;
    std::vector<Bone*> bones_;

    void updateTransforms();
    void setWorldTransform(Joint* bone);
    void clearSkeleton();
};

void freeSkeletonPose(SkeletonPose *sp);

