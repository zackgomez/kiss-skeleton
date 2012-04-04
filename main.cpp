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
#include "zgfx.h"

// constants
static int windowWidth = 800, windowHeight = 600;
static const float arcballRadius = 10.f;
static const float selectThresh = 0.02f;
static const float axisLength = 0.10f; // ndc
static const float circleRadius = 0.12f; //ndc
static const float scaleCircleRadius = 0.15f; //ndc
static const int TRANSLATION_MODE = 1, ROTATION_MODE = 2, SCALE_MODE = 3;

// global vars
static Skeleton *skeleton;
static Arcball *arcball;
static rawmesh *mesh;
static std::map<std::string, glm::vec3> jointNDC;
static glm::vec3 axisDir[3]; // x,y,z axis directions
static glm::vec3 axisNDC[3]; // x,y,z axis marker endpoints
static glm::vec3 circleNDC;  // circle ndc coordinate

// UI vars
static std::string selectedJoint;
static glm::vec2 dragStart;
static bool rotating = false;
static bool dragging = false;
static bool zooming  = false;
static int editMode = TRANSLATION_MODE;

// translation mode variables
static glm::vec3 startingPos; // the parent space starting pos of selectedJoint
static glm::vec3 translationVec(0.f);
// rotation mode variables
static glm::vec3 rotationVec;
static glm::quat startingRot;
// scaling mode variables
static float startingScale = 1.f;

// Functions
static glm::vec2 clickToScreenPos(int x, int y);
static glm::vec3 applyMatrix(const glm::mat4 &mat, const glm::vec3 &vec, bool homogenous = true);
static void setTranslationVec(const glm::vec2 &clickPos);
static void setRotationVec(const glm::vec2 &clickPos);
static void setScaleVec(const glm::vec2 &clickPos);
// p1 and p2 determine the line, pt is the point to get distance for
// if the point is not between the line segment, then HUGE_VAL is returned
static float pointLineDist(const glm::vec2 &p1, const glm::vec2 &p2, const glm::vec2 &pt);

static void setJointPosition(const Joint* joint, const glm::vec2 &dragPos);
static void setJointRotation(const Joint* joint, const glm::vec2 &dragPos);
static void setJointScale(const Joint* joint, const glm::vec2 &dragPos);

static void renderCube();
static void renderJoint(const glm::mat4 &viewTransform, const Joint* joint, const std::vector<Joint*> joints);
static void renderAxes(const glm::mat4 &viewTransform, const glm::vec3 &worldCoord);
static void renderLine(const glm::vec4 &transform, const glm::vec3 &p0, const glm::vec3 &p1);
static void renderScaleCircle(const glm::mat4 &viewTransform, const glm::vec3 &worldCoord);
static void renderCircle(const glm::mat4 &worldTransform);
static void renderRotationSphere(const glm::mat4 &worldTransform, const glm::vec3 &worldCoord);
static void renderPoints(const glm::mat4 &transform, vert *verts, size_t nverts);
static void renderRawMesh(const glm::mat4 &transform, rawmesh *mesh);
static glm::mat4 getViewMatrix();
static glm::mat4 getProjectionMatrix();
std::ostream& operator<< (std::ostream& os, const glm::vec2 &v);
std::ostream& operator<< (std::ostream& os, const glm::vec3 &v);
std::ostream& operator<< (std::ostream& os, const glm::vec4 &v);
std::ostream& operator<< (std::ostream& os, const glm::quat &v);

// quaternion functions
static glm::quat axisAngleToQuat(const glm::vec4 &axisAngle); // input vec: (axis, angle (deg))
static glm::vec4 quatToAxisAngle(const glm::quat &q); // output vec (axis, angle (deg))

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

    // Render the joints
    const std::vector<Joint*> joints = skeleton->getJoints();
    for (size_t i = 0; i < joints.size(); i++)
        renderJoint(viewMatrix, joints[i], joints);

    // And the mesh
    if (mesh)
    {
        glColor3f(1.f, 1.f, 1.f);
        renderPoints(viewMatrix, mesh->verts, mesh->nverts);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(0.8f, 0.4f, 0.2f, 0.5f);
        renderRawMesh(viewMatrix, mesh);
        glDisable(GL_BLEND);
    }

    glutSwapBuffers();
}

