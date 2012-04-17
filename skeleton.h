#pragma once
#include <string>
#include <map>
#include <vector>
#include <glm/glm.hpp>
#include <cstdio>

struct Joint
{
    glm::vec4 rot; // x,y,z, angle
    glm::vec3 pos;
    float scale;

    unsigned index;
    unsigned parent;

    glm::mat4 worldTransform;
    glm::mat4 inverseBindTransform;
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

// TODO a comment
struct Bone
{
    std::string name;
    Joint *joint;
    // only one of the next two will be valid
    Joint *childjoint; // NULL when not a joint, just a point
    glm::vec3 endpt;

    // These functions return the world position of the bone
    glm::vec3 p0() const;
    glm::vec3 p1() const;
    float length() const;
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
    const Joint* getJoint(unsigned index) const;

    void setPose(unsigned index, const JointPose *pose);
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

    void setWorldTransform(Joint* bone);
    void clearSkeleton();
};

void freeSkeletonPose(SkeletonPose *sp);

