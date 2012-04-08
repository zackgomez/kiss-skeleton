#include <GL/glew.h>
#include <QtGui>
#include <iostream>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "glwidget.h"
#include "Arcball.h"
#include "kiss-skeleton.h"

const float GLWidget::ZOOM_SPEED = 2.f;
const float GLWidget::WHEEL_ZOOM_SPEED = 1.1f;
const glm::vec3 GLWidget::ACTIVE_COLOR = glm::vec3(0.8f, 0.4f, 0.2f);
const float GLWidget::AXIS_LENGTH = 0.10f; // ndc
const float GLWidget::CIRCLE_RADIUS= 0.12f; //ndc
const float GLWidget::SCALE_CIRCLE_RADIUS = 0.15f; //ndc
const float GLWidget::SELECT_THRESH = 0.02f;

static void renderCube();
static void renderCircle(const glm::mat4 &transform);
static void renderArc(const glm::vec3 &start, const glm::vec3 &center,
        const glm::vec3 &normal, float deg);
static void renderLine(const glm::mat4 &transform,
        const glm::vec3 &p0, const glm::vec3 &p1);

static glm::vec3 applyMatrix(const glm::mat4 &mat, const glm::vec3 &v, bool homo = true);

static std::ostream& operator<< (std::ostream& os, const glm::vec2 &v);
static std::ostream& operator<< (std::ostream& os, const glm::vec3 &v);
static std::ostream& operator<< (std::ostream& os, const glm::vec4 &v);
static std::ostream& operator<< (std::ostream& os, const glm::quat &v);

GLWidget::GLWidget(QWidget *parent) :
    QGLWidget(parent),
    timelineHeight(0),
    selectedJoint(NULL),
    editMode(ROTATION_MODE),
    localMode(false),
    rotating(false), translating(false), zooming(false)
{
    // XXX hardcoded files
    skeleton = new Skeleton();
    skeleton->readSkeleton("humanoid.bones");
    bindPose = skeleton->currentPose();
    jointNDC.resize(skeleton->getJoints().size());
}

GLWidget::~GLWidget()
{
}

QSize GLWidget::minimumSizeHint() const
{
    return QSize(0, 200);
}

QSize GLWidget::sizeHint() const
{
    return QSize(800, 1000);
}

void GLWidget::initializeGL()
{
    GLenum err = glewInit();
    if (err != GLEW_OK)
    {
        std::cerr << "Unable to initialize GLEW: " << glewGetErrorString(err) << '\n';
        exit(1);
    }

    glClearColor(0.f, 0.f, 0.f, 0.f);
    glEnable(GL_DEPTH_TEST);

    arcball = new Arcball(glm::vec3(0, 0, -30), 20.f, 1.f, 0.1f, 1000.f, 50.f);
}

void GLWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // render
    glViewport(0, timelineHeight, windowWidth, windowHeight - timelineHeight);
    arcball->setAspect((float) windowWidth / (windowHeight - timelineHeight));
    glMatrixMode(GL_PROJECTION);
    glm::mat4 projMatrix = arcball->getProjectionMatrix();
    glLoadMatrixf(glm::value_ptr(projMatrix));
    glMatrixMode(GL_MODELVIEW);
    glm::mat4 viewMatrix = arcball->getViewMatrix();
    glLoadMatrixf(glm::value_ptr(viewMatrix));

    // render grid
    renderEditGrid();
    // Render the joints
    const std::vector<Joint*> joints = skeleton->getJoints();
    for (size_t i = 0; i < joints.size(); i++)
        renderJoint(viewMatrix, joints[i], joints);
}

void GLWidget::resizeGL(int width, int height)
{
    windowWidth = width;
    windowHeight = height;
}

void GLWidget::mousePressEvent(QMouseEvent *event)
{
    glm::vec2 screenCoord = clickToScreenPos(
            event->x(), event->y());

    // Middle button adjusts camera
    if (event->button() == Qt::MidButton)
    {
        glm::vec3 ndc(screenCoord, 0.f);
        arcball->start(ndc);
        dragStart = screenCoord;
        zoomStart = arcball->getZoom();

        // No button is arcball rotation
        if (!event->modifiers())
            rotating = true;
        else if (event->modifiers() == Qt::CTRL)
            zooming = true;
        else if (event->modifiers() == Qt::SHIFT)
            translating = true;
    }
    // Right mouse selects and deselects bones
    else if (event->button() == Qt::RightButton)
    {
        selectedJoint = NULL;

        // Find closest joint
        float nearest = HUGE_VAL;
        for (size_t i = 0; i < jointNDC.size(); i++)
        {
            const glm::vec3 &ndc = jointNDC[i];
            float dist = glm::length(screenCoord - glm::vec2(ndc));
            if (dist < SELECT_THRESH && ndc.z < nearest)
            {
                selectedJoint = skeleton->getJoint(i);
                nearest = ndc.z;
            }
        }
    }

    updateGL();
}

