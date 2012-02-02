#include "kiss-skeleton.h"
#include <GL/glew.h>
#include <iostream>
#include <sstream>
#include <fstream>

void renderCube();

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

void Skeleton::render() const
{
    assert(bones_.size() > 0);
    assert(bones_.find("root") != bones_.end());
    renderBone(bones_.find("root")->second);
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
    // TODO setPose(referencePose_);
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
}

void Skeleton::renderBone(const Bone *bone) const
{
    if (!bone)
        return;

    glPushMatrix();

    glTranslatef(bone->pos[0], bone->pos[1], bone->pos[2]);
    glRotatef(bone->rot[3], bone->rot[0], bone->rot[1], bone->rot[2]);

    // render the bone using the current renderer
    (*renderer_)(bone);

    // Move to the tip of the bone
    glTranslatef(bone->length, 0, 0);

    // Render children recursively
    for (size_t i = 0; i < bone->children.size(); i++)
        renderBone(bone->children[i]);

    glPopMatrix();
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

void SimpleBoneRenderer::operator() (const Bone* bone)
{
    glBegin(GL_LINES);
        glColor3f(0, 1, 0);
        glVertex3f(0, 0, 0);
        glColor3f(1, 0, 0);
        glVertex3f(bone->length, 0, 0);
    glEnd();
}
