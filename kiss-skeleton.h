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
    
    unsigned parent;

    glm::mat4 worldTransform;
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
    void setPose(const std::string& name, const JointPose *pose);

    // The parent index of the root bone
    static const unsigned ROOT_PARENT;

private:
    std::vector<Joint*> joints_;

    void setWorldTransform(Joint* bone);
    void readJoint(const std::string &jointstr);
};

