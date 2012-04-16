#pragma once
#include <QMainWindow>

class GLWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow();

protected:
    void contextMenuEvent(QContextMenuEvent *event);

private slots:
    void open();

private:
    QMenu *fileMenu;
    QMenu *importMenu;
    QMenu *skinningMenu;
    QMenu *animationMenu;

    QAction *newAct;
    QAction *openAct;
    QAction *saveAct;
    QAction *saveAsAct;
    QAction *importModelAct;
    QAction *importBonesAct;
    QAction *exportAct;
    QAction *closeAct;
    QAction *exitAct;

    QAction *autoSkinAct;

    QAction *newAnimation;
    QAction *setKeyframe;
    QAction *delKeyframe;
    QAction *copyPose;
    QAction *pastePose;

    GLWidget *glwidget;

    void createActions();
    void createMenus();
};

