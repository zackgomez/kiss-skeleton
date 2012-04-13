#include <GL/glew.h>
#include <QtGui>
#include <iostream>
#include <cassert>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "glwidget.h"
#include "Arcball.h"
#include "kiss-skeleton.h"
#include "libgsm.h"
#include "meshops.h"

const float GLWidget::ZOOM_SPEED = 2.f;
const float GLWidget::WHEEL_ZOOM_SPEED = 1.1f;
const glm::vec3 GLWidget::ACTIVE_COLOR = glm::vec3(0.8f, 0.4f, 0.2f);
const float GLWidget::AXIS_LENGTH = 0.10f; // ndc
const float GLWidget::CIRCLE_RADIUS= 0.12f; //ndc
const float GLWidget::SCALE_CIRCLE_RADIUS = 0.15f; //ndc
const float GLWidget::SELECT_THRESH = 0.02f;
const int GLWidget::NO_MESH_MODE = 0;
const int GLWidget::SKINNING_MODE = 1;
const int GLWidget::POSING_MODE = 2;

static void renderCube();
static void renderCircle(const glm::mat4 &transform);
static void renderArc(const glm::vec3 &start, const glm::vec3 &center,
        const glm::vec3 &normal, float deg);
static void renderLine(const glm::mat4 &transform,
        const glm::vec3 &p0, const glm::vec3 &p1);
static void renderPoints(const glm::mat4 &transform, vert *verts, size_t nverts);
static void renderSelectedPoints(const glm::mat4 &transform, rawmesh *mesh,
        int joint, const glm::vec4 &color);
static void renderVisiblePoints(const glm::mat4 &transform, rawmesh *mesh,
        const glm::vec3 &pt, int selectedVert);
static void renderMesh(const glm::mat4 &transform, const vert_p4t2n3j8 *verts,
        size_t nverts);
static void renderIntersectionMesh(const glm::mat4 &transform, const rawmesh *rmesh,
        const glm::vec3 &a, const glm::vec3 &b);

static float pointLineDist(const glm::vec2 &p1, const glm::vec2 &p2,
        const glm::vec2 &pt);