void GLWidget::mouseReleaseEvent(QMouseEvent *event)
{
    // middle mouse controls camera
    if (event->button() == Qt::MidButton)
    {
        rotating = false;
        translating = false;
        zooming = false;
    }
    updateGL();
}

void GLWidget::mouseMoveEvent(QMouseEvent *event)
{
    glm::vec2 screenCoord = clickToScreenPos(
            event->x(), event->y());
    // This means it's outside the main view
    if (screenCoord == glm::vec2(HUGE_VAL, HUGE_VAL))
        // just ignore for now
        return;

    if (rotating)
    {
        glm::vec3 ndc(screenCoord, 0.f);
        arcball->move(ndc);
    }
    else if (zooming)
    {
        glm::vec2 delta = screenCoord - dragStart;
        // only the y coordinate of the drag matters
        float fact = delta.y;
        float newZoom = zoomStart * powf(ZOOM_SPEED, fact);
        arcball->setZoom(newZoom);
    }
    else if (translating)
    {
        glm::vec2 delta = screenCoord - dragStart;
        arcball->translate(-10.f * delta);
    }

    updateGL();
}

void GLWidget::wheelEvent(QWheelEvent *event)
{
    // Adjust zoom in chunks
    float fact = event->delta() < 0 ? WHEEL_ZOOM_SPEED : 1.f/WHEEL_ZOOM_SPEED;
    arcball->setZoom(arcball->getZoom() * fact);

    updateGL();
}

void GLWidget::renderEditGrid() const
{
    glLoadMatrixf(glm::value_ptr(getViewMatrix()));
    const int width = 16;

    glBegin(GL_LINES);
    for (int i = -width/2; i <= width/2; i++)
    {
        glColor3f(0.2f, 0.2f, 0.2f);
        // in zdirection
        if (i == 0) glColor3f(0.4f, 0.f, 0.f);
        glVertex3f(i, 0.f, -width/2);
        glVertex3f(i, 0.f, width/2);
        if (i == 0) glColor3f(0.f, 0.4f, 0.f);
        // in xdirection
        glVertex3f(-width/2, 0.f, i);
        glVertex3f(width/2, 0.f, i);
    }
    glEnd();
}

void GLWidget::renderJoint(const glm::mat4 &transform, const Joint* joint,
        const std::vector<Joint*> joints)
{
    // Draw cube
    glLoadMatrixf(glm::value_ptr(glm::scale(transform * joint->worldTransform,
                    glm::vec3(0.08f))));
    if (joint == selectedJoint)
        glColor3fv(glm::value_ptr(ACTIVE_COLOR));
    else
        glColor3f(1, 1, 1);
    renderCube();

    // Render axis (x,y,z lines) if necessary
    if (editMode == TRANSLATION_MODE && joint == selectedJoint)
    {
        glm::vec3 axisCenter = applyMatrix(joint->worldTransform, glm::vec3(0,0,0));
        renderAxes(transform, axisCenter);
    }
    else if (editMode == ROTATION_MODE && joint == selectedJoint)
    {
        glm::vec3 pos = applyMatrix(joint->worldTransform, glm::vec3(0,0,0));
        renderRotationSphere(transform, pos);
    }
    else if (editMode == SCALE_MODE && joint == selectedJoint)
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
    jointNDC[joint->index] = glm::vec3(jointndc);

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

void GLWidget::renderAxes(const glm::mat4 &transform, const glm::vec3 &worldCoord)
{
    // Render in NDC coordinates, no projection matrix needed
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);

    // ndc coord of axis center
    glm::vec3 ndcCoord = applyMatrix(getProjectionMatrix() * transform, worldCoord);

    glm::mat4 axisTransform = getProjectionMatrix() * transform;
    if (localMode)
        axisTransform = axisTransform * selectedJoint->worldTransform;

    glm::vec3 xdir = applyMatrix(axisTransform, glm::vec3(1,0,0), false);
    glm::vec3 ydir = applyMatrix(axisTransform, glm::vec3(0,1,0), false);
    glm::vec3 zdir = applyMatrix(axisTransform, glm::vec3(0,0,1), false);
    glm::vec3 p0   = glm::vec3(ndcCoord.x, ndcCoord.y, 0.f);
    glm::vec3 zp1  = ndcCoord + 0.5f * AXIS_LENGTH * zdir; zp1.z = 0.f;
    glm::vec3 xp1  = ndcCoord + 0.5f * AXIS_LENGTH * xdir; xp1.z = 0.f;
    glm::vec3 yp1  = ndcCoord + 0.5f * AXIS_LENGTH * ydir; yp1.z = 0.f;

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
    glm::mat4 circleTransform = glm::scale(glm::translate(glm::mat4(1.f), ndcCoord), glm::vec3(CIRCLE_RADIUS));
    glColor3f(1.f, 1.f, 1.f);
    renderCircle(circleTransform);
    // record center ndc coord
    circleNDC = ndcCoord;

    // Fix matrices
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void GLWidget::renderScaleCircle(const glm::mat4 &transform, const glm::vec3 &worldCoord)
{
    // Render in NDC coordinates, no projection matrix needed
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);

    // ndc coord of axis center
    glm::vec3 ndcCoord = applyMatrix(getProjectionMatrix() * transform, worldCoord);

    // render and record an enclosing circle
    glm::mat4 circleTransform = glm::scale(glm::translate(glm::mat4(1.f), ndcCoord), glm::vec3(SCALE_CIRCLE_RADIUS));
    glColor3f(1.f, 0.5f, 0.5f);
    renderCircle(circleTransform);
    // record center ndc coord
    circleNDC = ndcCoord;

    // Fix matrices
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

}

