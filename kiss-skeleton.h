#pragma once
#include <string>
#include <map>
#include <vector>
#include <glm/glm.hpp>

struct Joint
{
    glm::vec4 rot; // x,y,z, angle
    glm::vec3 pos;
    float scale;

    std::string name;
    
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

class Skeleton
{
public:
    Skeleton();
    ~Skeleton();

    void readSkeleton(const std::string &filename);

    const std::vector<Joint*> getJoints() const;
    const Joint* getJoint(const std::string &name) const;
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

    void setWorldTransform(Joint* bone);
    void readJoint(const std::string &jointstr);
};


void freeSkeletonPose(SkeletonPose *sp);