GLWidget::GLWidget(QWidget *parent) :
    QGLWidget(parent),
    timelineHeight(0),
    selectedJoint(NULL),
    skeletonMode(NO_SKELETON_MODE),
    meshMode(NO_MESH_MODE),
    editMode(ROTATION_MODE),
    localMode(false),
    dragging(false),
    rotating(false), translating(false), zooming(false)
{
    skeleton = NULL;
    bindPose = NULL;
    copiedPose = NULL;

    rmesh = NULL;
    verts = NULL;

    // timeline vars
    tdata_ = new timeline_data();
    tdata_->startFrame = 1;
    tdata_->endFrame = 60;
    tdata_->currentFrame = 1;
    tdisplay_ = new TimelineDisplay(this, tdata_);
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

void GLWidget::newFile()
{
    closeFile();
}

void GLWidget::openFile(const QString &path)
{
    closeFile();

    gsm *f = gsm_open(path.toAscii());
    
    if (!f)
    {
        // TODO display error dialog
        return;
    }

    size_t len;
    char *data = (char *) gsm_mesh_contents(f, len);
    if (data)
    {
        bool skinned = true;
        rmesh = readRawMesh(data, len, skinned);
        verts = createSkinnedVertArray(rmesh, &nverts);
        free(data);
    }

    data = (char *) gsm_bones_contents(f, len);
    if (data)
    {
        skeleton = new Skeleton();
        if (skeleton->readSkeleton(data, len))
        {
            free(skeleton);
            skeleton = NULL;
            QMessageBox::information(this, tr("Error reading GSM File"),
                    tr("Unable to parse bones file."));
        }
        if (skeleton)
        {
            bindPose = skeleton->currentPose();
            jointNDC.resize(skeleton->getJoints().size());
        }
        free(data);
    }

    gsm_close(f);

    skeletonMode = skeleton ? STICK_MODE : NO_SKELETON_MODE;
    meshMode = rmesh ?
        (skeleton ? POSING_MODE : SKINNING_MODE) :
        NO_MESH_MODE;
    
    currentFile = path;

    paintGL();
}

void GLWidget::importModel()
{
    QString filename = QFileDialog::getOpenFileName(this, tr("Import Model"),
            ".", tr("OBJ Files (*.obj)"));

    if (filename.isEmpty()) return;

    bool skinned = true;
    rawmesh *newmesh = loadRawMesh(filename.toAscii(), skinned);
    if (!newmesh)
        QMessageBox::information(this, tr("Unable to import mesh"),
                tr("Couldn't parse mesh file"));

    freeRawMesh(rmesh);
    free(verts);
    rmesh = newmesh;
    verts = createSkinnedVertArray(rmesh, &nverts);

    meshMode = skeleton && skinned ? POSING_MODE : SKINNING_MODE;
}

void GLWidget::importBones()
{
    // TODO use a "set skeleton function"
    QString filename = QFileDialog::getOpenFileName(this, tr("Import Bones"),
            ".", tr("Skeleton Files (*.bones)"));

    if (filename.isEmpty()) return;

    Skeleton *newskel = new Skeleton();
    
    if (newskel->readSkeleton(filename.toStdString()))
    {
        QMessageBox::information(this, tr("Unable to import skeleton"),
                tr("Couldn't parse bones file"));
        delete newskel;
        return;
    }

    delete skeleton;
    skeleton = newskel;
    delete bindPose;
    bindPose = skeleton->currentPose();
    jointNDC.resize(skeleton->getJoints().size());

    meshMode = rmesh ? SKINNING_MODE : NO_MESH_MODE;
    skeletonMode = STICK_MODE;
}

void GLWidget::autoSkinMesh()
{
    if (!rmesh || !skeleton)
        return;

    // TODO display a QProgressDialog here
    autoSkinMeshBest(rmesh, skeleton);
    // regenerate verts
    free(verts);
    verts = createSkinnedVertArray(rmesh, &nverts);

    skeleton->setBindPose();
    freeSkeletonPose(bindPose);
    bindPose = skeleton->currentPose();

    std::cout << "auto skinning complete\n";
}

void GLWidget::saveFile()
{
    std::cout << "saveFile()\n";
    if (currentFile.isEmpty()) return;
    writeGSM(currentFile);
}

void GLWidget::saveFileAs()
{
    std::cout << "saveFileAs()\n";
    QString path = QFileDialog::getSaveFileName(this, tr("Save GSM"),
            currentFile, tr("GSM Files (*.gsm)"));
    if (!path.isEmpty())
        writeGSM(path);
}

void GLWidget::writeGSM(const QString &path)
{
    gsm *gsmf;
    if (!(gsmf = gsm_open(path.toAscii())))
    {
        QMessageBox::information(this, tr("Unable to open gsm file"),
                tr("TODO There should be an error message here..."));
        return;
    }

    if (skeleton)
    {
        std::cout << "writing skeleton\n";
        assert(bindPose);
        FILE *bonef = tmpfile();
        assert(bonef);
        writeSkeleton(bonef, skeleton, bindPose);
        gsm_set_bones(gsmf, bonef);
    }
    if (rmesh)
    {
        std::cout << "writing mesh\n";
        FILE *meshf = tmpfile();
        assert(meshf);
        writeRawMesh(rmesh, meshf);
        gsm_set_mesh(gsmf, meshf);
    }

    gsm_close(gsmf);
}

void GLWidget::closeFile()
{
    std::cout << "GLWidget::closeFile()\n";
    delete skeleton;
    skeleton = NULL;
    freeSkeletonPose(bindPose);
    bindPose = NULL;

    freeRawMesh(rmesh);
    rmesh = NULL;
    free(verts);
    verts = NULL;
    nverts = 0;

    skeletonMode = NO_SKELETON_MODE;
    meshMode = NO_MESH_MODE;

    currentFile.clear();

    paintGL();

    // TODO clear timeline/keyframes
}

void GLWidget::setKeyframe()
{
    if (skeleton == NULL) return;
    tdata_->keyframes[tdata_->currentFrame] = skeleton->currentPose();
    updateGL();
}

void GLWidget::deleteKeyframe()
{
    tdata_->keyframes.erase(tdata_->currentFrame);
    updateGL();
}

void GLWidget::copyPose()
{
    if (skeleton == NULL) return;
    delete copiedPose;
    copiedPose = skeleton->currentPose();
    updateGL();
}

void GLWidget::pastePose()
{
    if (skeleton == NULL) return;
    skeleton->setPose(copiedPose);
    updateGL();
}

void GLWidget::initializeGL()
{
    GLenum err = glewInit();
    if (err != GLEW_OK)
    {
        std::cerr << "Unable to initialize GLEW: "
            << glewGetErrorString(err) << '\n';
        exit(1);
    }

    glClearColor(0.f, 0.f, 0.f, 0.f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shaderProgram = make_program("meshskin.v.glsl", "meshskin.f.glsl");
    if (!shaderProgram)
        QMessageBox::information(this, tr("Error loading resources"),
                tr("Unable to compile skinning shader."));

    arcball = new Arcball(glm::vec3(0, 0, -20), 20.f, 1.f, 0.1f, 1000.f, 50.f);
}

void GLWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // render main view
    glViewport(0, timelineHeight, windowWidth, windowHeight - timelineHeight);
    arcball->setAspect((float) windowWidth / (windowHeight - timelineHeight));
    glMatrixMode(GL_PROJECTION);
    glm::mat4 projMatrix = arcball->getProjectionMatrix();
    glLoadMatrixf(glm::value_ptr(projMatrix));
    glMatrixMode(GL_MODELVIEW);
    glm::mat4 viewMatrix = arcball->getViewMatrix();
    glLoadMatrixf(glm::value_ptr(viewMatrix));

    glEnable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);

    // render grid
    renderEditGrid();

    // Render the joints
    if (skeletonMode != NO_SKELETON_MODE)
    {
        const std::vector<Joint*> joints = skeleton->getJoints();
        for (size_t i = 0; i < joints.size(); i++)
            renderJoint(viewMatrix, joints[i], joints);
    }

    // And the mesh
    if (meshMode == SKINNING_MODE)
    {
        glDisable(GL_DEPTH_TEST);
        glColor4fv(glm::value_ptr(glm::vec4(ACTIVE_COLOR, 0.5f)));
        renderMesh(viewMatrix, verts, nverts);

        glColor3f(1.f, 1.f, 1.f);
        glLineWidth(1);
        renderPoints(viewMatrix, rmesh->verts, rmesh->nverts);


        if (selectedJoint)
        {
            renderSelectedPoints(viewMatrix, rmesh, selectedJoint->index,
                    glm::vec4(0,1,0,1));
            //glColor3f(0,0,1);
            //glm::vec3 jointpos = applyMatrix(selectedJoint->worldTransform, glm::vec3(0, 0, 0));
            //renderVisiblePoints(viewMatrix, rmesh, jointpos, -1);
        }
        glEnable(GL_DEPTH_TEST);
    }
    else if (meshMode == POSING_MODE)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        if (shaderProgram)
            renderSkinnedMesh(viewMatrix, verts, nverts,
                    glm::vec4(0.5f, 0.5f, 0.8f, 0.5f));
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    // render timeline
    tdisplay_->render();
}