void GLWidget::renderRotationSphere(const glm::mat4 &transform, const glm::vec3 &worldCoord)
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
    renderArc(glm::vec3(CIRCLE_RADIUS, 0, 0)+screenCoord, screenCoord,
            glm::vec3(0,0,-1), 360.f);

    glm::mat4 axisTrans = transform;
    if (localMode)
        axisTrans = axisTrans * selectedJoint->worldTransform;
    glm::vec3 xdir = glm::normalize(applyMatrix(axisTrans, glm::vec3(1,0,0), false));
    glm::vec3 ydir = glm::normalize(applyMatrix(axisTrans, glm::vec3(0,1,0), false));
    glm::vec3 zdir = glm::normalize(applyMatrix(axisTrans, glm::vec3(0,0,1), false));

    glm::vec3 camdir(0,0,1);
    glm::vec3 dir; 
    
    glColor3f(1,0.5f,0.5f);
    dir = glm::normalize(camdir - glm::dot(camdir, xdir) * xdir);
    renderArc(screenCoord + AXIS_LENGTH*dir, screenCoord, xdir, 180.f);

    glColor3f(0.5f,1,0.5f);
    dir = glm::normalize(camdir - glm::dot(camdir, ydir) * ydir);
    renderArc(screenCoord + AXIS_LENGTH*dir, screenCoord, ydir, 180.f);

    glColor3f(0.5f,0.5f,1);
    dir = glm::normalize(camdir - glm::dot(camdir, zdir) * zdir);
    renderArc(screenCoord + AXIS_LENGTH*dir, screenCoord, zdir, 180.f);

    // Fix matrices
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);


    // Record
    axisDir[0] = xdir;
    axisDir[1] = ydir;
    axisDir[2] = zdir;
}


glm::mat4 GLWidget::getProjectionMatrix() const
{
    return arcball->getProjectionMatrix();
}

glm::mat4 GLWidget::getViewMatrix() const
{
    return arcball->getViewMatrix();
}

glm::vec2 GLWidget::clickToScreenPos(int x, int y) const
{
    glm::vec2 screencoord((float)x / windowWidth, (float)y / (windowHeight - timelineHeight));
    screencoord -= glm::vec2(0.5f);
    screencoord *= 2.f;
    screencoord.y = -screencoord.y;

    return screencoord;
}


// ------------------
// - FREE FUNCTIONS -
// ------------------

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

void renderLine(const glm::mat4 &transform, const glm::vec3 &p0, const glm::vec3 &p1)
{
    glLoadMatrixf(glm::value_ptr(transform));
    glBegin(GL_LINES);
    glVertex3fv(glm::value_ptr(p0));
    glVertex3fv(glm::value_ptr(p1));
    glEnd();
}

glm::vec3 applyMatrix(const glm::mat4 &mat, const glm::vec3 &vec, bool homo)
{
    glm::vec4 pt(vec, homo ? 1 : 0);
    pt = mat * pt;
    if (homo) pt /= pt.w;
    return glm::vec3(pt);
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
}

