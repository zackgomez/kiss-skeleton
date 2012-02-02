#include <string>
#include <map>
#include <vector>
#include <glm/glm.hpp>


struct Bone
{
    std::string name;
    float length;

    glm::vec3 pos;
    // x,y,z, angle
    glm::vec4 rot;
    
    Bone *parent;
    std::vector<Bone*> children;

    Bone(const std::string &nm, float l,
            const glm::vec3 &relpos, const glm::vec4 &relrot,
            Bone *parnt) :
        name(nm), length(l), pos(relpos), rot(relrot),
        parent(parnt)
    { }
};

struct BoneFrame
{
    float length;
    glm::vec4 rot;
};

struct Keyframe
{
    int frame;
    std::map<std::string, BoneFrame> bones;
};

struct Animation
{
    std::string name;
    int numframes;
    std::vector<Keyframe> keyframes;
};

// Callback functor for render each bone.
// 
struct BoneRenderer
{
    virtual void operator() (const glm::mat4 &transform, const Bone* b) = 0;
};

struct SimpleBoneRenderer : public BoneRenderer
{
    virtual void operator() (const glm::mat4 &transform, const Bone* b);
};

class Skeleton
{
public:
    Skeleton();
    ~Skeleton();

    void render(const glm::mat4 &transform) const;
    void dumpPose(std::ostream &os) const;

    // Parameters to setBoneTipPosition mode type
    static const int ANGLE_MODE  = 1;
    static const int LENGTH_MODE = 2;

    void setPose(const std::map<std::string, BoneFrame> &pose);
    void setBoneTipPosition(const std::string &bone, const glm::vec3 &targetPos,
            int mode);
    void resetPose();

    Keyframe getPose() const;

    void readSkeleton(const std::string &filename);

    void setBoneRenderer(BoneRenderer *renderer);
    void setDefaultRenderer();

private:
    std::map<std::string, Bone *> bones_;

    void renderBone(const glm::mat4 &parentTransform, const Bone *bone) const;
    void readBone(const std::string &bonestr);
    void printBone(const Bone *bone, std::ostream &os) const;
    glm::mat4 getBoneMatrix(const Bone* bone) const;
    glm::mat4 getFullBoneMatrix(const Bone* bone) const;

    BoneRenderer *renderer_;

    Keyframe refPose_;

    // TODO add reference pose, computed in readSkeleton
};