void GLWidget::resizeGL(int width, int height)
{
    windowWidth = width;
    windowHeight = height;
    tdisplay_->setViewport(0, 0, windowWidth, timelineHeight);
}

void GLWidget::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Tab && meshMode != NO_MESH_MODE)
    {
        if (meshMode == POSING_MODE)
        {
            meshMode = SKINNING_MODE;
            skeleton->setPose(bindPose);
        }
        else if (meshMode == SKINNING_MODE)
        {
            meshMode = POSING_MODE;
        }
    }
    else if (e->key() == Qt::Key_R)
        editMode = ROTATION_MODE;
    else if (e->key() == Qt::Key_T)
        editMode = TRANSLATION_MODE;
    else if (e->key() == Qt::Key_S)
        editMode = SCALE_MODE;
    else if (e->key() == Qt::Key_Space)
    {
        play = !play;
        if (play)
        {
            animTimer = new QTimer(this);
            connect(animTimer, SIGNAL(timeout()), this, SLOT(update()));
            animTimer->start(1000 / FPS);
        }
        else
        {
            animTimer->stop();
        }
    }
    else
        QWidget::keyPressEvent(e);

    updateGL();
}

void GLWidget::mousePressEvent(QMouseEvent *event)
{
    if (dragging || translating || zooming || rotating)
        return;
    int x = event->x(), y = windowHeight-event->y();
    // Timeline events
    if (tdisplay_->contains(x, y))
        tdisplay_->mousePressEvent(event, x, y);
    // Handle main window events
    else 
    {
        assert(y > timelineHeight);
        glm::vec2 screenCoord = clickToScreenPos(x, y);
        // Left button possibly starts an edit
        if (event->button() == Qt::LeftButton && !dragging)
        {
            // If no joint selected, nothing to do
            if (!selectedJoint) return;

            if (editMode == TRANSLATION_MODE)
                setTranslationVec(screenCoord);
            else if (editMode == ROTATION_MODE)
                setRotationVec(screenCoord);
            else if (editMode == SCALE_MODE)
                setScaleVec(screenCoord);
            else
                assert(false && "unknown mode");
        }
        // Middle button adjusts camera
        else if (event->button() == Qt::MidButton)
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
        // Right mouse selects and deselects joints/points
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
    else if (event->button() == Qt::LeftButton)
    {
        dragging = false;
    }
    updateGL();
}

void GLWidget::mouseMoveEvent(QMouseEvent *event)
{
    int x = event->x(), y = windowHeight - event->y();
    glm::vec2 screenCoord = clickToScreenPos(x, y);
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
    else if (dragging)
    {
        // If no joint selected, nothing to do
        assert(selectedJoint);

        if (editMode == TRANSLATION_MODE)
            setJointPosition(selectedJoint, screenCoord);
        else if (editMode == ROTATION_MODE)
            setJointRotation(selectedJoint, screenCoord);
        else if (editMode == SCALE_MODE)
            setJointScale(selectedJoint, screenCoord);
        else
            assert(false && "Unknown edit mode");
    }
    else
        QWidget::mouseMoveEvent(event);

    updateGL();
}

void GLWidget::wheelEvent(QWheelEvent *event)
{
    // Adjust zoom in chunks
    float fact = event->delta() < 0 ? WHEEL_ZOOM_SPEED : 1.f/WHEEL_ZOOM_SPEED;
    arcball->setZoom(arcball->getZoom() * fact);

    updateGL();
}

void GLWidget::setTranslationVec(const glm::vec2 &clickPos)
{
    // pixels to NDC
    const float axisDistThresh = 5.f / std::max(windowWidth, windowHeight);
    const float circleDistThresh = 8.f / std::max(windowWidth, windowHeight);

    glm::vec3 selJointNDC = jointNDC[selectedJoint->index];

    glm::vec3 clickNDC(clickPos, selJointNDC.z);

    // figure out what the vector is (i.e. what they clicked on, if anything)
    // First see if they clicked too far away
    float dist = glm::length(clickNDC - selJointNDC);
    if (dist > CIRCLE_RADIUS + circleDistThresh)
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
            (dist < CIRCLE_RADIUS - circleDistThresh || dist > CIRCLE_RADIUS + circleDistThresh))
        return;

    // If they choose an axis translation vector, we need to make sure that it's in the
    // correct (parent) space
    if (localMode)
        translationVec = applyMatrix(selectedJoint->worldTransform, translationVec, false);
    glm::mat4 parentTransform = selectedJoint->parent == Skeleton::ROOT_PARENT ?
        glm::mat4(1.f) : skeleton->getJoint(selectedJoint->parent)->worldTransform;
    translationVec = applyMatrix(glm::inverse(parentTransform), translationVec, false);

    // If we got here, then it's a valid drag and translationVec is already set
    std::cout << "Translation drag.  vec: " << translationVec << '\n';
    dragging = true;
    dragStart = clickPos;
    startingPos = selectedJoint->pos;
}

