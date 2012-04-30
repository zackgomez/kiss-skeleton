#pragma once
#include <QMainWindow>

class GLWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow();

private slots:
    void openFile();
    void newFile();
    void importModel();
    void importBones();
    void saveFile();
    void saveFileAs();
    void closeFile();

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

    QAction *newAnimationAct;
    QAction *setKeyframeAct;
    QAction *delKeyframeAct;
    QAction *copyPoseAct;
    QAction *pastePoseAct;

    GLWidget *glwidget;

    void createActions();
    void createMenus();
    void writeGSM(const QString &path);
};

