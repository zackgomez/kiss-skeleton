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
#include "arcball.h"
#include "kiss-skeleton.h"

// constants
static int windowWidth = 800, windowHeight = 600;
static const float arcballRadius = 10.f;
static const float selectThresh = 0.02f;
static const float axisLength = 0.5f;
static const float circleRadius = 0.3f;
static const int TRANSLATION_MODE = 1, ROTATION_MODE = 2, SCALE_MODE = 3;

// global vars
static glm::mat4 projMatrix(1.f);
static glm::mat4 viewMatrix(1.f);
static Skeleton *skeleton;
static Arcball *arcball;
static std::map<std::string, glm::vec3> jointNDC;
static glm::vec3 axisNDC[3]; // x,y,z axis marker endpoints

// UI vars
static std::string selectedJoint;
static glm::vec2 dragStart;
static bool rotating = false;
static bool dragging = false;
static int editMode = TRANSLATION_MODE;

// translation mode variables
static glm::vec3 startingPos; // the parent space starting pos of selectedJoint
static glm::vec3 translationVec(0.f);
// rotation mode variables
static glm::vec3 rotationVec(0.f);
// scaling mode variables

// Functions
glm::vec2 clickToScreenPos(int x, int y);
glm::vec3 applyMatrix(const glm::mat4 &mat, const glm::vec3 &vec);
void setTranslationVec(const glm::vec2 &clickPos);
void setRotationVec(const glm::vec2 &clickPos);
void setScaleVec(const glm::vec2 &clickPos);
// p1 and p2 determine the line, pt is the point to get distance for
// if the point is not between the line segment, then HUGE_VAL is returned
float pointLineDist(const glm::vec2 &p1, const glm::vec2 &p2, const glm::vec2 &pt);

void setJointPosition(const Joint* joint, const glm::vec2 &dragPos);
void setJointRotation(const Joint* joint, const glm::vec2 &dragPos);
void setJointScale(const Joint* joint, const glm::vec2 &dragPos);

static void renderCube();
void renderAxes(const glm::mat4 &viewTransform, const glm::vec3 &worldCoord);
void renderCircle(const glm::mat4 &worldTransform);
void renderHalfCircle(const glm::mat4 &worldTransform);
void renderRotationSphere(const glm::mat4 &worldTransform);
glm::mat4 getModelviewMatrix();
glm::mat4 getProjectionMatrix();
std::ostream& operator<< (std::ostream& os, const glm::vec2 &v);
std::ostream& operator<< (std::ostream& os, const glm::vec3 &v);

struct EditBoneRenderer : public BoneRenderer
{
    virtual void operator() (const glm::mat4 &transform, const Joint* b, const std::vector<Joint*> joints);
};

void redraw(void)
{
    // Now render
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    viewMatrix = getModelviewMatrix();

    glMatrixMode(GL_PROJECTION);
    projMatrix = arcball->getProjectionMatrix();
    glLoadMatrixf(glm::value_ptr(projMatrix));

    glMatrixMode(GL_MODELVIEW);
    viewMatrix = arcball->getViewMatrix();
    glLoadMatrixf(glm::value_ptr(viewMatrix));

    skeleton->render(viewMatrix);

    glutSwapBuffers();
}

void reshape(int width, int height)
{
    if (width <= 0 || height <= 0)
        return;

    windowWidth = width;
    windowHeight = height;

    glViewport(0, 0, width, height);

    arcball->setAspect(float(width) / height);
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

        glm::vec2 clickPos = clickToScreenPos(x, y);
        if (editMode == TRANSLATION_MODE)
            setTranslationVec(clickPos);
        else if (editMode == ROTATION_MODE)
            setRotationVec(clickPos);
        else if (editMode == SCALE_MODE)
            setScaleVec(clickPos);
        else
            assert(false && "unknown mode");
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
        if (state == GLUT_DOWN)
        {
            glm::vec3 ndc(clickToScreenPos(x, y), 0.f);
            arcball->start(ndc);
            rotating = true;
        }
        else
            rotating = false;
    }
    // Mouse wheel (buttons 3 and 4) zooms in an out (translates camera)
    else if (button == 3 || button == 4)
    {
        const float zoomSpeed = 1.1f;
        float fact = (button == 4) ? zoomSpeed : 1.f/zoomSpeed;
        arcball->setZoom(arcball->getZoom() * fact);
    }

    glutPostRedisplay();
}