void GLWidget::setRotationVec(const glm::vec2 &clickPos)
{
    const float axisDistThresh = 5.f / glm::max(windowWidth, windowHeight);
    const float arcDistThresh = 0.003f;
    glm::vec3 selJointNDC = jointNDC[selectedJoint->index];
    glm::vec3 clickNDC(clickPos, selJointNDC.z);
    const float r = AXIS_LENGTH;

    // Check if they're outside of the rendered rotation sphere
    float dist = glm::length(clickNDC - selJointNDC);
    if (dist > AXIS_LENGTH + axisDistThresh)
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
    glm::mat4 parentTransform = selectedJoint->parent == Skeleton::ROOT_PARENT ? glm::mat4(1.f) : skeleton->getJoint(selectedJoint->parent)->worldTransform;
    rotationVec = globalVec;
    if (localMode)
        rotationVec = applyMatrix(selectedJoint->worldTransform, rotationVec, false);
    rotationVec = applyMatrix(glm::inverse(parentTransform), rotationVec, false);
    // Save startign rotation as quat
    startingRot = axisAngleToQuat(selectedJoint->rot);

    dragging = true;
    dragStart = clickPos;

    std::cout << "Rotation drag.  axis: " << globalVec << "  vec: " << rotationVec << '\n';
}

