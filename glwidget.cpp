#include <GL/glew.h>
#include <QtGui>
#include <iostream>
#include <cassert>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "glwidget.h"
#include "Arcball.h"
#include "skeleton.h"
#include "libgsm.h"
#include "meshops.h"

const float GLWidget::ZOOM_SPEED = 2.f;
const float GLWidget::WHEEL_ZOOM_SPEED = 1.1f;
const glm::vec3 GLWidget::ACTIVE_COLOR = glm::vec3(0.8f, 0.4f, 0.2f);
const float GLWidget::AXIS_LENGTH = 0.10f; // ndc
const float GLWidget::CIRCLE_RADIUS= 0.12f; //ndc
const float GLWidget::SCALE_CIRCLE_RADIUS = 0.15f; //ndc
const float GLWidget::SELECT_THRESH = 0.01f;
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
static void renderWeightedPoints(const glm::mat4 &transform, const rawmesh *mesh,
        const graph *g, size_t ji, const glm::vec4 &color);
static void renderMesh(const glm::mat4 &transform, const vert_p4t2n3j8 *verts,
        size_t nverts);

GLWidget::GLWidget(QWidget *parent) :
    QGLWidget(parent),
    renderSelected(true),
    selectedBone(NULL),
    meshMode(NO_MESH_MODE),
    editMode(TRANSLATION_MODE),
    localMode(false),
    dragging(false),
    rotating(false), translating(false), zooming(false)
{
    vertgraph = NULL;
    skeleton = NULL;
    bindPose = NULL;
    copiedPose = NULL;

    selectedObject = OBJ_HEAD;

    rmesh = NULL;
    verts = NULL;
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

    // Create a blank skeleton, with only a root node
    skeleton = new Skeleton();
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
        }
        free(data);
    }

    gsm_close(f);

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

    meshMode = rmesh ? SKINNING_MODE : NO_MESH_MODE;
}

