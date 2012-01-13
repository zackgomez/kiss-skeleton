#include <GL/glew.h>
#include <GL/glut.h>
#include <math.h>       /* for cos(), sin(), and sqrt() */
#include <iostream>
#include <vector>
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
};

void renderBone(Bone *bone);


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
    
    Bone *head = new Bone("head", 2.f, glm::vec3(0.f, 3.f, 0.f), glm::vec4(0.f, 0.f, 1.f, -90.f), NULL);
    Bone *backh = new Bone("backh", 3.f, glm::vec3(0.f, 0.f, 0.f), glm::vec4(0.f, 0.f, 0.f, 0.f), head);
    head->children.push_back(backh);
    Bone *backl = new Bone("backl", 3.f, glm::vec3(0.f, 0.f, 0.f), glm::vec4(0.f, 0.f, 0.f, 0.f), backh);
    head->children.push_back(backl);

    Bone *rarm = new Bone("rarm", 3.f, glm::vec3(0.f, 0.f, 0.f), glm::vec4(0.f, 0.f, 1.f, 60.f), head);
    Bone *larm = new Bone("larm", 3.f, glm::vec3(0.f, 0.f, 0.f), glm::vec4(0.f, 0.f, 1.f, -60.f), head);
    head->children.push_back(rarm);
    head->children.push_back(larm);

    Bone *rleg = new Bone("rleg", 3.f, glm::vec3(0.f, 0.f, 0.f), glm::vec4(0.f, 0.f, 1.f, 20.f), backl);
    Bone *lleg = new Bone("lleg", 3.f, glm::vec3(0.f, 0.f, 0.f), glm::vec4(0.f, 0.f, 1.f, -20.f), backl);
    backl->children.push_back(rleg);
    backl->children.push_back(lleg);

    skeleton = head;

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

    std::cout << "Rendering bone: " << bone->name << '\n';

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