void GLWidget::setScaleVec(const glm::vec2 &clickPos)
{
    // pixels to NDC
    const float circleDistThresh = 8.f / std::max(windowWidth, windowHeight);

    glm::vec3 selJointNDC = jointNDC[selectedJoint->index];

    glm::vec3 clickNDC(clickPos, selJointNDC.z);

    // First see if they clicked too far away
    float dist = glm::length(clickNDC - selJointNDC);
    if (dist > SCALE_CIRCLE_RADIUS + circleDistThresh || dist < SCALE_CIRCLE_RADIUS - circleDistThresh)
        return;
    dragging = true;
    dragStart = clickPos;
    startingScale = selectedJoint->scale;
    std::cout << "Scale drag.\n";
}

void GLWidget::setJointPosition(const Joint* joint, const glm::vec2 &dragPos)
{
    if (dragPos == glm::vec2(HUGE_VAL))
        return;
    glm::mat4 parentWorld = joint->parent == Skeleton::ROOT_PARENT ? glm::mat4(1.f) : skeleton->getJoint(joint->parent)->worldTransform;

    // Get the positions of the two mouse positions in parent joint space
    glm::vec3 startParentPos = applyMatrix(glm::inverse(getProjectionMatrix() * getViewMatrix() * parentWorld),
            glm::vec3(dragStart, jointNDC[selectedJoint->index].z));
    glm::vec3 mouseParentPos = applyMatrix(glm::inverse(getProjectionMatrix() * getViewMatrix() * parentWorld),
            glm::vec3(dragPos, jointNDC[selectedJoint->index].z));

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
    skeleton->setPose(selectedJoint->index, &pose);
}

void GLWidget::setJointRotation(const Joint* joint, const glm::vec2 &dragPos)
{
    glm::vec3 ndcCoord(dragPos, jointNDC[selectedJoint->index].z);
    if (dragPos == glm::vec2(HUGE_VAL))
        return;
    if (dragStart == dragPos)
        return;
    glm::vec2 center(jointNDC[selectedJoint->index]);

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

    skeleton->setPose(selectedJoint->index, &newpose);
}

void GLWidget::setJointScale(const Joint* joint, const glm::vec2 &dragPos)
{
    glm::vec2 boneNDC(jointNDC[selectedJoint->index]);
    float newScale = glm::length(dragPos - boneNDC) / SCALE_CIRCLE_RADIUS;
    JointPose pose;
    pose.rot = joint->rot;
    pose.pos = joint->pos;
    pose.scale = newScale;
    skeleton->setPose(selectedJoint->index, &pose);
}

