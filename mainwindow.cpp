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

    importAct = new QAction(tr("Import..."), this);
    importAct->setShortcut(QKeySequence(tr("Ctrl+i")));
    importAct->setStatusTip(tr("Import a resource"));
    //connect(importAct, SIGNAL(triggered()), this, SLOT(import()));

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
    //autoSkinAct->setShortcuts(QKeySequence::Open);
    autoSkinAct->setStatusTip(tr("Automatically skin the mesh"));
    connect(autoSkinAct, SIGNAL(triggered()), glwidget, SLOT(autoSkin()));
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
    fileMenu->addAction(importAct);
    fileMenu->addAction(exportAct);
    fileMenu->addSeparator();
    fileMenu->addAction(closeAct);
    fileMenu->addAction(exitAct);

    skinningMenu = menuBar()->addMenu(tr("&Skinning"));
    skinningMenu->addAction(autoSkinAct);
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
