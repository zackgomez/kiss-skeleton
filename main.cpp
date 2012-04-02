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
#include <glm/gtc/quaternion.hpp>
#include "arcball.h"
#include "kiss-skeleton.h"

// constants
static int windowWidth = 800, windowHeight = 600;
static int windowCoord = 600;
static const float arcballRadius = 10.f;
static const float selectThresh = 0.02f;
static const float axisLength = 0.10f; // ndc
static const float circleRadius = 0.15f; //ndc
static const int TRANSLATION_MODE = 1, ROTATION_MODE = 2, SCALE_MODE = 3;

// global vars
static Skeleton *skeleton;
static Arcball *arcball;
static std::map<std::string, glm::vec3> jointNDC;
static glm::vec3 axisNDC[3]; // x,y,z axis marker endpoints
static glm::vec3 circleNDC;  // circle ndc coordinate

// UI vars
static std::string selectedJoint;
static glm::vec2 dragStart;
static bool rotating = false;
static bool dragging = false;
static int editMode = TRANSLATION_MODE;

// translation mode variables
static glm::vec3 startingPos; // the parent space starting pos of selectedJoint
static glm::vec3 translationVec;
// rotation mode variables
static glm::vec3 rotationVec;
static glm::quat startingRot;
// scaling mode variables

// Functions
glm::vec2 clickToScreenPos(int x, int y);
glm::vec3 applyMatrix(const glm::mat4 &mat, const glm::vec3 &vec, bool homogenous = true);
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
void renderLine(const glm::vec4 &transform, const glm::vec3 &p0, const glm::vec3 &p1);
void renderCircle(const glm::mat4 &worldTransform);
void renderHalfCircle(const glm::mat4 &worldTransform);
void renderRotationSphere(const glm::mat4 &worldTransform, const glm::vec3 &worldCoord);
glm::mat4 getViewMatrix();
glm::mat4 getProjectionMatrix();
std::ostream& operator<< (std::ostream& os, const glm::vec2 &v);
std::ostream& operator<< (std::ostream& os, const glm::vec3 &v);
std::ostream& operator<< (std::ostream& os, const glm::vec4 &v);
std::ostream& operator<< (std::ostream& os, const glm::quat &v);

// quaternion functions
glm::quat axisAngleToQuat(const glm::vec4 &axisAngle); // input vec: (axis, angle (deg))
glm::vec4 quatToAxisAngle(const glm::quat &q); // output vec (axis, angle (deg))

struct EditBoneRenderer : public BoneRenderer
{
    virtual void operator() (const glm::mat4 &transform, const Joint* b, const std::vector<Joint*> joints);
};

void redraw(void)
{
    // Now render
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glm::mat4 projMatrix = arcball->getProjectionMatrix();
    glLoadMatrixf(glm::value_ptr(projMatrix));

    glMatrixMode(GL_MODELVIEW);
    glm::mat4 viewMatrix = arcball->getViewMatrix();
    glLoadMatrixf(glm::value_ptr(viewMatrix));

    skeleton->render(viewMatrix);

    glutSwapBuffers();
}

void reshape(int width, int height)
{
    if (width <= 0 || height <= 0)
        return;

    windowCoord = std::min(width, height);

    windowWidth = width;
    windowHeight = height;

    glViewport(0, 0, windowCoord, windowCoord);

    arcball->setAspect(1.f);
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
        if (ndc != glm::vec3(HUGE_VAL, HUGE_VAL, 0.f))
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

    arcball = new Arcball(glm::vec3(0, 0, -20), 20.f, 1.f, 0.1f, 1000.f, 50.f);

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
    if (x > windowCoord || y > windowCoord)
        return glm::vec2(HUGE_VAL);

    glm::vec2 screencoord((float)(x) / windowCoord, (float)(y - (windowHeight - windowCoord)) / windowCoord);
    screencoord -= glm::vec2(0.5f);
    screencoord *= 2.f;
    screencoord.y = -screencoord.y;

    //std::cout << "in: " << x << ' ' << y << "  coord: " << screencoord << '\n';
    return screencoord;
}

glm::vec3 applyMatrix(const glm::mat4 &mat, const glm::vec3 &vec, bool homo)
{
    glm::vec4 pt(vec, homo ? 1 : 0);
    pt = mat * pt;
    if (homo) pt /= pt.w;
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
    const float circleDistThresh = 8.f / std::max(windowWidth, windowHeight);
    glm::vec3 selJointNDC = jointNDC[selectedJoint];
    glm::vec3 clickNDC(clickPos, selJointNDC.z);
    const Joint* joint = skeleton->getJoint(selectedJoint);

    float dist = glm::length(clickNDC - selJointNDC);
    if (dist > circleRadius + circleDistThresh)
        return;

    rotationVec = applyMatrix(glm::inverse(joint->worldTransform), glm::vec3(1,0,0), false);

    dragging = true;
    dragStart = clickPos;

    startingRot = axisAngleToQuat(joint->rot);

    std::cout << "Rotation drag.  vec: " << rotationVec << '\n';
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
    
    //std::cout << "p1: " << p1 << " p2: " << p2 << " intersect: " << intersect << '\n';

    return glm::length(pt - intersect);
}