void GLWidget::setPoseFromFrame(int frame)
{
    if (tdata_->keyframes.count(frame) > 0)
    {
        skeleton->setPose(tdata_->keyframes[frame]);
    } 
    else if (tdata_->keyframes.size() > 1)
    {
        int lastKeyframe = tdata_->currentFrame;
        int nextKeyframe = tdata_->currentFrame;
        while (tdata_->keyframes.count(lastKeyframe) == 0 && lastKeyframe > tdata_->startFrame)
            lastKeyframe--;
        while (tdata_->keyframes.count(nextKeyframe) == 0 && nextKeyframe < tdata_->endFrame)
            nextKeyframe++;
        if (tdata_->keyframes.count(lastKeyframe) == 0)
            lastKeyframe = nextKeyframe;
        if (tdata_->keyframes.count(nextKeyframe) == 0)
            nextKeyframe = lastKeyframe;
        
        // The normalized position of the current frame between the keyframes
        float anim = 0.f;
        if (nextKeyframe - lastKeyframe > 0)
            anim = float(tdata_->currentFrame - lastKeyframe) / (nextKeyframe - lastKeyframe);

		assert(tdata_->keyframes.count(lastKeyframe) && tdata_->keyframes.count(nextKeyframe));
		const SkeletonPose *last = tdata_->keyframes[lastKeyframe];
		const SkeletonPose *next = tdata_->keyframes[nextKeyframe];
		assert(last->poses.size() == next->poses.size());

		SkeletonPose* pose = new SkeletonPose;
		pose->poses.resize(last->poses.size());
        for (size_t i = 0; i < pose->poses.size(); i++)
        {
			JointPose *jp = pose->poses[i] = new JointPose;
            glm::vec3 startPos = last->poses[i]->pos;
            glm::vec3 endPos = next->poses[i]->pos;
            jp->pos = startPos + (endPos - startPos) * anim;

            glm::quat startRot = axisAngleToQuat(last->poses[i]->rot);
            glm::quat endRot = axisAngleToQuat(next->poses[i]->rot);
            jp->rot = quatToAxisAngle(glm::normalize((1.f - anim) * startRot + anim * endRot));

            float startScale = last->poses[i]->scale;
            float endScale = next->poses[i]->scale;
            jp->scale = startScale + (endScale - startScale) * anim;
        }

        skeleton->setPose(pose);
		freeSkeletonPose(pose);
    }
}

void GLWidget::setFrame(int frame)
{
    tdata_->currentFrame = frame;
    setPoseFromFrame(frame);
    updateGL();
}