void GLWidget::autoSkinMesh()
{
    if (!rmesh || !skeleton)
        return;

    // TODO display a QProgressDialog here
    graph *newgraph = autoSkinMeshBest(rmesh, skeleton);
    // regenerate verts
    free(verts);
    verts = createSkinnedVertArray(rmesh, &nverts);

    skeleton->setBindPose();
    freeSkeletonPose(bindPose);
    bindPose = skeleton->currentPose();
    free_graph(vertgraph);
    vertgraph = newgraph;

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
        // TODO
        //std::cout << "writing skeleton\n";
        //assert(bindPose);
        //FILE *bonef = tmpfile();
        //assert(bonef);
        //writeSkeleton(bonef, skeleton, bindPose);
        //gsm_set_bones(gsmf, bonef);
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

    meshMode = NO_MESH_MODE;

    currentFile.clear();

    paintGL();

    // TODO clear timeline/keyframes
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
    glViewport(0, 0, windowWidth, windowHeight);
    arcball->setAspect((float) windowWidth / windowHeight);
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
    if (skeleton)
    {
        const std::vector<Bone*> bones = skeleton->getBones();
        for (size_t i = 0; i < bones.size(); i++)
            renderBone(bones[i]);
        // now render and ui on the selectedBone
        if (selectedBone)
        {
            if (editMode == TRANSLATION_MODE)
            {
                // either, head, bone or tip
                glm::vec3 axisPos = glm::vec3(0.f); // default to head
                if (selectedObject == OBJ_BONE)
                    axisPos = 0.5f * selectedBone->tipPos;
                else if (selectedObject == OBJ_TIP)
                    axisPos = selectedBone->tipPos;

                glm::mat4 axisMat = glm::translate(selectedBone->joint->worldTransform,
                        axisPos);
                renderAxes(axisMat);
            }
        }
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
    // Render points ineither mode
    if (selectedBone && rmesh && renderSelected)
    {
        renderSelectedPoints(viewMatrix, rmesh, selectedBone->joint->index,
                glm::vec4(0,1,0,1));
    }
}

void GLWidget::resizeGL(int width, int height)
{
    windowWidth = width;
    windowHeight = height;
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

    updateGL();
}

void GLWidget::mousePressEvent(QMouseEvent *event)
{
    if (dragging || translating || zooming || rotating)
        return;
    int x = event->x(), y = windowHeight-event->y();

    glm::vec2 screenCoord = clickToScreenPos(x, y);
    // Left button possibly starts an edit
    if (event->button() == Qt::LeftButton && !dragging)
    {
        // If no joint selected, nothing to do
        if (!selectedBone) return;

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
    // Right mouse selects and deselects bone
    else if (event->button() == Qt::RightButton)
    {
        selectBone(screenCoord);
    }

    updateGL();
}

void GLWidget::selectBone(const glm::vec2 &screenCoord)
{
    // Constants
    static const float END_SELECT_THRESH = 0.015f;
    static const float BODY_SELECT_THRESH = 0.01f;
    // reset
    selectedBone = NULL;
    selectedObject = OBJ_NONE;

    float bestdist = HUGE_VAL;
    Bone *bestbone = NULL;
    int bestobj = OBJ_NONE;

    const std::vector<Bone *> bones = skeleton->getBones();
    for (size_t i = 0; i < bones.size(); i++)
    {
        Bone* b = bones[i];
        const glm::mat4 fullMat = getProjectionMatrix() * getViewMatrix() * b->joint->worldTransform;
        const glm::vec2 headpos = glm::vec2(applyMatrix(fullMat, glm::vec3(0.f)));
        const glm::vec2 tailpos = glm::vec2(applyMatrix(fullMat, b->tipPos));

        // Calculate the three distances
        // distance from click to line...
        float bonedist = pointLineDist(headpos, tailpos, screenCoord);
        float headdist = glm::length(headpos - screenCoord);
        float taildist = glm::length(tailpos - screenCoord);

        // Prefer the head to tail to body
        if (headdist < END_SELECT_THRESH && headdist < bestdist)
        {
            bestdist = headdist;
            bestbone = b;
            bestobj = OBJ_HEAD;
        }
        else if (taildist < END_SELECT_THRESH && taildist < bestdist)
        {
            bestdist = taildist;
            bestbone = b;
            bestobj = OBJ_TIP;
        }
        else if (bonedist < BODY_SELECT_THRESH && bonedist < bestdist)
        {
            bestdist = bonedist;
            bestbone = b;
            bestobj = OBJ_BONE;
        }
    }

    if (bestbone)
    {
        selectedBone = bestbone;
        selectedObject = bestobj;
    }
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
        assert(selectedBone);

        if (editMode == TRANSLATION_MODE)
            setBoneTranslation(selectedBone, screenCoord);
        else if (editMode == ROTATION_MODE)
            setBoneRotation(selectedBone, screenCoord);
        else if (editMode == SCALE_MODE)
            setBoneScale(selectedBone, screenCoord);
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

    glm::vec3 centerNDC = axisNDC[3];
    glm::vec3 clickNDC(clickPos, centerNDC.z);

    // figure out what the vector is (i.e. what they clicked on, if anything)
    // First see if they clicked too far away
    float dist = glm::length(clickNDC - centerNDC);
    if (dist > CIRCLE_RADIUS + circleDistThresh)
        return;

    translationVec = glm::vec3(0.f);
    // now see if they clicked on any of the axis lines, within margin
    if (pointLineDist(glm::vec2(centerNDC), glm::vec2(axisNDC[0]), glm::vec2(clickNDC)) < axisDistThresh)
        translationVec.x = 1.f;
    else if (pointLineDist(glm::vec2(centerNDC), glm::vec2(axisNDC[1]), glm::vec2(clickNDC)) < axisDistThresh)
        translationVec.y = 1.f;
    else if (pointLineDist(glm::vec2(centerNDC), glm::vec2(axisNDC[2]), glm::vec2(clickNDC)) < axisDistThresh)
        translationVec.z = 1.f;

    // Now the vector is either set, or they didn't click on an axis vector
    // Check for circle click by just seeing if they clicked close to the radius
    // if it's not circle click, no drag
    if (translationVec == glm::vec3(0.f) &&
            (dist < CIRCLE_RADIUS - circleDistThresh || dist > CIRCLE_RADIUS + circleDistThresh))
        return;

    // If they choose an axis translation vector, we need to make sure that it's in the
    // correct (parent) space
    Joint *parentjoint = selectedBone->joint;
    if (localMode)
        translationVec = applyMatrix(parentjoint->worldTransform, translationVec, false);
    glm::mat4 parentTransform = parentjoint->parent == Skeleton::ROOT_PARENT ?
        glm::mat4(1.f) : skeleton->getJoint(parentjoint->parent)->worldTransform;
    translationVec = applyMatrix(glm::inverse(parentTransform), translationVec, false);

    // If we got here, then it's a valid drag and translationVec is already set
    std::cout << "Translation drag.  vec: " << translationVec << '\n';
    dragging = true;
    dragStart = clickPos;
    // starting position of axis in world coords
    startingPos = applyMatrix(glm::inverse(getProjectionMatrix() * getViewMatrix()), centerNDC);
}

void GLWidget::setRotationVec(const glm::vec2 &clickPos)
{
    /*
    const float axisDistThresh = 5.f / glm::max(windowWidth, windowHeight);
    const float arcDistThresh = 0.003f;
    glm::vec3 selJointNDC = boneNDC[selectedBone];
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
    Joint *parentJoint = selectedBone->joint;
    glm::mat4 parentTransform = parentJoint->parent == Skeleton::ROOT_PARENT ? glm::mat4(1.f) : skeleton->getJoint(parentJoint->parent)->worldTransform;
    rotationVec = globalVec;
    if (localMode)
        rotationVec = applyMatrix(parentJoint->worldTransform, rotationVec, false);
    rotationVec = applyMatrix(glm::inverse(parentTransform), rotationVec, false);
    // Save startign rotation as quat
    startingRot = axisAngleToQuat(parentJoint->rot);

    dragging = true;
    dragStart = clickPos;

    std::cout << "Rotation drag.  axis: " << globalVec << "  vec: " << rotationVec << '\n';
    */
}

void GLWidget::setScaleVec(const glm::vec2 &clickPos)
{
    /*
    // pixels to NDC
    const float circleDistThresh = 8.f / std::max(windowWidth, windowHeight);

    glm::vec3 selJointNDC = boneNDC[selectedBone];

    glm::vec3 clickNDC(clickPos, selJointNDC.z);

    // First see if they clicked too far away
    float dist = glm::length(clickNDC - selJointNDC);
    if (dist > SCALE_CIRCLE_RADIUS + circleDistThresh || dist < SCALE_CIRCLE_RADIUS - circleDistThresh)
        return;
    dragging = true;
    dragStart = clickPos;
    startingScale = selectedBone->joint->scale;
    std::cout << "Scale drag.\n";
    */
}

void GLWidget::setBoneTranslation(Bone* bone, const glm::vec2 &dragPos)
{
    if (dragPos == glm::vec2(HUGE_VAL))
        return;
    const glm::mat4 worldMat = getProjectionMatrix() * getViewMatrix();
    const glm::mat4 inverseWorldMat = glm::inverse(worldMat);

    // Get the positions of the two mouse positions in world space
    glm::vec3 startWorldPos = applyMatrix(inverseWorldMat, glm::vec3(dragStart, axisNDC[3].z));
    glm::vec3 mouseWorldPos = applyMatrix(inverseWorldMat, glm::vec3(dragPos, axisNDC[3].z));

    // project on translation vector if necessary
    if (translationVec != glm::vec3(0.f))
    {
        startWorldPos = glm::dot(startWorldPos, translationVec) * translationVec;
        mouseWorldPos = glm::dot(mouseWorldPos, translationVec) * translationVec;
    }

    // Delta is trivial to compute now
    glm::vec3 deltaPos = mouseWorldPos - startWorldPos;
    glm::vec3 finalPos = startingPos + deltaPos;

    // Update correct position
    if (selectedObject == OBJ_HEAD)
        skeleton->setBoneHeadPos(bone, finalPos);
    else if (selectedObject == OBJ_BONE)
        skeleton->translateBone(bone, deltaPos);
    else if (selectedObject == OBJ_TIP)
        skeleton->translateTail(bone, deltaPos);
}

void GLWidget::setBoneRotation(Bone* bone, const glm::vec2 &dragPos)
{
    /*
    glm::vec3 ndcCoord(dragPos, boneNDC[bone].z);
    if (dragPos == glm::vec2(HUGE_VAL))
        return;
    if (dragStart == dragPos)
        return;
    glm::vec2 center(boneNDC[bone]);

    glm::vec2 starting = glm::normalize(dragStart - center);
    glm::vec2 current  = glm::normalize(dragPos - center);

    float angle = 180.f/M_PI * acosf(glm::dot(starting, current));
    angle *= glm::sign(glm::cross(glm::vec3(starting, 0.f), glm::vec3(current, 0.f)).z);
    glm::quat deltarot = axisAngleToQuat(glm::vec4(rotationVec, angle));

    glm::quat newrot = deltarot * startingRot;

    const Joint *parentJoint = bone->joint;
    JointPose newpose;
    newpose.pos = parentJoint->pos;
    newpose.scale = parentJoint->scale;
    newpose.rot = quatToAxisAngle(newrot);

    skeleton->setPose(parentJoint->index, &newpose);
    */
}

void GLWidget::setBoneScale(Bone* bone, const glm::vec2 &dragPos)
{
    /*
    glm::vec2 center(boneNDC[bone]);
    float newScale = glm::length(dragPos - center) / SCALE_CIRCLE_RADIUS;
    const Joint *parentJoint = bone->joint;
    JointPose pose;
    pose.rot = parentJoint->rot;
    pose.pos = parentJoint->pos;
    pose.scale = newScale;
    skeleton->setPose(parentJoint->index, &pose);
    */
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

void GLWidget::renderBone(const Bone *bone)
{
    std::cout << "rendering bone " << bone->name << '\n';
    const Joint *joint = bone->joint;
    const glm::mat4 viewMat = getViewMatrix();
    const glm::mat4 headMat = joint->worldTransform;

    // Set color
    if (bone == selectedBone)
        glColor3fv(glm::value_ptr(ACTIVE_COLOR));
    else
        glColor3f(1, 1, 1);

    // Draw cube for head of bone
    glLoadMatrixf(glm::value_ptr(glm::scale(viewMat * headMat,
                    glm::vec3(0.08f))));
    renderCube();

    // Draw line connected head and tail
    glLoadMatrixf(glm::value_ptr(viewMat * headMat));
    glBegin(GL_LINES);
    glVertex3f(0.f, 0.f, 0.f);
    glVertex3fv(glm::value_ptr(bone->tipPos));
    glEnd();

    // draw tail
    const glm::mat4 tailMat = glm::scale(glm::translate(headMat, bone->tipPos),
            glm::vec3(0.08f));
    glLoadMatrixf(glm::value_ptr(viewMat * tailMat));
    renderCube();
}

void GLWidget::renderAxes(const glm::mat4 &modelMat)
{
    // Render in NDC coordinates, no projection matrix needed
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);

    const glm::mat4 projMat = getProjectionMatrix();
    const glm::mat4 viewMat = getViewMatrix();
    const glm::mat4 fullMat = projMat * viewMat * modelMat;

    // ndc coord of axis center
    glm::vec3 ndcCoord = applyMatrix(fullMat, glm::vec3(0.f));

    glm::mat4 axisTransform = projMat * viewMat;
    if (localMode)
        axisTransform = axisTransform * modelMat;

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
    axisNDC[3] = ndcCoord;

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
        axisTrans = axisTrans * selectedBone->joint->worldTransform;
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
    glm::vec2 screencoord((float)x / windowWidth, (float)y / windowHeight);
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
                glm::vec4 curcolor = glm::vec4(glm::vec3(color), color.a * weights[4*i+j]);
                glColor4fv(glm::value_ptr(color));
                const float BAD_THRESH = 0.05f;
                if (weights[4*i+j] < BAD_THRESH)
                    glColor4f(0.8f, 0.1f, 0.1f, 1.f);
                glVertex4fv(verts[i].pos);
            }
    glEnd();
    glPointSize(1);
    glEnable(GL_DEPTH_TEST);
}

void renderWeightedPoints(const glm::mat4 &transform, const rawmesh *mesh,
        const graph *g, size_t ji, const glm::vec4 &color)
{
    glLoadMatrixf(glm::value_ptr(transform));
    glDisable(GL_DEPTH_TEST);
    const size_t nverts = mesh->nverts;
    const vert* verts = mesh->verts;

    glPointSize(5);
    glBegin(GL_POINTS);
    glColor4fv(glm::value_ptr(color));
    for (size_t i = 0; i < nverts; i++)
    {
        if (g->nodes.find(i)->second->weights[ji] != -HUGE_VAL)
            glVertex4fv(verts[i].pos);
    }
    glEnd();
    glPointSize(1);
    glEnable(GL_DEPTH_TEST);
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

