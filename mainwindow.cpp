#include <QMenuBar>
#include <QAction>
#include <QFileDialog>
#include <iostream>
#include "mainwindow.h"
#include "glwidget.h"

MainWindow::MainWindow()
{
    glwidget = new GLWidget;
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
    connect(newAct, SIGNAL(triggered()), glwidget, SLOT(newFile()));

    openAct = new QAction(tr("&Open..."), this);
    openAct->setShortcuts(QKeySequence::Open);
    openAct->setStatusTip(tr("Open an existing file"));
    connect(openAct, SIGNAL(triggered()), this, SLOT(open()));

    saveAct = new QAction(tr("&Save"), this);
    saveAct->setShortcuts(QKeySequence::Save);
    saveAct->setStatusTip(tr("Save the current file"));
    connect(saveAct, SIGNAL(triggered()), glwidget, SLOT(saveFile()));

    saveAsAct = new QAction(tr("Save &As..."), this);
    saveAsAct->setShortcut(QKeySequence(tr("Ctrl+Shift+S")));
    saveAsAct->setStatusTip(tr("Save the current file with a different name"));
    connect(saveAsAct, SIGNAL(triggered()), glwidget, SLOT(saveFileAs()));

    importModelAct = new QAction(tr("Import .OBJ"), this);
    importModelAct->setShortcut(QKeySequence(tr("Ctrl+i")));
    importModelAct->setStatusTip(tr("Import a mesh file"));
    connect(importModelAct, SIGNAL(triggered()), glwidget, SLOT(importModel()));

    importBonesAct = new QAction(tr("Import .bones"), this);
    importBonesAct->setShortcut(QKeySequence(tr("Ctrl+i")));
    importBonesAct->setStatusTip(tr("Import a skeleton file"));
    connect(importBonesAct, SIGNAL(triggered()), glwidget, SLOT(importBones()));

    exportAct = new QAction(tr("Export..."), this);
    exportAct->setShortcut(QKeySequence(tr("Ctrl+e")));
    exportAct->setStatusTip(tr("Export a resource"));
    //connect(exportAct, SIGNAL(triggered()), this, SLOT(export()));

    closeAct = new QAction(tr("&Close"), this);
    closeAct->setShortcuts(QKeySequence::Close);
    closeAct->setStatusTip(tr("Close the current model"));
    connect(closeAct, SIGNAL(triggered()), glwidget, SLOT(closeFile()));

    exitAct = new QAction(tr("E&xit"), this);
    exitAct->setShortcut(QKeySequence(tr("Ctrl+q")));
    exitAct->setStatusTip(tr("Exit the application"));
    connect(exitAct, SIGNAL(triggered()), this, SLOT(close()));


    autoSkinAct = new QAction(tr("Auto Skin"), this);
    //autoSkinAct->setShortcuts(QKeySequence(tr("Ctrl+")));
    autoSkinAct->setStatusTip(tr("Automatically skin the mesh"));
    connect(autoSkinAct, SIGNAL(triggered()), glwidget, SLOT(autoSkin()));
    
    
    setKeyframe = new QAction(tr("Set &Keyframe"), this);
    setKeyframe->setShortcut(QKeySequence(tr("Ctrl+k")));
    setKeyframe->setStatusTip(tr("Creates a keyframe at the current frame with the current pose"));
    connect(setKeyframe, SIGNAL(triggered()), glwidget, SLOT(setKeyframe()));
    
    delKeyframe = new QAction(tr("De&lete Keyframe"), this);
    delKeyframe->setShortcut(QKeySequence(tr("Ctrl+l")));
    delKeyframe->setStatusTip(tr("Deletes the keyframe at the current frame"));
    connect(delKeyframe, SIGNAL(triggered()), glwidget, SLOT(deleteKeyframe()));
    
    copyPose = new QAction(tr("&Copy Pose"), this);
    copyPose->setShortcut(QKeySequence(tr("Ctrl+c")));
    copyPose->setStatusTip(tr("Copies the current pose"));
    connect(copyPose, SIGNAL(triggered()), glwidget, SLOT(copyPose()));
    
    pastePose = new QAction(tr("&Paste Pose"), this);
    pastePose->setShortcut(QKeySequence(tr("Ctrl+v")));
    pastePose->setStatusTip(tr("Pastes the pose that was previously copied"));
    connect(pastePose, SIGNAL(triggered()), glwidget, SLOT(pastePose()));
    
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
        glwidget->openFile(filename);
}