void motion(int x, int y)
{
    if (dragging)
    {
        const Joint* joint = skeleton->getJoint(selectedJoint);
        glm::vec2 screenpos = clickToScreenPos(x, y);

        if (editMode == TRANSLATION_MODE)
            setJointPosition(joint, screenpos);
        else if (editMode == ROTATION_MODE)
            setJointRotation(joint, screenpos);
        else if (editMode == SCALE_MODE)
            setJointScale(joint, screenpos);
        else
            assert(false && "Unknown edit mode");
    }
    else if (rotating)
    {
        glm::vec3 ndc(clickToScreenPos(x, y), 0.f);
        arcball->move(ndc);
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

    arcball = new Arcball(glm::vec3(0, 0, -40), 20.f, 1.f, 0.1f, 1000.f, 50.f);

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

glm::vec3 applyMatrix(const glm::mat4 &mat, const glm::vec3 &vec)
{
    glm::vec4 pt(vec, 1);
    pt = mat * pt;
    pt /= pt.w;
    return glm::vec3(pt);
}

void setTranslationVec(const glm::vec2 &clickPos)
{
    // pixels to NDC
    const float axisDistThresh = 5.f / std::max(windowWidth, windowHeight);
    const float circleDistThresh = 8.f / std::max(windowWidth, windowHeight);

    glm::vec3 selJointNDC = jointNDC[selectedJoint];

    glm::vec3 clickNDC(clickPos, selJointNDC.z);

    // figure out what the vector is (i.e. what they clicked on, if anything)
    // First see if they clicked too far away
    float dist = glm::length(clickNDC - selJointNDC);
    if (dist > circleRadius + circleDistThresh)
        return;

    translationVec = glm::vec3(0.f);
    // now see if they clicked on any of the axis lines, within margin
    if (pointLineDist(glm::vec2(selJointNDC), glm::vec2(axisNDC[0]), glm::vec2(clickNDC)) < axisDistThresh)
        translationVec.x = 1.f;
    else if (pointLineDist(glm::vec2(selJointNDC), glm::vec2(axisNDC[1]), glm::vec2(clickNDC)) < axisDistThresh)
        translationVec.y = 1.f;
    else if (pointLineDist(glm::vec2(selJointNDC), glm::vec2(axisNDC[2]), glm::vec2(clickNDC)) < axisDistThresh)
        translationVec.z = 1.f;

    std::cout << "Xdist: " << pointLineDist(glm::vec2(selJointNDC), glm::vec2(axisNDC[0]), glm::vec2(clickNDC))
        << "  Ydist: " << pointLineDist(glm::vec2(selJointNDC), glm::vec2(axisNDC[1]), glm::vec2(clickNDC))
        << "  Zdist: " << pointLineDist(glm::vec2(selJointNDC), glm::vec2(axisNDC[1]), glm::vec2(clickNDC))
        << "  dist:  " << dist << '\n';

    // Now the vector is either set, or they didn't click on an axis vector
    // Check for circle click by just seeing if they clicked close to the radius
    // if it's not circle click, no drag
    if (translationVec == glm::vec3(0.f) &&
            (dist < circleRadius - circleDistThresh || dist > circleRadius + circleDistThresh))
        return;

    // If we got here, then it's a valid drag and translationVec is already set
    std::cout << "Translation drag.  vec: " << translationVec << '\n';
    dragging = true;
    dragStart = clickPos;
    startingPos = skeleton->getJoint(selectedJoint)->pos;
}

void setRotationVec(const glm::vec2 &clickPos)
{
}

void setScaleVec(const glm::vec2 &clickPos)
{
}

float pointLineDist(const glm::vec2 &p1, const glm::vec2 &p2, const glm::vec2 &pt)
{
    // from http://local.wasp.uwa.edu.au/~pbourke/geometry/pointline/
    float l = glm::length(p2 - p1);
    float u = ((pt.x - p1.x) * (p2.x - p1.x) + (pt.y - p1.y) * (p2.y - p1.y)) / (l*l);

    // if not 'on' then line segment, then inf distance
    if (u < 0.f || u > 1.f)
        return HUGE_VAL;

    glm::vec2 intersect = p1 + u * (p2 - p1);
    
    std::cout << "p1: " << p1 << " p2: " << p2 << " intersect: " << intersect << '\n';

    return glm::length(pt - intersect);
}

void setJointPosition(const Joint* joint, const glm::vec2 &dragPos)
{
    glm::vec3 ndcCoord(dragPos, jointNDC[selectedJoint].z);
    glm::mat4 parentWorld = joint->parent == 255 ? glm::mat4(1.f) : skeleton->getJoint(joint->parent)->worldTransform;

    glm::vec4 mouseParentPos(ndcCoord, 1.f);
    mouseParentPos = glm::inverse(getProjectionMatrix() * getModelviewMatrix() * parentWorld) * mouseParentPos;
    mouseParentPos /= mouseParentPos.w;

    glm::vec3 posDelta = glm::vec3(mouseParentPos) - startingPos;
    // Now constrain to translation direction, if requested
    if (translationVec != glm::vec3(0.f))
    {
        // NOTE: Currently this is in local mode
        // Get direction in parent space
        glm::vec4 tmp(translationVec, 1.f);
        glm::vec3 dir = glm::normalize(glm::vec3(tmp));

        // now project onto that vector
        posDelta = glm::dot(posDelta, dir) * dir;
    }

    glm::vec3 newPos = startingPos + posDelta;

    JointPose pose;
    pose.rot = joint->rot;
    pose.scale = joint->scale;
    pose.pos = newPos;
    skeleton->setPose(selectedJoint, &pose);
}

void setJointRotation(const Joint* joint, const glm::vec2 &dragPos)
{
    glm::vec3 ndcCoord(dragPos, jointNDC[selectedJoint].z);
}

void setJointScale(const Joint* joint, const glm::vec2 &dragPos)
{
    glm::vec3 ndcCoord(dragPos, jointNDC[selectedJoint].z);
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
    if (editMode == TRANSLATION_MODE && joint->name == selectedJoint)
    {
        glm::vec3 axisCenter = applyMatrix(joint->worldTransform, glm::vec3(0,0,0));
        renderAxes(transform, axisCenter);
    }
    else if (editMode == ROTATION_MODE && joint->name == selectedJoint)
    {
        renderRotationSphere(transform * joint->worldTransform);
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
    return arcball->getProjectionMatrix();
}

glm::mat4 getModelviewMatrix()
{
    return arcball->getViewMatrix();
}

void renderAxes(const glm::mat4 &transform, const glm::vec3 &worldCoord)
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

    glm::vec4 tmp(0,0,0,1);
    tmp = transform * tmp; tmp /= tmp.w;
    glm::vec3 pos(tmp);

    glColor3f(1.f, 1.f, 1.f);
    glm::vec3 ndcCoord = applyMatrix(getProjectionMatrix() * transform, worldCoord);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);

    renderCircle(glm::scale(glm::translate(glm::mat4(1.f), ndcCoord), glm::vec3(circleRadius)));

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    // TODO record NDC coordinates...
}

void renderCircle(const glm::mat4 &transform)
{
    const float r = 0.5f;
    const int nsegments = 40;

    glLoadMatrixf(glm::value_ptr(transform));

    glBegin(GL_LINE_LOOP);
    for (float theta = 0.f; theta < 2*M_PI; theta += 2*M_PI/nsegments)
        glVertex3f(r*cosf(theta), r*sinf(theta), 0.f);
    glEnd();
}

void renderHalfCircle(const glm::mat4 &transform)
{
    const float r = 0.5f;
    const int nsegments = 20;

    glLoadMatrixf(glm::value_ptr(transform));

    glBegin(GL_LINE_STRIP);
    for (int i = 0; i < nsegments+1; i++)
    {
        float theta = i * M_PI/nsegments;
        glVertex3f(r*cosf(theta), r*sinf(theta), 0.f);
    }
    glEnd();
}

void renderRotationSphere(const glm::mat4 &transform)
{
    glm::vec4 tmp(0,0,0,1);
    tmp = transform * tmp; tmp /= tmp.w;
    glm::vec3 viewPos(tmp);
    glm::mat4 sphereTransform = glm::scale(
            glm::translate(glm::mat4(1.f), viewPos),
            glm::vec3(3.f));

    glm::mat4 worldTransform = skeleton->getJoint(selectedJoint)->worldTransform;
    glm::mat4 viewTransform = transform * glm::inverse(worldTransform);
    tmp = glm::vec4(0,0,0,1);
    tmp = worldTransform * tmp; tmp /= tmp.w;
    glm::vec3 worldPos(tmp);

    glm::mat4 arcTransform;
    // global arc transform
    arcTransform = viewTransform *
        glm::scale(glm::translate(glm::mat4(1.f), worldPos), glm::vec3(2.f));


    glLineWidth(2);
    // Render a white enclosing circle
    glColor3f(1.f, 1.f, 1.f);
    renderCircle(sphereTransform);

    // TODO rotate arcs to always face the camera as much as possible
    // vector pointing at camera in NDC
    //tmp = glm::vec4(0,0,-1,0);
    //tmp = glm::inverse(getProjectionMatrix() * arcTransform) * tmp;
    //glm::vec3 cameraDir = glm::normalize(glm::vec3(tmp));
    //float angle = 180.f / M_PI * acosf(glm::dot(cameraDir, glm::vec3(0,1,0)));
    //if (cameraDir.z < 0) angle = -angle;
    //std::cout << "camera dir: " << cameraDir << '\n';
    //std::cout << "angle: " << angle << '\n';

    // X handle
    glColor3f(1.0f, 0.5f, 0.5f);
    renderHalfCircle(glm::rotate(
                glm::rotate(arcTransform, 0.f, glm::vec3(1,0,0)),
                90.f, glm::vec3(0,1,0)));
    // Y handle
    glColor3f(0.5f, 1.0f, 0.5f);
    renderHalfCircle(glm::rotate(
                glm::rotate(arcTransform, 0.f, glm::vec3(0,1,0)),
                90.f, glm::vec3(1,0,0)));
    // Z handle
    glColor3f(0.5f, 0.5f, 1.0f);
    renderHalfCircle(glm::rotate(
                glm::rotate(arcTransform, 0.f, glm::vec3(0,1,0)),
                90.f, glm::vec3(0,0,1)));
    glLineWidth(1);
}

std::ostream& operator<< (std::ostream& os, const glm::vec2 &v)
{
    os << v.x << ' ' << v.y;
    return os;
}

std::ostream& operator<< (std::ostream& os, const glm::vec3 &v)
{
    os << v.x << ' ' << v.y << ' ' << v.z;
    return os;
}
