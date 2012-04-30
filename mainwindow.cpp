#include <QMenuBar>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <iostream>
#include "mainwindow.h"
#include "glwidget.h"
#include "skeleton.h"
#include "libgsm.h"

MainWindow::MainWindow()
{
    cdata = new CharacterData;
    cdata->skeleton = NULL;
    cdata->rmesh = NULL;
    glwidget = new GLWidget(this, cdata);
    setCentralWidget(glwidget);
    glwidget->setFocus();

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
    connect(openAct, SIGNAL(triggered()), this, SLOT(openFile()));

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
    connect(autoSkinAct, SIGNAL(triggered()), glwidget, SLOT(autoSkinMesh()));
    
    
    newAnimationAct = new QAction(tr("&New Animation"), this);
    newAnimationAct->setShortcut(QKeySequence(tr("Ctrl+Shift+k")));
    newAnimationAct->setStatusTip(tr("Creates a new animation."));
    //connect(newAnimationAct, SIGNAL(triggered()), glwidget, SLOT(newAnimation()));

    setKeyframeAct = new QAction(tr("Set &Keyframe"), this);
    setKeyframeAct->setShortcut(QKeySequence(tr("k")));
    setKeyframeAct->setStatusTip(tr("Creates a keyframe at the current frame with the current pose"));
    //connect(setKeyframeAct, SIGNAL(triggered()), glwidget, SLOT(setKeyframe()));
    
    delKeyframeAct = new QAction(tr("De&lete Keyframe"), this);
    delKeyframeAct->setShortcut(QKeySequence(tr("Ctrl+k")));
    delKeyframeAct->setStatusTip(tr("Deletes the keyframe at the current frame"));
    //connect(delKeyframeAct, SIGNAL(triggered()), glwidget, SLOT(deleteKeyframe()));
    
    copyPoseAct = new QAction(tr("&Copy Pose"), this);
    copyPoseAct->setShortcut(QKeySequence(tr("Ctrl+c")));
    copyPoseAct->setStatusTip(tr("Copies the current pose"));
    //connect(copyPoseAct, SIGNAL(triggered()), glwidget, SLOT(copyPose()));
    
    pastePoseAct = new QAction(tr("&Paste Pose"), this);
    pastePoseAct->setShortcut(QKeySequence(tr("Ctrl+v")));
    pastePoseAct->setStatusTip(tr("Pastes the pose that was previously copied"));
    //connect(pastePoseAct, SIGNAL(triggered()), glwidget, SLOT(pastePose()));
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
    animationMenu->addAction(newAnimationAct);
    animationMenu->addAction(setKeyframeAct);
    animationMenu->addAction(delKeyframeAct);
    animationMenu->addAction(copyPoseAct);
    animationMenu->addAction(pastePoseAct);
}

void MainWindow::newFile()
{
    // TODO
}

void MainWindow::openFile()
{
    QString filename = QFileDialog::getOpenFileName(this,
            tr("Open GSM"), ".", tr("GSM Files (*.gsm)"));
}

void MainWindow::closeFile()
{
}

void MainWindow::saveFile()
{
}

void MainWindow::saveFileAs()
{
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

    freeRawMesh(cdata->rmesh);
    cdata->rmesh = newmesh;

    glwidget->dirtyCData();
}

void MainWindow::importBones()
{
    QString filename = QFileDialog::getOpenFileName(this, tr("Import Bones"),
            ".", tr("Skeleton Files (*.bones)"));

    if (filename.isEmpty()) return;

    Skeleton *newskel = Skeleton::readSkeleton(filename.toStdString());
    
    if (!newskel)
    {
        QMessageBox::information(this, tr("Unable to import skeleton"),
                tr("Couldn't parse bones file"));
        return;
    }

    // Clean up old skeleton
    delete cdata->skeleton;
    cdata->skeleton = newskel;

    glwidget->dirtyCData();
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

    /* TODO
    if (skeleton)
    {
        // Get a temp file
        char tmpname[] = "/tmp/geoeditXXXXXX";
        int fd = mkstemp(tmpname); // keep fd for gsm_set_bones
        std::cout << "writing skeleton to temp file " << tmpname << '\n';
        // write skeleton to tmpfile
        std::ofstream f(tmpname);
        writeSkeleton(skeleton, f);
        // write to gsm
        gsm_set_bones(gsmf, fd);

    }
    if (rmesh)
    {
        std::cout << "writing mesh\n";
        FILE *meshf = tmpfile();
        assert(meshf);
        writeRawMesh(rmesh, meshf);
        gsm_set_mesh(gsmf, meshf);
    }
    */

    gsm_close(gsmf);
}

