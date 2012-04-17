#pragma once
#include <GL/glew.h>
#include <QGLWidget>
#include <QTimer>
#include <QDialog>
#include <QLineEdit>
#include <QGridLayout>
#include <QPushButton>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <map>
#include "Arcball.h"
#include "zgfx.h"
#include "glsubdisplay.h"

class Arcball;
class Skeleton;
struct SkeletonPose;
class Joint;
struct graph;

class GLWidget : public QGLWidget
{
    Q_OBJECT
public:
    GLWidget(QWidget *parent = 0);
    ~GLWidget();

    QSize minimumSizeHint() const;
    QSize sizeHint() const;

public slots:
    void newFile();
// TODO no arg
    void openFile(const QString &path);
    void importModel();
    void importBones();
    void saveFile();
    void saveFileAs();
    void closeFile();
    void autoSkinMesh();

protected:
    void initializeGL();
    void paintGL();
    void resizeGL(int width, int height);
    void keyPressEvent(QKeyEvent *e);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void wheelEvent(QWheelEvent *event);

    // helper functions
    void writeGSM(const QString &path);

private:
    // Data members
    Arcball *arcball;
    Skeleton *skeleton;
    shader *shaderProgram;
    rawmesh *rmesh;
    vert_p4t2n3j8 *verts;
    size_t nverts;
    graph *vertgraph;

    bool renderSelected;

    QString currentFile;

    int windowWidth, windowHeight;
    SkeletonPose *bindPose, *copiedPose;
    const Joint* selectedJoint;

    int skeletonMode, meshMode, editMode;
    bool localMode;

    // graphics -> UI vars
    std::vector<glm::vec3> jointNDC;
    glm::vec3 axisNDC[3];
    glm::vec3 axisDir[3];
    glm::vec3 circleNDC;

    // UI state vars
    bool dragging;
    bool rotating, translating, zooming;
    glm::vec2 dragStart;
    float zoomStart;
    glm::vec3 translationVec, rotationVec;
    glm::vec3 startingPos;
    glm::quat startingRot;
    float startingScale;

    // Skeleton editing functions
    void setTranslationVec(const glm::vec2 &clickPos);
    void setRotationVec(const glm::vec2 &clickPos);
    void setScaleVec(const glm::vec2 &clickPos);
    void setJointPosition(const Joint* joint, const glm::vec2 &dragPos);
    void setJointRotation(const Joint* joint, const glm::vec2 &dragPos);
    void setJointScale(const Joint* joint, const glm::vec2 &dragPos);
    // Timeline functions
    void setPoseFromFrame(int frame);
    void setFrame(int frame);

    // Rendering helpers
    void renderEditGrid() const;
    void renderJoint(const glm::mat4 &transform, const Joint *joint,
            const std::vector<Joint*> joints);
    void renderAxes(const glm::mat4 &transform, const glm::vec3 &center);
    void renderRotationSphere(const glm::mat4 &transform, const glm::vec3 &center);
    void renderScaleCircle(const glm::mat4 &transform, const glm::vec3 &center);
    void renderSkinnedMesh(const glm::mat4 &transform, const vert_p4t2n3j8 *verts,
            size_t nverts, const glm::vec4 &color);

    glm::mat4 getProjectionMatrix() const;
    glm::mat4 getViewMatrix() const;
    glm::vec2 clickToScreenPos(int x, int y) const;

    // Constants
    static const float ZOOM_SPEED;
    static const float WHEEL_ZOOM_SPEED;
    static const glm::vec3 ACTIVE_COLOR;
    static const float AXIS_LENGTH;
    static const float CIRCLE_RADIUS;
    static const float SCALE_CIRCLE_RADIUS;
    static const float SELECT_THRESH;
    static const int TRANSLATION_MODE = 1, ROTATION_MODE = 2, SCALE_MODE = 3;
    static const int NO_MESH_MODE, SKINNING_MODE, POSING_MODE;

    enum { NO_SKELETON_MODE, STICK_MODE };
};

