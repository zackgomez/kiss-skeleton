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

// Callback functor for render each bone.
// 
struct BoneRenderer
{
    virtual void operator() (const glm::mat4 &transform, const Joint* b, const std::vector<Joint*> joints) = 0;
};

struct SimpleBoneRenderer : public BoneRenderer
{
    virtual void operator() (const glm::mat4 &transform, const Joint* b, const std::vector<Joint*> joints);
};

class Skeleton
{
public:
    Skeleton();
    ~Skeleton();

    void render(const glm::mat4 &transform) const;

    void setBoneRenderer(BoneRenderer *renderer);
    void setDefaultRenderer();

    void readSkeleton(const std::string &filename);

private:
    std::vector<Joint*> joints_;

    BoneRenderer *renderer_;

    void setWorldTransform(Joint* bone);
    void readJoint(const std::string &jointstr);

    static const unsigned ROOT_PARENT;
};

