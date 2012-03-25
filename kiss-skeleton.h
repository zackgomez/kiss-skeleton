#pragma once
#include <string>
#include <map>
#include <vector>
#include <glm/glm.hpp>

struct Bone
{
    glm::vec4 rot; // x,y,z, angle
    glm::vec3 pos;
    float scale;

    std::string name;
    
    int parent;

    glm::mat4 worldTransform;
};

struct BonePose
{
    glm::vec4 rot;
    glm::vec3 pos;
    float scale;
};

struct SkeletonPose
{
    std::vector<BonePose*> poses;
};

// Callback functor for render each bone.
// 
struct BoneRenderer
{
    virtual void operator() (const glm::mat4 &transform, const Bone* b, const std::vector<Bone*> bones) = 0;
};

struct SimpleBoneRenderer : public BoneRenderer
{
    virtual void operator() (const glm::mat4 &transform, const Bone* b, const std::vector<Bone*> bones);
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
    std::vector<Bone*> bones_;

    BoneRenderer *renderer_;

    void setWorldTransform(Bone* bone);
    void readBone(const std::string &bonestr);

    static const int ROOT_PARENT;
};

