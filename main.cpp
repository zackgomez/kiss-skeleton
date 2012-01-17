#include <GL/glew.h>
#include <GL/glut.h>
#include <math.h>       /* for cos(), sin(), and sqrt() */
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <map>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "uistate.h"

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

void renderBone(Bone *bone);
void printBone(Bone *skeleton);
void readBone(const std::string &bonestr, std::map<std::string, Bone*> &skeleton);
Animation readAnimation(const std::string &filename);
void setPose(std::map<std::string, Bone*> &skeleton,
        const Animation &anim, int frame);
void setPose(std::map<std::string, Bone*> &skeleton,
        const std::map<std::string, BoneFrame> &pose);

int windowWidth = 800, windowHeight = 600;

std::map<std::string, Bone*> skeleton;
Animation anim;
int frame = 0;

// Peter's mystical ui controller for arcball transformation and stuff
static UIState *ui;

void redraw(void)
{
    // Now render
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Apply the camera transformation.
    ui->ApplyViewingTransformation();

    // Set the bone pose
    setPose(skeleton, anim, frame);

    glMatrixMode(GL_MODELVIEW);
    renderBone(skeleton["root"]);

    glutSwapBuffers();
}

void reshape(int width, int height)
{
    windowWidth = width;
    windowHeight = height;
    
    if( width <= 0 || height <= 0 ) return;
    
    ui->WindowX() = width;
    ui->WindowY() = height;
    
    ui->Aspect() = float( width ) / height;
    ui->SetupViewport();
    ui->SetupViewingFrustum();
}

void mouse(int button, int state, int x, int y)
{
    // Just pass it on to the ui controller.
    ui->MouseFunction(button, state, x, y);
}

void motion(int x, int y)
{
    // Just pass it on to the ui controller.
    ui->MotionFunction(x, y);
}

void keyboard(GLubyte key, GLint x, GLint y)
{
    // Quit on ESC
    if (key == 27)
        exit(0);
    if (key == 'd')
        printBone(skeleton["root"]);
    if (key == 'n')
    {
        frame++;
        glutPostRedisplay();
    }
    if (key == 'r')
    {
        frame = 0;
        glutPostRedisplay();
    }
}

int main(int argc, char **argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH | GLUT_MULTISAMPLE);

    glutCreateWindow("kiss_particle demo");
    glutDisplayFunc(redraw);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutKeyboardFunc(keyboard);

    GLenum err = glewInit();
    if (err != GLEW_OK)
    {
        std::cerr << "Unable to initialize GLEW: " << glewGetErrorString(err) << '\n';
        exit(1);
    }

    glEnable(GL_DEPTH_TEST);

    ui = new UIState;
    ui->Trans() = glm::vec3(0, 0, 0);
    ui->Radius() = 80;
    ui->Near() = .1;
    ui->Far() = 1000;
    ui->CTrans() = glm::vec3(0, 0, -40);

    // --------------------
    // START MY SETUP
    // --------------------

    std::string bonefile = "test.bones";

    std::ifstream file(bonefile.c_str());

    std::string line;
    while (std::getline(file, line))
        readBone(line, skeleton);

    anim = readAnimation("testanim.anim");

    // --------------------
    // END MY SETUP
    // --------------------

    glutMainLoop();
    return 0;             /* ANSI C requires main to return int. */
}

void renderBone(Bone *bone)
{
    if (!bone)
        return;

    glPushMatrix();

    glTranslatef(bone->pos[0], bone->pos[1], bone->pos[2]);
    glRotatef(bone->rot[3], bone->rot[0], bone->rot[1], bone->rot[2]);

    glBegin(GL_LINES);
        glColor3f(0, 1, 0);
        glVertex3f(0, 0, 0);
        glColor3f(1, 0, 0);
        glVertex3f(bone->length, 0, 0);
    glEnd();


    glTranslatef(bone->length, 0, 0);

    for (size_t i = 0; i < bone->children.size(); i++)
    {
        renderBone(bone->children[i]);
    }

    glPopMatrix();
}

void readBone(const std::string &bonestr, std::map<std::string, Bone*> &skeleton)
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
        assert(skeleton.find(parentname) != skeleton.end());
        parent = skeleton[parentname];
    }

    Bone *newbone = new Bone(name, length, glm::vec3(x, y, z), glm::vec4(rotx, roty, rotz, a), parent);
    if (parent)
        parent->children.push_back(newbone);
    skeleton[name] = newbone;
}



