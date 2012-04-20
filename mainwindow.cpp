#include <QMenuBar>
#include <QAction>
#include <QFileDialog>
#include <iostream>
#include "mainwindow.h"
#include "glwidget.h"
#include "glTimelineWidget.h"

MainWindow::MainWindow()
{
    glwidget = new GLWidget;

	glTimelineWidget = new glTimelineWidget();

    createActions();
    createMenus();

    setWindowTitle(tr("kiss-skeleton"));
    setMinimumSize(glwidget->minimumSizeHint());
    resize(glwidget->sizeHint());
}

void MainWindow::createActions()
{
    newAct = new QAction(tr("&New"), this);
    newAct->setShortcuts(QKeySequence::New);
    newAct->setStatusTip(tr("Start a new model"));
    connect(newAct, SIGNAL(triggered()), this, SLOT(newFile()));

    openAct = new QAction(tr("&Open..."), this);
    openAct->setShortcuts(QKeySequence::Open);
    openAct->setStatusTip(tr("Open an existing file"));
    connect(openAct, SIGNAL(triggered()), this, SLOT(open()));

    saveAct = new QAction(tr("&Save"), this);
    saveAct->setShortcuts(QKeySequence::Save);
    saveAct->setStatusTip(tr("Save the current file"));
    connect(saveAct, SIGNAL(triggered()), this, SLOT(saveFile()));

    saveAsAct = new QAction(tr("Save &As..."), this);
    saveAsAct->setShortcut(QKeySequence(tr("Ctrl+Shift+S")));
    saveAsAct->setStatusTip(tr("Save the current file with a different name"));
    connect(saveAsAct, SIGNAL(triggered()), this, SLOT(saveFileAs()));

    importModelAct = new QAction(tr("Import .OBJ"), this);
    importModelAct->setShortcut(QKeySequence(tr("Ctrl+i")));
    importModelAct->setStatusTip(tr("Import a mesh file"));
    connect(importModelAct, SIGNAL(triggered()), this, SLOT(importModel()));

    importBonesAct = new QAction(tr("Import .bones"), this);
    importBonesAct->setShortcut(QKeySequence(tr("Ctrl+i")));
    importBonesAct->setStatusTip(tr("Import a skeleton file"));
    connect(importBonesAct, SIGNAL(triggered()), this, SLOT(importBones()));

    exportAct = new QAction(tr("Export..."), this);
    exportAct->setShortcut(QKeySequence(tr("Ctrl+e")));
    exportAct->setStatusTip(tr("Export a resource"));
    //connect(exportAct, SIGNAL(triggered()), this, SLOT(export()));

    closeAct = new QAction(tr("&Close"), this);
    closeAct->setShortcuts(QKeySequence::Close);
    closeAct->setStatusTip(tr("Close the current model"));
    connect(closeAct, SIGNAL(triggered()), this, SLOT(closeFile()));

    exitAct = new QAction(tr("E&xit"), this);
    exitAct->setShortcut(QKeySequence(tr("Ctrl+q")));
    exitAct->setStatusTip(tr("Exit the application"));
    connect(exitAct, SIGNAL(triggered()), this, SLOT(close()));


    autoSkinAct = new QAction(tr("Auto Skin"), this);
    //autoSkinAct->setShortcuts(QKeySequence(tr("Ctrl+")));
    autoSkinAct->setStatusTip(tr("Automatically skin the mesh"));
    connect(autoSkinAct, SIGNAL(triggered()), this, SLOT(autoSkinMesh()));
    
    
    newAnimation = new QAction(tr("&New Animation"), this);
    newAnimation->setShortcut(QKeySequence(tr("Ctrl+Shift+k")));
    newAnimation->setStatusTip(tr("Creates a new animation."));
    connect(newAnimation, SIGNAL(triggered()), this, SLOT(newAnimation()));

    setKeyframe = new QAction(tr("Set &Keyframe"), this);
    setKeyframe->setShortcut(QKeySequence(tr("k")));
    setKeyframe->setStatusTip(tr("Creates a keyframe at the current frame with the current pose"));
    connect(setKeyframe, SIGNAL(triggered()), this, SLOT(setKeyframe()));
    
    delKeyframe = new QAction(tr("De&lete Keyframe"), this);
    delKeyframe->setShortcut(QKeySequence(tr("Ctrl+k")));
    delKeyframe->setStatusTip(tr("Deletes the keyframe at the current frame"));
    connect(delKeyframe, SIGNAL(triggered()), this, SLOT(deleteKeyframe()));
    
    copyPose = new QAction(tr("&Copy Pose"), this);
    copyPose->setShortcut(QKeySequence(tr("Ctrl+c")));
    copyPose->setStatusTip(tr("Copies the current pose"));
    connect(copyPose, SIGNAL(triggered()), this, SLOT(copyPose()));
    
    pastePose = new QAction(tr("&Paste Pose"), this);
    pastePose->setShortcut(QKeySequence(tr("Ctrl+v")));
    pastePose->setStatusTip(tr("Pastes the pose that was previously copied"));
    connect(pastePose, SIGNAL(triggered()), this, SLOT(pastePose()));
    
}

