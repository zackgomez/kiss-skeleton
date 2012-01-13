#include <GL/glew.h>
#include <GL/glut.h>
#include <math.h>       /* for cos(), sin(), and sqrt() */
#include <iostream>
#include <sstream>
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
    glm::vec4 rot;
    
    Bone *parent;
    std::vector<Bone*> children;

    Bone(const std::string &nm, float l,
            const glm::vec3 &relpos, const glm::vec4 &relrot,
            Bone *parnt) :
        name(nm), length(l), pos(relpos), rot(relrot),
        parent(parnt)
    {
    }

    Bone(const std::string &line);
};

void renderBone(Bone *bone);
void printSkeleton(Bone *skeleton);
Bone* readBone(const std::string &bonestr, Bone *skeleton,
        std::map<std::string, Bone*> &bonemap);


int windowWidth = 800, windowHeight = 600;
Bone *skeleton = NULL;

// Peter's mystical ui controller for arcball transformation and stuff
static UIState *ui;

void redraw(void)
{
    // Update first
    // TODO

    // Now render
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Apply the camera transformation.
    ui->ApplyViewingTransformation();

    glMatrixMode(GL_MODELVIEW);
    renderBone(skeleton);

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
        printSkeleton(skeleton);
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

    char *bonestrs[7] = {
        "head 0 3 0 0 0 1 -90 2 NULL", 
        "backh 0 0 0 0 0 0 0 3 head", 
        "backl 0 0 0 0 0 0 0 3 backh", 
        "rleg 0 0 0 0 0 1 20 3 backl", 
        "lleg 0 0 0 0 0 1 -20 3 backl", 
        "rarm 0 0 0 0 0 1 60 3 head", 
        "larm 0 0 0 0 0 1 -60 3 head", 
    };

    skeleton = NULL;
    std::map<std::string, Bone*> bonemap;
    for (int i = 0; i < 7; i++)
    {
        skeleton = readBone(bonestrs[i], skeleton, bonemap);
    }

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

Bone* readBone(const std::string &bonestr, Bone *skeleton, std::map<std::string, Bone*> &bonemap)
{
    std::stringstream ss(bonestr);
    std::string name, parentname;
    float x, y, z, a, rotx, roty, rotz, length;
    ss >> name >> x >> y >> z >> rotx >> roty >> rotz >> a >> length >> parentname;
    if (!ss)
    {
        std::cerr << "Could not read bone from string: '" << bonestr << "'\n";
        return NULL;
    }

    if (parentname == "NULL")
    {
        assert(!skeleton);
        Bone *newbone = new Bone(name, length, glm::vec3(x, y, z), glm::vec4(rotx, roty, rotz, a), NULL);
        bonemap[name] = newbone;
        return newbone;
    }

    assert(skeleton);
    assert(bonemap.find(parentname) != bonemap.end());
    Bone *parent = bonemap[parentname];

    Bone *newbone = new Bone(name, length, glm::vec3(x, y, z), glm::vec4(rotx, roty, rotz, a), parent);
    parent->children.push_back(newbone);
    bonemap[name] = newbone;
    return skeleton;
}

void printSkeleton(Bone *cur)
{
    if (cur == NULL)
        return;

    std::cout << cur->name << ' ' << cur->pos[0] << ' ' << cur->pos[1] << ' ' << cur->pos[2] << ' '
        << cur->rot[0] << ' ' << cur->rot[1] << ' ' << cur->rot[2] << ' ' << cur->rot[3] << ' '
        << cur->length << ' ' << (cur->parent == NULL ? "NULL" : cur->parent->name) << '\n';

    for (size_t i = 0; i < cur->children.size(); i++)
        printSkeleton(cur->children[i]);
}
