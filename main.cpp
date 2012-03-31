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
#include "kiss-skeleton.h"

// global vars
static int windowWidth = 800, windowHeight = 600;
static glm::mat4 viewMatrix(1.f);
static Skeleton *skeleton;
static UIState *ui; // ui controller
static std::string selectedJoint;
static std::map<std::string, glm::vec3> jointNDC;
static const float selectThresh = 0.02f;

static bool alterLength = false;
static bool renderJointAxis = true;

// Functions
void setJointPosition(const Joint* joint, const glm::vec3 &ndcCoord);
void setJointRotation(const Joint* joint, const glm::vec3 &ndcCoord);
void renderAxes(const glm::mat4 &worldTransform);
glm::mat4 getModelviewMatrix();
glm::mat4 getProjectionMatrix();

struct EditBoneRenderer : public BoneRenderer
{
    virtual void operator() (const glm::mat4 &transform, const Joint* b, const std::vector<Joint*> joints);
};

void redraw(void)
{
    // Now render
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Apply the camera transformation.
    ui->ApplyViewingTransformation();

    glGetFloatv(GL_MODELVIEW_MATRIX, glm::value_ptr(viewMatrix));

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    skeleton->render(viewMatrix);

    glutSwapBuffers();
}

void reshape(int width, int height)
{
    windowWidth = width;
    windowHeight = height;
    
    if (width <= 0 || height <= 0)
        return;
    
    ui->WindowX() = width;
    ui->WindowY() = height;
    
    ui->Aspect() = float( width ) / height;
    ui->SetupViewport();
    ui->SetupViewingFrustum();
}

void mouse(int button, int state, int x, int y)
{
    // Right mouse selects and deselects bones
    if (button == GLUT_RIGHT_BUTTON)
    {
        if (state == GLUT_DOWN)
        {
            selectedJoint = "";
            glm::vec2 screencoord((float)x / windowWidth, (float)y / windowHeight);
            screencoord -= glm::vec2(0.5f);
            screencoord *= 2.f;
            screencoord.y = -screencoord.y;

            // Find closest joint
            float nearest = HUGE_VAL;
            for (std::map<std::string, glm::vec3>::const_iterator it = jointNDC.begin(); it != jointNDC.end(); it++)
            {
                float dist = glm::length(screencoord - glm::vec2(it->second));
                if (dist < selectThresh && it->second.z < nearest)
                {
                    selectedJoint = it->first;
                    nearest = it->second.z;
                }
            }
        }
    }
    else
    {
        // Just pass it on to the ui controller.
        ui->MouseFunction(button, state, x, y);
    }

    glutPostRedisplay();
}

void setJointPosition(const Joint* joint, const glm::vec3 &ndcCoord)
{
    float length = glm::length(joint->pos);
    glm::mat4 parentWorld = joint->parent == 255 ? glm::mat4(1.f) : skeleton->getJoint(joint->parent)->worldTransform;

    glm::vec4 mouseParentPos(ndcCoord, 1.f);
    mouseParentPos = glm::inverse(getProjectionMatrix() * getModelviewMatrix() * parentWorld) * mouseParentPos;
    mouseParentPos /= mouseParentPos.w;

    glm::vec3 newPos = glm::vec3(mouseParentPos);
    if (!alterLength && joint->parent != 255)
        newPos = length * glm::normalize(glm::vec3(mouseParentPos));

    JointPose pose;
    pose.rot = joint->rot;
    pose.scale = joint->scale;
    pose.pos = newPos;
    skeleton->setPose(selectedJoint, &pose);
}

void setJointRotation(const Joint* joint, const glm::vec3 &ndcCoord)
{
    glm::vec4 localCoord = glm::inverse(getProjectionMatrix() * getModelviewMatrix() * joint->worldTransform) * glm::vec4(ndcCoord, 1);
    localCoord /= localCoord.w;

    glm::vec3 dir = glm::normalize(glm::vec3(localCoord));

    std::cout << "Dir: " << dir.x << ' ' << dir.y << ' ' << dir.z << '\n';
}

void setJointScale(const Joint* joint, const glm::vec3 &ndcCoord)
{
    float length = glm::length(joint->pos);
    glm::mat4 parentWorld = joint->parent == 255 ? glm::mat4(1.f) : skeleton->getJoint(joint->parent)->worldTransform;

    glm::vec4 mouseParentPos(ndcCoord, 1.f);
    mouseParentPos = glm::inverse(getProjectionMatrix() * getModelviewMatrix() * parentWorld) * mouseParentPos;
    mouseParentPos /= mouseParentPos.w;

    float newScale = 1.f;
    newScale = length - glm::length(mouseParentPos);

    JointPose pose;
    pose.rot = joint->rot;
    pose.pos = joint->pos;
    pose.scale = joint->scale * newScale;
    skeleton->setPose(selectedJoint, &pose);
}