void MainWindow::createMenus()
{
    fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(newAct);
    fileMenu->addAction(openAct);
    fileMenu->addSeparator();
    fileMenu->addAction(saveAct);
    fileMenu->addAction(saveAsAct);
    fileMenu->addSeparator();
    importMenu = fileMenu->addMenu(tr("&Import"));
    fileMenu->addAction(exportAct);
    fileMenu->addSeparator();
    fileMenu->addAction(closeAct);
    fileMenu->addAction(exitAct);

    importMenu->addAction(importBonesAct);
    importMenu->addAction(importModelAct);

    skinningMenu = menuBar()->addMenu(tr("&Skinning"));
    skinningMenu->addAction(autoSkinAct);

    animationMenu = menuBar()->addMenu(tr("&Animation"));
    animationMenu->addAction(newAnimation);
    animationMenu->addAction(setKeyframe);
    animationMenu->addAction(delKeyframe);
    animationMenu->addAction(copyPose);
    animationMenu->addAction(pastePose);
}

void MainWindow::contextMenuEvent(QContextMenuEvent *event)
{
}

void MainWindow::open()
{
    QString filename = QFileDialog::getOpenFileName(this,
            tr("Open GSM"), ".", tr("GSM Files (*.gsm)"));

    if (!filename.isEmpty())
        openFile(filename);
}

void MainWindow::newFile()
{
    closeFile();
}

void MainWindow::openFile(const QString &path)
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

void MainWindow::importModel()
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

void MainWindow::importBones()
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

void MainWindow::setPoseFromFrame(int frame)
{
    if (tdata_->currentAnimation->keyframes.count(frame) > 0)
    {
        skeleton->setPose(tdata_->currentAnimation->keyframes[frame]);
    } 
    else if (tdata_->currentAnimation->keyframes.size() > 1)
    {
        int lastKeyframe = tdata_->currentFrame;
        int nextKeyframe = tdata_->currentFrame;
        while (tdata_->currentAnimation->keyframes.count(lastKeyframe) == 0 && lastKeyframe > 1) 
            lastKeyframe--;
        while (tdata_->currentAnimation->keyframes.count(nextKeyframe) == 0 && nextKeyframe < tdata_->currentAnimation->endFrame)
            nextKeyframe++;
        if (tdata_->currentAnimation->keyframes.count(lastKeyframe) == 0)
            lastKeyframe = nextKeyframe;
        if (tdata_->currentAnimation->keyframes.count(nextKeyframe) == 0)
            nextKeyframe = lastKeyframe;
        
        // The normalized position of the current frame between the keyframes
        float anim = 0.f;
        if (nextKeyframe - lastKeyframe > 0)
            anim = float(tdata_->currentFrame - lastKeyframe) / (nextKeyframe - lastKeyframe);

		assert(tdata_->currentAnimation->keyframes.count(lastKeyframe) && tdata_->currentAnimation->keyframes.count(nextKeyframe));
		const SkeletonPose *last = tdata_->currentAnimation->keyframes[lastKeyframe];
		const SkeletonPose *next = tdata_->currentAnimation->keyframes[nextKeyframe];
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

void MainWindow::saveFile()
{
    std::cout << "saveFile()\n";
    if (currentFile.isEmpty()) return;
    writeGSM(currentFile);
}

void MainWindow::saveFileAs()
{
    std::cout << "saveFileAs()\n";
    QString path = QFileDialog::getSaveFileName(this, tr("Save GSM"),
            currentFile, tr("GSM Files (*.gsm)"));
    if (!path.isEmpty())
        writeGSM(path);
}

void MainWindow::writeGSM(const QString &path)
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

void MainWindow::closeFile()
{
    std::cout << "MainWindow::closeFile()\n";
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

void MainWindow::newAnimation()
{
    NewAnimDialog *animDialog = new NewAnimDialog(this);
    if (animDialog->result() != QDialog::Accepted) return;
    animation* anim = new animation;
    anim->endFrame = animDialog->getAnimLen();
    std::string animName = animDialog->getAnimName().toStdString();
    // Not ok if anim length not positive
    bool ok = anim->endFrame > 0;
    QString errormsg = tr("Invalid length; cannot be negative!");
    // Not ok if another animation has the same name
    for (auto it = tdata_->animations.begin(); it < tdata_->animations.end(); it++)
        if (animName == (*it)->name)
        {
            ok = false;
            errormsg = tr("Animation name already used!");
        }
    if (!ok)
    {
        QMessageBox::information(this, tr("Error!"), errormsg);
        return;
    }
    anim->name = animName; 
    tdata_->animations.push_back(anim);
    tdata_->currentAnimation = anim;
    std::cout << "New animation made: " << anim->name << ", length: " << anim->endFrame << std::endl;
}
void MainWindow::setKeyframe()
{
    if (skeleton == NULL) return;
    tdata_->currentAnimation->keyframes[tdata_->currentFrame] = skeleton->currentPose();
    updateGL();
}

void MainWindow::deleteKeyframe()
{
    tdata_->currentAnimation->keyframes.erase(tdata_->currentFrame);
    updateGL();
}

void MainWindow::copyPose()
{
    if (skeleton == NULL) return;
    delete copiedPose;
    copiedPose = skeleton->currentPose();
    updateGL();
}

void MainWindow::pastePose()
{
    if (skeleton == NULL) return;
    skeleton->setPose(copiedPose);
    updateGL();
}
