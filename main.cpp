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

// constants
static int windowWidth = 800, windowHeight = 600;
static const float selectThresh = 0.02f;
static const float axisLength = 0.5f;
static const int TRANSLATION_MODE = 1, ROTATION_MODE = 2, SCALE_MODE = 3;

// global vars
static glm::mat4 viewMatrix(1.f);
static Skeleton *skeleton;
static UIState *ui; // ui controller
static std::map<std::string, glm::vec3> jointNDC;
static glm::vec3 axisNDC[3]; // x,y,z axis marker endpoints

// UI vars
static std::string selectedJoint;
static bool alterLength = false;
static bool renderJointAxis = true;
static glm::vec2 dragStart;
static bool dragging = false;
static int editMode = TRANSLATION_MODE;

// Functions
glm::vec2 clickToScreenPos(int x, int y);
void setJointPosition(const Joint* joint, const glm::vec3 &ndcCoord);
void setJointRotation(const Joint* joint, const glm::vec3 &ndcCoord);
void setJointScale(const Joint* joint, const glm::vec3 &ndcCoord);
void renderAxes(const glm::mat4 &worldTransform);
void renderRotationSphere(const glm::mat4 &worldTransform);
glm::mat4 getModelviewMatrix();
glm::mat4 getProjectionMatrix();
std::ostream& operator<< (std::ostream& os, const glm::vec3 &v);

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

    viewMatrix = getModelviewMatrix();

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
    // Left button possible starts an edit
    if (button == GLUT_LEFT_BUTTON)
    {
        // If button up, nothing to do...
        if (state == GLUT_UP)
        {
            dragging = false;
            return;
        }
        // If no joint selected, nothing to do
        if (selectedJoint.empty()) return;

        // Begin a drag
        dragging = true;
        dragStart = clickToScreenPos(x, y);

        glm::vec3 clickNDC(dragStart, jointNDC[selectedJoint].z);
        std::cout << "Click: " << clickNDC << '\n';
        std::cout << "Joint: " << jointNDC[selectedJoint] << '\n';
        std::cout << "X axis: " << axisNDC[0] << '\n';
        std::cout << "Y axis: " << axisNDC[1] << '\n';
        std::cout << "Z axis: " << axisNDC[2] << '\n';
    }
    // Right mouse selects and deselects bones
    else if (button == GLUT_RIGHT_BUTTON)
    {
        if (state == GLUT_DOWN)
        {
            selectedJoint = "";
            glm::vec2 screencoord = clickToScreenPos(x, y);

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
    // Middle button rotates camera
    else if (button == GLUT_MIDDLE_BUTTON)
    {
        // Just pass it on to the ui controller.
        ui->MouseFunction(GLUT_LEFT_BUTTON, state, x, y);
    }
    // Mouse wheel (buttons 3 and 4) zooms in an out (translates camera)
    else if (button == 3 || button == 4)
    {
        std::cout << "Mouse wheel...\n";
        // TODO zoom
    }

    glutPostRedisplay();
}

void motion(int x, int y)
{
    if (dragging)
    {
        const Joint* joint = skeleton->getJoint(selectedJoint);
        glm::vec2 screenpos = clickToScreenPos(x, y);
        glm::vec3 ndcCoord(screenpos, jointNDC[selectedJoint].z);

        if (editMode == TRANSLATION_MODE)
            setJointPosition(joint, ndcCoord);
        else if (editMode == ROTATION_MODE)
            setJointRotation(joint, ndcCoord);
        else if (editMode == SCALE_MODE)
            setJointScale(joint, ndcCoord);
        else
            assert(false && "Unknown edit mode");
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
    if (key == 't')
        editMode = TRANSLATION_MODE;
    if (key == 'r')
        editMode = ROTATION_MODE;
    if (key == 's')
        editMode = SCALE_MODE;
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

glm::vec2 clickToScreenPos(int x, int y)
{
    glm::vec2 screencoord((float)x / windowWidth, (float)y / windowHeight);
    screencoord -= glm::vec2(0.5f);
    screencoord *= 2.f;
    screencoord.y = -screencoord.y;

    return screencoord;
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
    newScale = glm::length(mouseParentPos) - length;
    //std::cout << newScale << std::endl;

    JointPose pose;
    pose.rot = joint->rot;
    pose.pos = joint->pos;
    pose.scale = newScale;
    skeleton->setPose(selectedJoint, &pose);
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

    // Render axis (x,y,z lines) if necessary
    if (renderJointAxis && joint->name == selectedJoint)
    {
        renderAxes(transform * joint->worldTransform);
        // Also record the NDC coordinates
        glm::mat4 fullTrans = glm::scale(getProjectionMatrix() * transform * joint->worldTransform,
                glm::vec3(axisLength));
        glm::vec4 p;
        // x
        p = glm::vec4(1, 0, 0, 1);
        p = fullTrans * p;
        p /= p.w;
        axisNDC[0] = glm::vec3(p);
        // y
        p = glm::vec4(0, 1, 0, 1);
        p = fullTrans * p;
        p /= p.w;
        axisNDC[1] = glm::vec3(p);
        // z
        p = glm::vec4(0, 0, 1, 1);
        p = fullTrans * p;
        p /= p.w;
        axisNDC[2] = glm::vec3(p);
    }

    // Record joint NDC coordinates
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
    glm::mat4 mat;
    glGetFloatv(GL_MODELVIEW_MATRIX, glm::value_ptr(mat));

    /*
    std::cout << "Modelview matrix:\n"
        << mat[0][0] << ' ' << mat[0][1] << ' ' << mat[0][2] << ' ' << mat[0][3] << '\n'
        << mat[1][0] << ' ' << mat[1][1] << ' ' << mat[1][2] << ' ' << mat[1][3] << '\n'
        << mat[2][0] << ' ' << mat[2][1] << ' ' << mat[2][2] << ' ' << mat[2][3] << '\n'
        << mat[3][0] << ' ' << mat[3][1] << ' ' << mat[3][2] << ' ' << mat[3][3] << '\n';
        */
    return mat;
}

void renderAxes(const glm::mat4 &transform)
{
    glm::mat4 finalTransform = glm::scale(transform, glm::vec3(axisLength));
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

void renderRotationSphere(const glm::mat4 &transform)
{
    // TODO
}

std::ostream& operator<< (std::ostream& os, const glm::vec3 &v)
{
    os << v.x << ' ' << v.y << ' ' << v.z;
    return os;
}
