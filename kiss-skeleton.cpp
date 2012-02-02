#include "kiss-skeleton.h"
#include <GL/glew.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

Skeleton::Skeleton() :
    renderer_(new SimpleBoneRenderer())
{
}

Skeleton::~Skeleton()
{
    // Clean up bone pointers
    std::map<std::string, Bone*>::iterator it;
    for (it = bones_.begin(); it != bones_.end(); it++)
        delete it->second;
}

void Skeleton::render(const glm::mat4 &transform) const
{
    assert(bones_.size() > 0);
    assert(bones_.find("root") != bones_.end());

    renderBone(transform, bones_.find("root")->second);
}

void Skeleton::dumpPose(std::ostream &os) const
{
    assert(bones_.find("root") != bones_.end());
    printBone(bones_.find("root")->second, os);
}

void Skeleton::setPose(const std::map<std::string, BoneFrame> &pose)
{
    std::map<std::string, BoneFrame>::const_iterator it;
    for (it = pose.begin(); it != pose.end(); it++)
    {
        const std::string name = it->first;
        const BoneFrame bframe = it->second;

        assert(bones_.find(name) != bones_.end());

        Bone *bone = bones_[name];
        bone->length = bframe.length;
        bone->rot = bframe.rot;
    }
}

void Skeleton::setBoneTipPosition(const std::string &bonename, const glm::vec3 &targetPos,
        int mode)
{
    assert(bones_.find(bonename) != bones_.end());
    Bone *bone = bones_[bonename];

    // First get the parent transform, we can't adjust that
    glm::mat4 parentTransform = getFullBoneMatrix(bone->parent);
    glm::mat4 inverseParentTransform = glm::inverse(parentTransform);
    glm::mat4 curBoneTransform = glm::translate(glm::mat4(1.f), bone->pos);
    curBoneTransform = glm::rotate(curBoneTransform, bone->rot.w, glm::vec3(bone->rot));
    curBoneTransform = glm::translate(curBoneTransform, glm::vec3(bone->length, 0.f, 0.f));

    glm::vec4 pt(targetPos, 1.f);
    pt = inverseParentTransform * pt; pt /= pt.w;


    glm::vec3 rotvec = glm::vec3(bone->rot);
    float angle = bone->rot.w;
    float length = bone->length;
    if (mode == ANGLE_MODE)
    {
        // vector in direction of point of bone length (parent space)
        glm::vec3 ptvec = glm::normalize(glm::vec3(pt)) * bone->length;

        // Get the angle between untransformed and target vector (parent space)
        angle = 180.f / M_PI * acos(glm::dot(glm::normalize(ptvec), glm::vec3(1, 0, 0)));
        // Create an orthogonal vector to rotate around (parent space)
        rotvec = glm::cross(glm::vec3(1, 0, 0), glm::normalize(ptvec));
    }
    else if (mode == LENGTH_MODE)
    {
        glm::vec4 parentpt = parentTransform * glm::vec4(0, 0, 0, 1);
        parentpt /= parentpt.w;

        length = glm::length(glm::vec3(pt - parentpt));
        std::cout << "new length (distance): " << length << '\n';
        /*
        // curpt is in parent space
        glm::vec4 curpt = curBoneTransform * glm::vec4(0, 0, 0, 1);
        curpt /= curpt.w;

        // a dot b / mag b [proj a onto b]
        length = glm::dot(glm::vec3(curpt), worldPos) / glm::length(curpt);

        std::cout << "length: " << bone->length << " |curpt|: " << glm::length(glm::vec3(curpt))
            << " new length (projection): " << length << '\n';
        */
    }
    else
    {
        assert(false && "unknown bone transform mode");
    }

    bone->rot = glm::vec4(rotvec, angle);
    bone->length = length;
}

Keyframe Skeleton::getPose() const
{
    Keyframe kf;

    std::map<std::string, Bone*>::const_iterator it;
    for (it = bones_.begin(); it != bones_.end(); it++)
    {
        const std::string &bname = it->first;
        const Bone *bone = it->second;
        BoneFrame bf;
        bf.length = bone->length;
        bf.rot = bone->rot;

        kf.bones[bname] = bf;
    }

    return kf;
}