void GLWidget::update()
{
    // TODO Couldn't this be done with a mod?
    if (tdata_->currentFrame == tdata_->endFrame)
        setFrame(tdata_->startFrame);
    else
        setFrame(tdata_->currentFrame + 1);
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

    const glm::vec3 worldPos = applyMatrix(joint->worldTransform, glm::vec3(0,0,0));

    // Render axis (x,y,z lines) if necessary
    if (editMode == TRANSLATION_MODE && joint == selectedJoint)
    {
        renderAxes(transform, worldPos);
    }
    else if (editMode == ROTATION_MODE && joint == selectedJoint)
    {
        renderRotationSphere(transform, worldPos);
    }
    else if (editMode == SCALE_MODE && joint == selectedJoint)
    {
        renderScaleCircle(transform * joint->scale, worldPos);
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

void GLWidget::renderSkinnedMesh(const glm::mat4 &transform, const vert_p4t2n3j8 *verts,
        size_t nverts, const glm::vec4 &color)
{
    const std::vector<Joint*> joints = skeleton->getJoints();
    // First get a list of the necessary transformations
    std::vector<glm::mat4> jointMats(joints.size());
    for (size_t i = 0; i < joints.size(); i++)
    {
        jointMats[i] = joints[i]->worldTransform * joints[i]->inverseBindTransform;

        // for each transformation try to extract translation and rotation
        // TODO turn these into dual quats and use to skin using dual quat blending
        //glm::quat qrot  = glm::quat_cast(jointMats[i]);
        //glm::vec3 trans = applyMatrix(jointMats[i], glm::vec3(0.f));
        //std::cout << "(" << i << ") quat: " << qrot << " trans: " << trans << '\n';
    }
    // XXX this should be a param
    glm::mat4 modelMatrix = glm::mat4(1.f);

    // uniforms
    GLuint projectionUniform  = glGetUniformLocation(shaderProgram->program, "projectionMatrix");
    GLuint viewUniform        = glGetUniformLocation(shaderProgram->program, "viewMatrix");
    GLuint modelUniform       = glGetUniformLocation(shaderProgram->program, "modelMatrix");
    GLuint jointMatrixUniform = glGetUniformLocation(shaderProgram->program, "jointMatrices");
    GLuint colorUniform       = glGetUniformLocation(shaderProgram->program, "color");
    // attribs
    GLuint positionAttrib     = glGetAttribLocation(shaderProgram->program, "position");
    GLuint jointAttrib        = glGetAttribLocation(shaderProgram->program, "joint");
    GLuint weightAttrib       = glGetAttribLocation(shaderProgram->program, "weight");
    GLuint normalAttrib       = glGetAttribLocation(shaderProgram->program, "norm");
    GLuint coordAttrib        = glGetAttribLocation(shaderProgram->program, "texcoord");

    glUseProgram(shaderProgram->program);
    glUniformMatrix4fv(projectionUniform, 1, GL_FALSE, glm::value_ptr(getProjectionMatrix()));
    glUniformMatrix4fv(viewUniform,  1, GL_FALSE, glm::value_ptr(getViewMatrix()));
    glUniformMatrix4fv(modelUniform,  1, GL_FALSE, glm::value_ptr(modelMatrix));
    glUniformMatrix4fv(jointMatrixUniform, jointMats.size(), GL_FALSE, &jointMats.front()[0][0]);
    glUniform4fv(colorUniform, 1, glm::value_ptr(color));

    glEnableVertexAttribArray(positionAttrib);
    glEnableVertexAttribArray(normalAttrib);
    glEnableVertexAttribArray(coordAttrib);
    glEnableVertexAttribArray(jointAttrib);
    glEnableVertexAttribArray(weightAttrib);

    glVertexAttribPointer(positionAttrib, 4, GL_FLOAT, GL_FALSE,
            sizeof(vert_p4t2n3j8), (char*)verts + offsetof(vert_p4t2n3j8, pos));
    glVertexAttribPointer(normalAttrib,   3, GL_FLOAT,  GL_FALSE,
            sizeof(vert_p4t2n3j8), (char*)verts + offsetof(vert_p4t2n3j8, norm));
    glVertexAttribPointer(coordAttrib,    2, GL_FLOAT,  GL_FALSE,
            sizeof(vert_p4t2n3j8), (char*)verts + offsetof(vert_p4t2n3j8, coord));
    glVertexAttribIPointer(jointAttrib,   4, GL_INT,
            sizeof(vert_p4t2n3j8), (char*)verts + offsetof(vert_p4t2n3j8, joints));
    glVertexAttribPointer(weightAttrib,   4, GL_FLOAT,  GL_FALSE,
            sizeof(vert_p4t2n3j8), (char*)verts + offsetof(vert_p4t2n3j8, weights));

    glDrawArrays(GL_TRIANGLES, 0, nverts);

    glDisableVertexAttribArray(positionAttrib);
    glDisableVertexAttribArray(normalAttrib);
    glDisableVertexAttribArray(coordAttrib);
    glDisableVertexAttribArray(jointAttrib);
    glDisableVertexAttribArray(weightAttrib);
    glUseProgram(0);
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
    glm::vec2 screencoord((float)x / windowWidth, (float)(y - timelineHeight) / (windowHeight - timelineHeight));
    screencoord -= glm::vec2(0.5f);
    screencoord *= 2.f;

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

void renderPoints(const glm::mat4 &transform, vert *verts, size_t nverts)
{
    glLoadMatrixf(glm::value_ptr(transform));

    glPointSize(2);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(4, GL_FLOAT, sizeof(vert), verts + offsetof(vert, pos));

    glDrawArrays(GL_POINTS, 0, nverts);
    glDisableClientState(GL_VERTEX_ARRAY);
    glPointSize(1);
}

void renderSelectedPoints(const glm::mat4 &transform, rawmesh *mesh, int joint,
        const glm::vec4 &color)
{
    glLoadMatrixf(glm::value_ptr(transform));
    glDisable(GL_DEPTH_TEST);
    const size_t nverts = mesh->nverts;
    const vert* verts = mesh->verts;
    const int* joints = mesh->joints;
    const float* weights = mesh->weights;

    glPointSize(5);
    glBegin(GL_POINTS);
    for (size_t i = 0; i < nverts; i++)
        for (size_t j = 0; j < 4; j++)
            if (joints[4*i + j] == joint)
            {
                glColor4fv(glm::value_ptr(color * weights[4*i+j]));
                glVertex4fv(verts[i].pos);
            }
    glEnd();
    glPointSize(1);
    glEnable(GL_DEPTH_TEST);
}

void renderVisiblePoints(const glm::mat4 &transform, rawmesh *mesh,
        const glm::vec3 &pt, int selectedVert)
{
    glLoadMatrixf(glm::value_ptr(transform));
    const size_t nverts = mesh->nverts;
    const vert* verts = mesh->verts;
    const int* joints = mesh->joints;
    const float* weights = mesh->weights;

    glColor3f(0, 1, 0);
    glPointSize(10);
    glBegin(GL_POINTS);
    glVertex3fv(glm::value_ptr(pt));
    glEnd();

    glPointSize(5);
    glBegin(GL_POINTS);
    for (size_t i = 0; i < nverts; i++)
    {
        glm::vec3 vert = glm::make_vec3(verts[i].pos);
        int intersectingFace = pointVisibleToPoint(pt, vert, mesh);
        if (intersectingFace == -1)
        {
            //std::cout << "rendering visible point " << vert << '\n';
            glColor3f(1, 0, 1);
            glVertex4fv(verts[i].pos);
        }
        else if (i == selectedVert && intersectingFace != -1)
        {
            std::cout << "rendering intersection tri\n";
            glm::vec3 tri[3];
            tri[0] = glm::make_vec3(mesh->verts[mesh->ffaces[intersectingFace].fverts[0].v].pos);
            tri[1] = glm::make_vec3(mesh->verts[mesh->ffaces[intersectingFace].fverts[1].v].pos);
            tri[2] = glm::make_vec3(mesh->verts[mesh->ffaces[intersectingFace].fverts[2].v].pos);
            glm::vec3 intersectionPt = segIntersectsTriangle(pt, vert, tri);
            std::cout << "vert: " << vert << "  intersection: " << intersectionPt << '\n';
            std::cout << "t: " << (intersectionPt - pt) / (vert - pt) << '\n';
            std::cout << "vert == intersection? " << (vert == intersectionPt) << 
                "  " << intersectionPt - vert << '\n';

            glEnd();
            glColor4f(1, 0, 0, 0.5f);
            glBegin(GL_TRIANGLES);
            glVertex4fv(mesh->verts[mesh->ffaces[intersectingFace].fverts[0].v].pos);
            glVertex4fv(mesh->verts[mesh->ffaces[intersectingFace].fverts[1].v].pos);
            glVertex4fv(mesh->verts[mesh->ffaces[intersectingFace].fverts[2].v].pos);
            glEnd();
            glBegin(GL_POINTS);

        }
    }
    glEnd();
    glPointSize(1);
}

void renderMesh(const glm::mat4 &transform, const vert_p4t2n3j8 *verts,
        size_t nverts)
{
    glLoadMatrixf(glm::value_ptr(transform));

    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(4, GL_FLOAT, sizeof(vert_p4t2n3j8), &verts[0].pos);
    glDrawArrays(GL_TRIANGLES, 0, nverts);
    glDisableClientState(GL_VERTEX_ARRAY);
}

void renderLine(const glm::mat4 &transform, const glm::vec3 &p0, const glm::vec3 &p1)
{
    glLoadMatrixf(glm::value_ptr(transform));
    glBegin(GL_LINES);
    glVertex3fv(glm::value_ptr(p0));
    glVertex3fv(glm::value_ptr(p1));
    glEnd();
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