void reshape(int width, int height)
{
    if (width <= 0 || height <= 0)
        return;

    windowWidth = width;
    windowHeight = height;

    glViewport(0, 0, windowWidth, windowHeight);

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
        if (clickPos == glm::vec2(HUGE_VAL))
            return;
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
            if (glutGetModifiers(GLUT_
            glm::vec2 screenPos = clickToScreenPos(x, y);
            glm::vec3 ndc(screenPos, 0.f);
            arcball->start(ndc);
            rotating = true;
        }
        else
        {
            rotating = false;
            zooming = false;
        }
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
    char *objfile = NULL;
    if (argc == 2)
        objfile = argv[1];

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

    // --------------------
    // START MY SETUP
    // --------------------

    arcball = new Arcball(glm::vec3(0, 0, -20), 20.f, 1.f, 0.1f, 1000.f, 50.f);

    std::string bonefile = "stickman.bones";
    skeleton = new Skeleton();
    skeleton->readSkeleton(bonefile);

    mesh = NULL;
    if (objfile)
        mesh = loadRawMesh(objfile);

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
    const Joint *joint = skeleton->getJoint(selectedJoint);

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

    // Now the vector is either set, or they didn't click on an axis vector
    // Check for circle click by just seeing if they clicked close to the radius
    // if it's not circle click, no drag
    if (translationVec == glm::vec3(0.f) &&
            (dist < circleRadius - circleDistThresh || dist > circleRadius + circleDistThresh))
        return;

    // If they choose an axis translation vector, we need to make sure that it's in the
    // correct (parent) space
    glm::mat4 parentTransform = joint->parent == Skeleton::ROOT_PARENT ?
        glm::mat4(1.f) : skeleton->getJoint(joint->parent)->worldTransform;
    translationVec = applyMatrix(glm::inverse(parentTransform), translationVec, false);

    // If we got here, then it's a valid drag and translationVec is already set
    std::cout << "Translation drag.  vec: " << translationVec << '\n';
    dragging = true;
    dragStart = clickPos;
    startingPos = joint->pos;
}

void setRotationVec(const glm::vec2 &clickPos)
{
    const float axisDistThresh = 5.f / glm::max(windowWidth, windowHeight);
    const float arcDistThresh = 0.003f;
    glm::vec3 selJointNDC = jointNDC[selectedJoint];
    glm::vec3 clickNDC(clickPos, selJointNDC.z);
    const Joint* joint = skeleton->getJoint(selectedJoint);
    const float r = axisLength;

    // Check if they're outside of the rendered rotation sphere
    float dist = glm::length(clickNDC - selJointNDC);
    if (dist > axisLength + axisDistThresh)
        return;

    // Now compute the location of their click on the sphere, radius small
    // Assume they've clicked on the sphere, so just map to front hemisphere
    glm::vec3 sphereLoc = glm::vec3((clickPos - glm::vec2(selJointNDC)), 0.f);
    // Now fill out z coordinate to make length appropriate
    float mag = glm::min(r, glm::length(sphereLoc));
    sphereLoc.z = sqrtf(r*r - mag*mag);
    sphereLoc = glm::normalize(sphereLoc);

    // The global rotation vector
    glm::vec3 globalVec(0.f);

    // Now figure out if any of those lie along the drawn arcs
    // Project the sphere location onto each of the axis planes (for x axis, the y,z plane)
    // Then get the length of that vector, if it's nearly 1, then they are next to the arc
    float xdist = 1.f - glm::length(sphereLoc - glm::dot(sphereLoc, axisDir[0]) * axisDir[0]);
    float ydist = 1.f - glm::length(sphereLoc - glm::dot(sphereLoc, axisDir[1]) * axisDir[1]);
    float zdist = 1.f - glm::length(sphereLoc - glm::dot(sphereLoc, axisDir[2]) * axisDir[2]);
    if (xdist < arcDistThresh)
        globalVec = glm::vec3(1, 0, 0);
    else if (ydist < arcDistThresh)
        globalVec = glm::vec3(0, 1, 0);
    else if (zdist < arcDistThresh)
        globalVec = glm::vec3(0, 0, 1);

    // If no arc selected, no drag initiated
    if (globalVec == glm::vec3(0.f))
        return;

    // Transform the rotation vector into parent space so that the transformation
    // is around the global axes
    glm::mat4 parentTransform = joint->parent == Skeleton::ROOT_PARENT ? glm::mat4(1.f) : skeleton->getJoint(joint->parent)->worldTransform;
    rotationVec = applyMatrix(glm::inverse(parentTransform), globalVec, false);
    // Save startign rotation as quat
    startingRot = axisAngleToQuat(joint->rot);

    dragging = true;
    dragStart = clickPos;

    std::cout << "Rotation drag.  axis: " << globalVec << "  vec: " << rotationVec << '\n';
}

void setScaleVec(const glm::vec2 &clickPos)
{
    // pixels to NDC
    const float circleDistThresh = 8.f / std::max(windowWidth, windowHeight);

    glm::vec3 selJointNDC = jointNDC[selectedJoint];

    glm::vec3 clickNDC(clickPos, selJointNDC.z);

    // First see if they clicked too far away
    float dist = glm::length(clickNDC - selJointNDC);
    if (dist > scaleCircleRadius + circleDistThresh || dist < scaleCircleRadius - circleDistThresh)
        return;
    dragging = true;
    dragStart = clickPos;
    startingScale = skeleton->getJoint(selectedJoint)->scale;
    std::cout << "Scale drag.\n";
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
    
    return glm::length(pt - intersect);
}

void setJointPosition(const Joint* joint, const glm::vec2 &dragPos)
{
    if (dragPos == glm::vec2(HUGE_VAL))
        return;
    glm::mat4 parentWorld = joint->parent == Skeleton::ROOT_PARENT ? glm::mat4(1.f) : skeleton->getJoint(joint->parent)->worldTransform;

    // Get the positions of the two mouse positions in parent joint space
    glm::vec3 startParentPos = applyMatrix(glm::inverse(getProjectionMatrix() * getViewMatrix() * parentWorld),
            glm::vec3(dragStart, jointNDC[selectedJoint].z));
    glm::vec3 mouseParentPos = applyMatrix(glm::inverse(getProjectionMatrix() * getViewMatrix() * parentWorld),
            glm::vec3(dragPos, jointNDC[selectedJoint].z));

    // project on translation vector if necessary
    if (translationVec != glm::vec3(0.f))
    {
        startParentPos = glm::dot(startParentPos, translationVec) * translationVec;
        mouseParentPos = glm::dot(mouseParentPos, translationVec) * translationVec;
    }

    // Delta is trivial to compute now
    glm::vec3 deltaPos = mouseParentPos - startParentPos;

    // Finally, set the joint pose
    JointPose pose;
    pose.rot = joint->rot;
    pose.scale = joint->scale;
    pose.pos = startingPos + deltaPos;
    skeleton->setPose(selectedJoint, &pose);
}

void setJointRotation(const Joint* joint, const glm::vec2 &dragPos)
{
    glm::vec3 ndcCoord(dragPos, jointNDC[selectedJoint].z);
    if (dragPos == glm::vec2(HUGE_VAL))
        return;
    glm::vec2 center(jointNDC[selectedJoint]);

    glm::vec2 starting = glm::normalize(dragStart - center);
    glm::vec2 current  = glm::normalize(dragPos - center);

    float angle = 180.f/M_PI * acosf(glm::dot(starting, current));
    angle *= glm::sign(glm::cross(glm::vec3(starting, 0.f), glm::vec3(current, 0.f)).z);
    glm::quat deltarot = axisAngleToQuat(glm::vec4(rotationVec, angle));

    glm::quat newrot = deltarot * startingRot;

    JointPose newpose;
    newpose.pos = joint->pos;
    newpose.scale = joint->scale;
    newpose.rot = quatToAxisAngle(newrot);

    skeleton->setPose(selectedJoint, &newpose);
}

void setJointScale(const Joint* joint, const glm::vec2 &dragPos)
{
    glm::vec2 boneNDC(jointNDC[selectedJoint]);
    float newScale = glm::length(dragPos - boneNDC) / scaleCircleRadius;
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

void renderJoint(const glm::mat4 &transform, const Joint* joint,
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
    else if (editMode == SCALE_MODE && joint->name == selectedJoint)
    {
        glm::vec3 pos = applyMatrix(joint->worldTransform, glm::vec3(0,0,0));
        renderScaleCircle(transform * joint->scale, pos);
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

void renderScaleCircle(const glm::mat4 &transform, const glm::vec3 &worldCoord)
{
    // Render in NDC coordinates, no projection matrix needed
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);

    // ndc coord of axis center
    glm::vec3 ndcCoord = applyMatrix(getProjectionMatrix() * transform, worldCoord);

    // render and record an enclosing circle
    glm::mat4 circleTransform = glm::scale(glm::translate(glm::mat4(1.f), ndcCoord), glm::vec3(scaleCircleRadius));
    glColor3f(1.f, 0.5f, 0.5f);
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


    // Record
    axisDir[0] = xdir;
    axisDir[1] = ydir;
    axisDir[2] = zdir;
}

void renderPoints(const glm::mat4 &transform, vert *verts, size_t nverts)
{
    glLoadMatrixf(glm::value_ptr(transform));

    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(4, GL_FLOAT, sizeof(vert), verts + offsetof(vert, pos));

    glDrawArrays(GL_POINTS, 0, nverts);
    glDisableClientState(GL_VERTEX_ARRAY);
}

void renderRawMesh(const glm::mat4 &transform, rawmesh *mesh)
{
    glDisable(GL_CULL_FACE);
    glLoadMatrixf(glm::value_ptr(transform));
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(4, GL_FLOAT, sizeof(vert), mesh->verts + offsetof(vert, pos));
    glDrawElements(GL_TRIANGLES, 3*mesh->nfaces, GL_UNSIGNED_INT, mesh->faces);
    glDisableClientState(GL_VERTEX_ARRAY);
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