void Skeleton::resetPose()
{
    setPose(refPose_.bones);
}

void Skeleton::readBone(const std::string &bonestr)
{
    std::stringstream ss(bonestr);
    std::string name, parentname;
    float x, y, z, a, rotx, roty, rotz, length;
    ss >> name >> x >> y >> z >> rotx >> roty >> rotz >> a >> length >> parentname;
    if (!ss)
    {
        std::cerr << "Could not read bone from string: '" << bonestr << "'\n";
        assert(false);
    }

    Bone *parent;
    if (parentname == "NULL")
    {
        assert(name == "root");
        parent = NULL;
    }
    else
    {
        assert(bones_.find(parentname) != bones_.end());
        parent = bones_[parentname];
    }

    Bone *newbone = new Bone(name, length, glm::vec3(x, y, z), glm::vec4(rotx, roty, rotz, a), parent);
    if (parent)
        parent->children.push_back(newbone);
    bones_[name] = newbone;
}

void Skeleton::readSkeleton(const std::string &filename)
{
    std::ifstream file(filename.c_str());

    std::string line;
    while (std::getline(file, line))
        readBone(line);

    refPose_ = getPose();
}

void Skeleton::renderBone(const glm::mat4 &parentTransform, const Bone *bone) const
{
    if (!bone)
        return;

    glm::mat4 curTransform = glm::translate(parentTransform, bone->pos);
    curTransform = glm::rotate(curTransform, bone->rot[3], glm::vec3(bone->rot));

    // render the bone using the current renderer
    (*renderer_)(curTransform, bone);

    // Move to the tip of the bone
    curTransform = glm::translate(curTransform, glm::vec3(bone->length, 0.f, 0.f));

    // Render children recursively
    for (size_t i = 0; i < bone->children.size(); i++)
        renderBone(curTransform, bone->children[i]);
}

void Skeleton::printBone(const Bone *cur, std::ostream &os) const
{
    if (cur == NULL)
        return;

    os  << cur->name << ' ' << cur->pos[0] << ' ' << cur->pos[1] << ' ' << cur->pos[2] << ' '
        << cur->rot[0] << ' ' << cur->rot[1] << ' ' << cur->rot[2] << ' ' << cur->rot[3] << ' '
        << cur->length << ' ' << (cur->parent == NULL ? "NULL" : cur->parent->name) << '\n';

    for (size_t i = 0; i < cur->children.size(); i++)
        printBone(cur->children[i], os);
}

glm::mat4 Skeleton::getBoneMatrix(const Bone* bone) const
{
    // null bone, identity transform
    if (!bone) return glm::mat4(1.f);

    // The transform for this bone
    glm::mat4 transform = glm::translate(glm::mat4(1.f), bone->pos);
    transform = glm::rotate(transform, bone->rot[3], glm::vec3(bone->rot));
    transform = glm::translate(transform, glm::vec3(bone->length, 0.f, 0.f));

    return transform;
}

glm::mat4 Skeleton::getFullBoneMatrix(const Bone* bone) const
{
    // base case, null bone, identity transform
    if (!bone) return glm::mat4(1.f);

    glm::mat4 transform = getBoneMatrix(bone);
    glm::mat4 parentTransform = getFullBoneMatrix(bone->parent);

    return parentTransform * transform;
}

void Skeleton::setBoneRenderer(BoneRenderer *br)
{
    delete renderer_;
    renderer_ = br;
}

void Skeleton::setDefaultRenderer()
{
    delete renderer_;
    renderer_ = new SimpleBoneRenderer();
}

void SimpleBoneRenderer::operator() (const glm::mat4 &transform, const Bone* bone)
{
    glPushMatrix();
    
    glMultMatrixf(glm::value_ptr(transform));

    glBegin(GL_LINES);
        glColor3f(0, 1, 0);
        glVertex3f(0, 0, 0);
        glColor3f(1, 0, 0);
        glVertex3f(bone->length, 0, 0);
    glEnd();

    glPopMatrix();
}

