#include <GL/glew.h>
#include <QtGui>
#include <iostream>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include "glwidget.h"
#include "Arcball.h"

const float GLWidget::ZOOM_SPEED = 2.f;
const float GLWidget::WHEEL_ZOOM_SPEED = 1.1f;

static void renderCube();
static std::ostream& operator<< (std::ostream& os, const glm::vec2 &v);
static std::ostream& operator<< (std::ostream& os, const glm::vec3 &v);
static std::ostream& operator<< (std::ostream& os, const glm::vec4 &v);
static std::ostream& operator<< (std::ostream& os, const glm::quat &v);

GLWidget::GLWidget(QWidget *parent) :
    QGLWidget(parent),
    timelineHeight(0),
    rotating(false), translating(false), zooming(false)
{
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

    glViewport(0, 0, windowWidth, windowHeight);
    
    arcball->setAspect((float) windowWidth / (windowHeight - timelineHeight));
    glMatrixMode(GL_PROJECTION);
    glm::mat4 projMatrix = arcball->getProjectionMatrix();
    glLoadMatrixf(glm::value_ptr(projMatrix));
    glMatrixMode(GL_MODELVIEW);
    glm::mat4 viewMatrix = arcball->getViewMatrix();
    glLoadMatrixf(glm::value_ptr(viewMatrix));

    renderEditGrid();
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