void printBone(Bone *cur)
{
    if (cur == NULL)
        return;

    std::cout << cur->name << ' ' << cur->pos[0] << ' ' << cur->pos[1] << ' ' << cur->pos[2] << ' '
        << cur->rot[0] << ' ' << cur->rot[1] << ' ' << cur->rot[2] << ' ' << cur->rot[3] << ' '
        << cur->length << ' ' << (cur->parent == NULL ? "NULL" : cur->parent->name) << '\n';

    for (size_t i = 0; i < cur->children.size(); i++)
        printBone(cur->children[i]);
}

void setPose(std::map<std::string, Bone*> &skeleton,
        const std::map<std::string, BoneFrame> &pose)
{
    std::map<std::string, BoneFrame>::const_iterator it;
    for (it = pose.begin(); it != pose.end(); it++)
    {
        const std::string name = it->first;
        const BoneFrame bframe = it->second;

        std::cout << "Setting bone " << name << " length " << bframe.length << '\n';

        assert(skeleton.find(name) != skeleton.end());

        Bone *bone = skeleton[name];
        bone->length = bframe.length;
        bone->rot = bframe.rot;

        std::cout << "Did it work? length = " << skeleton[name]->length << '\n';
    }
}

BoneFrame readBoneFrame(std::istream &is)
{
    BoneFrame bf;
    is >> bf.length >> bf.rot.x >> bf.rot.y >> bf.rot.z >> bf.rot[3];
    if (!is)
    {
        std::cerr << "Unable to read BoneFrame\n";
        exit(1);
    }

    return bf;
}

Animation readAnimation(const std::string &filename)
{
    Animation anim;
    Keyframe keyframe;
    
    std::ifstream file(filename.c_str());
    if (!file)
    {
        std::cerr << "Unable to open animation file: " << filename << '\n';
        exit(1);
    }

    file >> anim.name >> anim.numframes;
    if (!file)
    {
        std::cerr << "Unable to read header from animation file: " << filename << '\n';
        exit(1);
    }

    std::string line;
    keyframe.frame = -1;
    while (std::getline(file, line))
    {
        if (line.empty())
            continue;

        std::stringstream ss(line);
        std::string name;
        ss >> name;
        if (name == "KEYFRAME")
        {
            int num;
            ss >> num;
            if (!ss)
            {
                std::cerr << "Unable to read keyframe line: " << line << '\n';
                exit(1);
            }

            if (keyframe.frame >= 0)
                anim.keyframes.push_back(keyframe);
            keyframe = Keyframe();
            keyframe.frame = num;
        }
        else
        {
            BoneFrame bf = readBoneFrame(ss);
            keyframe.bones[name] = bf;
        }
    }

    anim.keyframes.push_back(keyframe);

    return anim;
}

glm::vec4 getquat(const glm::vec4 &rot)
{
    float rad = rot[3] * M_PI / 180.f / 2.f;

    return glm::vec4(cosf(rad),
            rot[0] * sinf(rad),
            rot[1] * sinf(rad),
            rot[2] * sinf(rad));
}


Keyframe interpolate(const Keyframe &a, const Keyframe &b, int fnum)
{
    Keyframe ret;
    ret.frame = fnum;

    assert(a.frame <= fnum && b.frame >= fnum);

    std::map<std::string, BoneFrame>::const_iterator it;
    for (it = a.bones.begin(); it != a.bones.end(); it++)
    {
        const std::string &name = it->first;
        assert(b.bones.find(name) != b.bones.end());

        const BoneFrame &af = it->second;
        const BoneFrame &bf = b.bones.find(name)->second;

        float fact = static_cast<float>(fnum - a.frame) / (b.frame - a.frame);

        glm::vec4 aquat = getquat(af.rot);
        glm::vec4 bquat = getquat(bf.rot);

        glm::vec4 interquat = fact * bquat + (1 - fact) * aquat;
        interquat /= glm::length(interquat);
        glm::vec4 interrot = glm::vec4(interquat[1], interquat[2], interquat[3],
                2*acos(interquat[0]) / M_PI * 180.f);

        float interlength = fact * bf.length + (1 - fact) * af.length;


        BoneFrame cf;
        cf.rot = interrot;
        cf.length = interlength;
        ret.bones[name] = cf;
    }

    return ret;
}

void setPose(std::map<std::string, Bone*> &skeleton,
        const Animation &anim, int frame)
{
    // Just stick on the last frame, no repeat for now
    if (frame >= anim.numframes)
    {
        setPose(skeleton, anim.keyframes.back().bones);
        return;
    }

    // Find the frames to interpolate
    size_t i;
    for (i = 1; i < anim.keyframes.size(); i++)
    {
        if (anim.keyframes[i].frame > frame)
            break;
    }
    assert(i < anim.keyframes.size());

    Keyframe kf = interpolate(anim.keyframes[i - 1], anim.keyframes[i], frame);

    setPose(skeleton, kf.bones);
}
