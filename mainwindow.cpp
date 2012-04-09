#include <QMenuBar>
#include <QAction>
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
    exitAct = new QAction(tr("E&xit"), this);
    QKeySequence quitseq(tr("Ctrl+q"));
    exitAct->setShortcut(quitseq);
    exitAct->setStatusTip(tr("Exit the application"));
    connect(exitAct, SIGNAL(triggered()), this, SLOT(close()));

    newAct = new QAction(tr("&New"), this);
    newAct->setShortcuts(QKeySequence::New);
    newAct->setStatusTip(tr("Start a new model"));
    connect(newAct, SIGNAL(triggered()), this, SLOT(new()));

    openAct = new QAction(tr("&Open"), this);
    openAct->setShortcuts(QKeySequence::Open);
    openAct->setStatusTip(tr("Open an existing file"));
    connect(openAct, SIGNAL(triggered()), this, SLOT(open()));

    importAct = new QAction(tr("Import"), this);
    importAct->setShortcut(QKeySequence(tr("Ctrl+i")));
    importAct->setStatusTip(tr("Import a resource"));
    connect(openAct, SIGNAL(triggered()), this, SLOT(import()));

    exportAct = new QAction(tr("Export"), this);
    exportAct->setShortcut(QKeySequence(tr("Ctrl+e")));
    exportAct->setStatusTip(tr("Export a resource"));
    connect(exportAct, SIGNAL(triggered()), this, SLOT(export()));

    autoSkinAct = new QAction(tr("Auto Skin"), this);
    autoSkinAct->setShortcuts(QKeySequence::Open);
    autoSkinAct->setStatusTip(tr("Automatically skin the mesh"));
    connect(openAct, SIGNAL(triggered()), glwidget, SLOT(autoSkin()));
}

void MainWindow::createMenus()
{
    fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(newAct);
    fileMenu->addAction(openAct);
    fileMenu->addSeparator();
    fileMenu->addAction(importAct);
    fileMenu->addAction(exportAct);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAct);

    skinningMenu = menuBar()->addMenu(tr("&Skinning"));
    skinningMenu->addAction(autoSkinAct);
}

void MainWindow::contextMenuEvent(QContextMenuEvent *event)
{
}

void MainWindow::open()
{
}
