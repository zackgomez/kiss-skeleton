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

    openAct = new QAction(tr("&Open..."), this);
    openAct->setShortcuts(QKeySequence::Open);
    openAct->setStatusTip(tr("Open an existing file"));
    connect(openAct, SIGNAL(triggered()), this, SLOT(open()));

    autoSkinAct = new QAction(tr("Auto Skin..."), this);
    autoSkinAct->setShortcuts(QKeySequence::Open);
    autoSkinAct->setStatusTip(tr("Automatically skin the mesh"));
    connect(openAct, SIGNAL(triggered()), this, SLOT(glwidget->autoSkin()));
}

void MainWindow::createMenus()
{
    fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(openAct);
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
