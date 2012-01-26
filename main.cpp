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
void renderCube();

int windowWidth = 800, windowHeight = 600;

std::map<std::string, Bone*> skeleton;
Animation anim;
int frame = 0;
bool drawCubes = true;
std::string selectedBone = "";
glm::vec3 selectedBonePos;

// the ndc positions of each bone tip cube
std::map<std::string, glm::vec3> bone_ndc;

// Peter's mystical ui controller for arcball transformation and stuff
static UIState *ui;

void redraw(void)
{
    // Now render
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Apply the camera transformation.
    ui->ApplyViewingTransformation();

    // Set the bone pose
    // XXX
    //setPose(skeleton, anim, frame);

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

glm::vec2 getNDC(int x, int y)
{
    // Find the cube they clicked on
    float screenx = static_cast<float>(x) / windowWidth;
    float screeny = static_cast<float>(y) / windowHeight;

    glm::vec2 screen_pos(screenx, screeny);
    screen_pos -= 0.5f; screen_pos *= 2.f;
    screen_pos.y *= -1;

    return screen_pos;
}

glm::mat4 getModelViewProjectionMatrix()
{
    glm::mat4 pmat, mvmat;
    glGetFloatv(GL_PROJECTION_MATRIX, glm::value_ptr(pmat));
    glGetFloatv(GL_MODELVIEW_MATRIX,  glm::value_ptr(mvmat));

    return pmat * mvmat;
}

glm::mat4 getBoneMatrix(const Bone* bone)
{
    // base case, null bone, identity transform
    if (!bone) return glm::mat4(1.f);

    // The transform for this bone
    glm::mat4 transform = glm::translate(glm::mat4(1.f), bone->pos);
    transform = glm::rotate(transform, bone->rot[3], glm::vec3(bone->rot));
    transform = glm::translate(transform, glm::vec3(bone->length, 0.f, 0.f));

    glm::mat4 parentTransform = getBoneMatrix(bone->parent);

    return parentTransform * transform;
}


void rotate(float &x, float &y, float angle){
    float c=cos(angle),s=sin(angle);
    float x0 = x;
    x = x*c - s*y;
    y = x0*s + x0*y;
}

void setBoneTipPosition(Bone *bone, const glm::vec3 &worldPos)
{
    // First get the parent transform, we can't adjust that
    glm::mat4 parentTransform = getBoneMatrix(bone->parent);
    glm::mat4 inverseParentTransform = glm::inverse(parentTransform);
    glm::mat4 curPoseTransform = getBoneMatrix(bone);

    glm::vec4 pt(worldPos, 1.f);
    pt = inverseParentTransform * pt; pt /= pt.w;

    glm::vec4 curpt = curPoseTransform * glm::vec4(0,0,0,1);
    curpt /= curpt.w;

    // vector in direction of point of bone length
    glm::vec3 ptvec = glm::normalize(glm::vec3(pt)) * bone->length;
    std::cout << "---------------------\n";
    std::cout << "worldpos (argument): " << worldPos.x << ' ' << worldPos.y << ' ' << worldPos.z << '\n';
    std::cout << "difference between parent tip and desired tip: " << ptvec.x << ' ' << ptvec.y << ' ' << ptvec.z << '\n';
    std::cout << "difference between current tip and desired tip: " << worldPos.x-curpt.x << ' ' << worldPos.y-curpt.y << ' ' << worldPos.z-curpt.z << '\n';

    glm::vec3 rotvec(1.f, 0.f, 0.f);

    rotate(rotvec.y, rotvec.z, 2); // Replace 4 by whatever you want. This is your free parameter
    rotate(rotvec.x, rotvec.z, atan2(ptvec.z, sqrtf(ptvec.x*ptvec.x + ptvec.y*ptvec.y)));
    rotate(rotvec.x, rotvec.y, atan2(ptvec.y, ptvec.x));
    rotvec = glm::normalize(rotvec);

    std::cout << "rotation vec: " << rotvec.x << ' ' << rotvec.y << ' ' << rotvec.z << '\n';

    // XXX not rotvec!
    float angle = 180.f / M_PI * glm::dot(ptvec, glm::vec3(1.f, 0.f, 0.f));

    std::cout << "angle between tips: " << angle << '\n';

    std::cout << "angle between current rotvecs: " << 180.f / M_PI * acos(glm::dot(rotvec, glm::vec3(bone->rot))) << '\n';

    bone->rot.x = rotvec.x;
    bone->rot.y = rotvec.y;
    bone->rot.z = rotvec.z;
    bone->rot.w = angle;

    // Sanity check
    glm::mat4 sanitymat = glm::translate(glm::mat4(1.f), bone->pos);
    sanitymat = glm::rotate(sanitymat, angle, rotvec);
    glm::vec4 sanityvec = sanitymat * glm::vec4(bone->length, 0.f, 0.f, 1.f);
    sanityvec /= sanityvec.w;

    std::cout << "sanityvec (parent space): " << sanityvec.x << ' ' << sanityvec.y << ' ' << sanityvec.z << '\n';
}

void mouse(int button, int state, int x, int y)
{
    if (button == GLUT_RIGHT_BUTTON && drawCubes)
    {
        if (state == GLUT_DOWN)
        {
            selectedBone = "";

            glm::vec2 screen_pos = getNDC(x, y);
            //std::cout << "Click screen pos: " << screen_pos.x << ' ' << screen_pos.y << '\n';

            std::map<std::string, glm::vec3>::const_iterator it;
            float closestZ = HUGE_VAL;
            for (it = bone_ndc.begin(); it != bone_ndc.end(); it++)
            {
                std::string name = it->first;
                glm::vec3 bonepos = it->second;

                const float select_dist = 0.01f;

                if (glm::length(glm::vec2(bonepos) - screen_pos) < select_dist &&
                    bonepos.z < closestZ)
                {
                    selectedBone = name;
                    closestZ = bonepos.z;
                    selectedBonePos = bonepos;
                }
            }

            if (!selectedBone.empty())
            {
                //glm::vec4 pt(0.f, 0.f, 0.f, 1.f);
                //glm::mat4 fullTransform = getModelViewProjectionMatrix() * getBoneMatrix(skeleton[selectedBone]);
                //pt = fullTransform * pt; pt /= pt.w;
                std::cout << "Selected bone: " << selectedBone << '\n';
                //std::cout << "Click screen pos: " << screen_pos.x << ' ' << screen_pos.y << '\n';
                //std::cout << "Bone ndc from getBoneMatrix: " << pt.x << ' ' << pt.y << ' ' << pt.z << '\n';
            }
        }
        if (state == GLUT_UP)
            selectedBone = "";

        glutPostRedisplay();
    }
    else
    {
        // Just pass it on to the ui controller.
        ui->MouseFunction(button, state, x, y);
    }
}

void motion(int x, int y)
{
    if (!selectedBone.empty())
    {
        glm::vec2 screen_pos = getNDC(x, y);
        glm::mat4 inverseMat = glm::inverse(getModelViewProjectionMatrix());

        glm::vec4 world_pos = inverseMat * glm::vec4(screen_pos, selectedBonePos.z, 1.f);
        world_pos /= world_pos.w;

        setBoneTipPosition(skeleton[selectedBone], glm::vec3(world_pos));

        glutPostRedisplay();

        //std::cout << "world pos: " << world_pos.x << ' ' << world_pos.y << ' ' << world_pos.z << '\n';
    }
    else
    {
        // Just pass it on to the ui controller.
        ui->MotionFunction(x, y);
    }
}

void keyboard(GLubyte key, GLint x, GLint y)
{
    // Quit on ESC
    if (key == 27)
        exit(0);
    if (key == 'd')
        printBone(skeleton["root"]);
    if (key == 'n')
        frame++;
    if (key == 'r')
        frame = 0;
    if (key == 'c')
        drawCubes = !drawCubes;

    // Update display...
    glutPostRedisplay();
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

    // Move to the tip of the bone
    glTranslatef(bone->length, 0, 0);

    // Draw a cube for ease of posing
    if (drawCubes)
    {
        glm::vec4 ndc_coord(0.f, 0.f, 0.f, 1.f);
        ndc_coord = getModelViewProjectionMatrix() * ndc_coord;
        ndc_coord /= ndc_coord.w;

        bone_ndc[bone->name] = glm::vec3(ndc_coord);

        //std::cout << bone->name << " screen coord: " << ndc_coord.x << ' ' << ndc_coord.y << ' ' << ndc_coord.z << ' ' << ndc_coord.w << '\n';

        float cube_scale = bone->length / 10.f;
        if (bone->name == selectedBone)
            glColor3f(0.2, 0.2, 0.8);
        else
            glColor3f(0.5, 0.5, 0.5);
        glPushMatrix();
        glScalef(cube_scale, cube_scale, cube_scale);
        renderCube();
        glPopMatrix();
    }

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

        assert(skeleton.find(name) != skeleton.end());

        Bone *bone = skeleton[name];
        bone->length = bframe.length;
        bone->rot = bframe.rot;
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

GLfloat vertices[] = {
    1,1,1,  -1,1,1,  -1,-1,1,  1,-1,1,        // v0-v1-v2-v3
    1,1,1,  1,-1,1,  1,-1,-1,  1,1,-1,        // v0-v3-v4-v5
    1,1,1,  1,1,-1,  -1,1,-1,  -1,1,1,        // v0-v5-v6-v1
    -1,1,1,  -1,1,-1,  -1,-1,-1,  -1,-1,1,    // v1-v6-v7-v2
    -1,-1,-1,  1,-1,-1,  1,-1,1,  -1,-1,1,    // v7-v4-v3-v2
    1,-1,-1,  -1,-1,-1,  -1,1,-1,  1,1,-1};   // v4-v7-v6-v5


void renderCube()
{
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, vertices);

    glDrawArrays(GL_QUADS, 0, 24);

    glDisableClientState(GL_VERTEX_ARRAY);
}