void motion(int x, int y)
{
    if (!selectedJoint.empty())
    {
        const Joint* joint = skeleton->getJoint(selectedJoint);
        glm::vec2 screenpos =
            (glm::vec2((float)x / windowWidth, (float)(windowHeight - y) / windowHeight)
            - 0.5f) * 2.f;
        glm::vec3 ndcCoord(screenpos, jointNDC[selectedJoint].z);

        //setJointPosition(joint, ndcCoord);
        //setJointRotation(joint, ndcCoord);
        setJointScale(joint, ndcCoord);
    }
    else
    {
        // Just pass it on to the ui controller.
        ui->MotionFunction(x, y);
    }

    glutPostRedisplay();
}

void keyboard(GLubyte key, GLint x, GLint y)
{
    // Quit on ESC
    if (key == 27)
        exit(0);
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

    std::string bonefile = "stickman.bones";
    skeleton = new Skeleton();
    skeleton->readSkeleton(bonefile);
    skeleton->setBoneRenderer(new EditBoneRenderer());

    // --------------------
    // END MY SETUP
    // --------------------

    glutMainLoop();
    return 0;             /* ANSI C requires main to return int. */
}

static GLfloat cube_verts[] = {
    1,1,1,    -1,1,1,   -1,-1,1,  1,-1,1,        // v0-v1-v2-v3
    1,1,1,    1,-1,1,   1,-1,-1,  1,1,-1,        // v0-v3-v4-v5
    1,1,1,    1,1,-1,   -1,1,-1,  -1,1,1,        // v0-v5-v6-v1
    -1,1,1,   -1,1,-1,  -1,-1,-1, -1,-1,1,    // v1-v6-v7-v2
    -1,-1,-1, 1,-1,-1,  1,-1,1,   -1,-1,1,    // v7-v4-v3-v2
    1,-1,-1,  -1,-1,-1, -1,1,-1,  1,1,-1};   // v4-v7-v6-v5

static void renderCube()
{
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, cube_verts);

    glDrawArrays(GL_QUADS, 0, 24);

    glDisableClientState(GL_VERTEX_ARRAY);
}

void EditBoneRenderer::operator() (const glm::mat4 &transform, const Joint* joint,
        const std::vector<Joint*> joints)
{
    // Draw cube
    glLoadMatrixf(glm::value_ptr(glm::scale(transform * joint->worldTransform,
                    glm::vec3(0.08f))));
    if (joint->name == selectedJoint)
        glColor3f(0.2f, 0.2f, 0.8f);
    else
        glColor3f(1, 1, 1);
    renderCube();

    if (renderJointAxis && joint->name == selectedJoint)
        renderAxes(transform * joint->worldTransform);

    // Record NDC coordinates
    glm::mat4 ptrans = getProjectionMatrix();
    glm::mat4 fullTransform = ptrans * transform * joint->worldTransform;
    glm::vec4 jointndc(0,0,0,1);
    jointndc = fullTransform * jointndc;
    jointndc /= jointndc.w;
    jointNDC[joint->name] = glm::vec3(jointndc);

    // No "bone" to draw for root
    if (joint->parent == 255)
        return;

    glLoadMatrixf(glm::value_ptr(transform));

    glm::mat4 parentTransform = joints[joint->parent]->worldTransform;
    glm::mat4 boneTransform = joint->worldTransform;

    glm::vec4 start(0, 0, 0, 1), end(0, 0, 0, 1);
    start = parentTransform * start;
    start /= start.w;
    end = boneTransform * end;
    end /= end.w;

    glBegin(GL_LINES);
    glColor3f(1, 0, 0);
    glVertex4fv(&start.x);

    glColor3f(0, 1, 0);
    glVertex4fv(&end.x);
    glEnd();
}

glm::mat4 getProjectionMatrix()
{
    glm::mat4 pmat;
    glGetFloatv(GL_PROJECTION_MATRIX, glm::value_ptr(pmat));
    return pmat;
}
glm::mat4 getModelviewMatrix()
{
    glm::mat4 viewMatrix;
    glGetFloatv(GL_MODELVIEW_MATRIX, glm::value_ptr(viewMatrix));
    return viewMatrix;
}

void renderAxes(const glm::mat4 &transform)
{
    glm::mat4 finalTransform = glm::scale(transform, glm::vec3(0.5f));
    glLoadMatrixf(glm::value_ptr(finalTransform));

    glBegin(GL_LINES);
    glColor3f(1.f, 0.5f, 0.5f);
    glVertex3f(0, 0, 0);
    glVertex3f(1, 0, 0);

    glColor3f(0.5f, 1.f, 0.5f);
    glVertex3f(0, 0, 0);
    glVertex3f(0, 1, 0);

    glColor3f(0.5f, 0.5f, 1.f);
    glVertex3f(0, 0, 0);
    glVertex3f(0, 0, 1);
    glEnd();
}