void setJointPosition(const Joint* joint, const glm::vec2 &dragPos)
{
    glm::vec3 ndcCoord(dragPos, jointNDC[selectedJoint].z);
    glm::mat4 parentWorld = joint->parent == 255 ? glm::mat4(1.f) : skeleton->getJoint(joint->parent)->worldTransform;

    glm::vec4 mouseParentPos(ndcCoord, 1.f);
    mouseParentPos = glm::inverse(getProjectionMatrix() * getViewMatrix() * parentWorld) * mouseParentPos;
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
    glm::vec2 center(jointNDC[selectedJoint]);

    glm::vec2 starting = glm::normalize(dragStart - center);
    glm::vec2 current  = glm::normalize(dragPos - center);

    float angle = 180.f/M_PI * acosf(glm::dot(starting, current));
    glm::quat deltarot = axisAngleToQuat(glm::vec4(rotationVec, angle));

    glm::quat newrot = deltarot * startingRot;

    std::cout << "angle: " << angle << '\n';
    std::cout << "rotvec: " << rotationVec << '\n';
    std::cout << "rot0: " << startingRot << '\n';
    std::cout << "rotd: " << deltarot << '\n';
    std::cout << "rot1: " << newrot << '\n';
    std::cout << "og  axis angle: " << quatToAxisAngle(startingRot) << '\n';
    std::cout << "del axis angle: " << quatToAxisAngle(deltarot) << '\n';
    std::cout << "new axis angle: " << quatToAxisAngle(newrot) << '\n';
    std::cout << '\n';

    JointPose newpose;
    newpose.pos = joint->pos;
    newpose.scale = joint->scale;
    newpose.rot = quatToAxisAngle(newrot);

    skeleton->setPose(selectedJoint, &newpose);
}

void setJointScale(const Joint* joint, const glm::vec2 &dragPos)
{
    glm::vec3 ndcCoord(dragPos, jointNDC[selectedJoint].z);
    float length = glm::length(joint->pos);
    glm::mat4 parentWorld = joint->parent == 255 ? glm::mat4(1.f) : skeleton->getJoint(joint->parent)->worldTransform;

    glm::vec4 mouseParentPos(ndcCoord, 1.f);
    mouseParentPos = glm::inverse(getProjectionMatrix() * getViewMatrix() * parentWorld) * mouseParentPos;
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
        glColor3f(0.9f, 0.5f, 0.5f);
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
        glm::vec3 pos = applyMatrix(joint->worldTransform, glm::vec3(0,0,0));
        renderRotationSphere(transform, pos);
    }

    // Record joint NDC coordinates
    glm::mat4 ptrans = getProjectionMatrix();
    glm::mat4 fullTransform = ptrans * transform * joint->worldTransform;
    glm::vec4 jointndc(0,0,0,1);
    jointndc = fullTransform * jointndc;
    jointndc /= jointndc.w;
    jointNDC[joint->name] = glm::vec3(jointndc);
    //std::cout << "joint: " << joint->name << " ndc: " << glm::vec3(jointndc) << '\n';

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

glm::mat4 getViewMatrix()
{
    return arcball->getViewMatrix();
}

void renderLine(const glm::mat4 &transform, const glm::vec3 &p0, const glm::vec3 &p1)
{
    glLoadMatrixf(glm::value_ptr(transform));
    glBegin(GL_LINES);
    glVertex3fv(glm::value_ptr(p0));
    glVertex3fv(glm::value_ptr(p1));
    glEnd();
}

void renderAxes(const glm::mat4 &transform, const glm::vec3 &worldCoord)
{
    // Render in NDC coordinates, no projection matrix needed
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);

    // ndc coord of axis center
    glm::vec3 ndcCoord = applyMatrix(getProjectionMatrix() * transform, worldCoord);

    glm::vec3 xdir = applyMatrix(getProjectionMatrix() * transform, glm::vec3(1,0,0), false);
    glm::vec3 ydir = applyMatrix(getProjectionMatrix() * transform, glm::vec3(0,1,0), false);
    glm::vec3 zdir = applyMatrix(getProjectionMatrix() * transform, glm::vec3(0,0,1), false);
    glm::vec3 p0   = glm::vec3(ndcCoord.x, ndcCoord.y, 0.f);
    glm::vec3 zp1  = ndcCoord + 0.5f * axisLength * zdir; zp1.z = 0.f;
    glm::vec3 xp1  = ndcCoord + 0.5f * axisLength * xdir; xp1.z = 0.f;
    glm::vec3 yp1  = ndcCoord + 0.5f * axisLength * ydir; yp1.z = 0.f;

    // render x axis
    glColor3f(1.0f, 0.5f, 0.5f);
    renderLine(glm::mat4(1.f),
            p0,
            xp1);
    // render y axis
    glColor3f(0.5f, 1.0f, 0.5f);
    renderLine(glm::mat4(1.f),
            p0,
            yp1);
    // render z axis
    glColor3f(0.5f, 0.5f, 1.0f);
    renderLine(glm::mat4(1.f),
            p0,
            zp1);
    // axis record NDC coordinates...
    axisNDC[0] = xp1;
    axisNDC[1] = yp1;
    axisNDC[2] = zp1;

    // render and record an enclosing circle
    glm::mat4 circleTransform = glm::scale(glm::translate(glm::mat4(1.f), ndcCoord), glm::vec3(circleRadius));
    glColor3f(1.f, 1.f, 1.f);
    renderCircle(circleTransform);
    // record center ndc coord
    circleNDC = ndcCoord;

    // Fix matrices
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void renderCircle(const glm::mat4 &transform)
{
    const float r = 1.f;
    const int nsegments = 100;

    glLoadMatrixf(glm::value_ptr(transform));

    glBegin(GL_LINE_LOOP);
    for (float theta = 0.f; theta < 2*M_PI; theta += 2*M_PI/nsegments)
        glVertex3f(r*cosf(theta), r*sinf(theta), 0.f);
    glEnd();
}

void renderArc(const glm::vec3 &start, const glm::vec3 &center,
        const glm::vec3 &normal, float deg)
{
    glLoadIdentity();
    const int nsegments = 50;
    const glm::vec3 arm = start - center;

    glBegin(GL_LINE_STRIP);
    for (int i = 0; i < nsegments+1; i++)
    {
        float theta = i * deg/nsegments - deg/2.f;
        glm::mat4 rot = glm::rotate(glm::mat4(1.f), theta, normal);
        glm::vec3 pt = applyMatrix(rot, arm) + center;
        glVertex3fv(glm::value_ptr(pt));
    }
    glEnd();
}

void renderRotationSphere(const glm::mat4 &transform, const glm::vec3 &worldCoord)
{
    // Render in NDC coordinates, no projection matrix needed
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);

    glm::mat4 fullMat = getProjectionMatrix() * transform;

    glm::vec3 ndcCoord = applyMatrix(fullMat, worldCoord);
    glm::vec3 screenCoord = glm::vec3(ndcCoord.x, ndcCoord.y, 0.f);

    // Render a white enclosing circle
    glColor3f(1,1,1);
    renderArc(glm::vec3(circleRadius,0,0)+screenCoord, screenCoord, glm::vec3(0,0,-1), 360.f);

    glm::vec3 xdir = glm::normalize(applyMatrix(transform, glm::vec3(1,0,0), false));
    glm::vec3 ydir = glm::normalize(applyMatrix(transform, glm::vec3(0,1,0), false));
    glm::vec3 zdir = glm::normalize(applyMatrix(transform, glm::vec3(0,0,1), false));

    glm::vec3 camdir(0,0,1);
    glm::vec3 dir; 
    
    glColor3f(1,0.5f,0.5f);
    dir = glm::normalize(camdir - glm::dot(camdir, xdir) * xdir);
    renderArc(screenCoord + axisLength*dir, screenCoord, xdir, 180.f);

    glColor3f(0.5f,1,0.5f);
    dir = glm::normalize(camdir - glm::dot(camdir, ydir) * ydir);
    renderArc(screenCoord + axisLength*dir, screenCoord, ydir, 180.f);

    glColor3f(0.5f,0.5f,1);
    dir = glm::normalize(camdir - glm::dot(camdir, zdir) * zdir);
    renderArc(screenCoord + axisLength*dir, screenCoord, zdir, 180.f);

    // Fix matrices
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
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

std::ostream& operator<< (std::ostream& os, const glm::vec4 &v)
{
    os << v.x << ' ' << v.y << ' ' << v.z << ' ' << v.w;
    return os;
}

std::ostream& operator<< (std::ostream& os, const glm::quat &v)
{
    os << v.x << ' ' << v.y << ' ' << v.z << ' ' << v.w;
    return os;
}

// Quaternion operations follow...
glm::quat axisAngleToQuat(const glm::vec4 &a)
{
    /* 
     * qx = ax * sin(angle/2)
     * qy = ay * sin(angle/2)
     * qz = az * sin(angle/2)
     * qw = cos(angle/2)
     */

    float angle = M_PI/180.f * a.w;
    float sina = sinf(angle/2.f);
    float cosa = cosf(angle/2.f);

    // CAREFUL: w, x, y, z
    return glm::quat(cosa, a.x * sina, a.y * sina, a.z * sina);
}

glm::vec4 quatToAxisAngle(const glm::quat &q)
{
    float angle = 2.f * acosf(q.w);
    float sina2 = sinf(angle/2.f);
    angle *= 180.f/M_PI;

    if (sina2 == 0.f)
        return glm::vec4(0, 0, 1, angle);

    return glm::vec4(
            q.x / sina2,
            q.y / sina2,
            q.z / sina2,
            angle);
};
