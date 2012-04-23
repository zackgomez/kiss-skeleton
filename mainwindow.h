#pragma once
#include <QMainWindow>

class Skeleton;
struct SkeletonPose;
class Joint;

class GLWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow();
	Skeleton *skeleton;
    QString currentFile;
    SkeletonPose *bindPose, *copiedPose;
	Joint *selectedJoint;
    // timeline ui vars
    QTimer *animTimer;
    bool play;
    // animation vars
    TimelineData tdata_;


public slots:
    void newFile();
// TODO no arg
    void openFile(const QString &path);
    void importModel();
    void importBones();
    void saveFile();
    void saveFileAs();
    void closeFile();
    void newAnimation();
    void setKeyframe();
    void deleteKeyframe();
    void copyPose();
    void pastePose();
    void autoSkinMesh();

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

    // Timeline functions
    void setPoseFromFrame(int frame);

    static const int FPS = 24;
    class NewAnimDialog: public QDialog
    {   
    public:
        QString getAnimName(){return setAnimName->text();}
        int getAnimLen(){bool ok; return setAnimLen->text().toInt(&ok, 10);}
        NewAnimDialog(QWidget *parent) : QDialog(parent)
        {
            setModal(true);
            setFocusPolicy(Qt::StrongFocus);
            setFocus();
            setAnimNameLabel = new QLabel(tr("Name:"));
            setAnimName = new QLineEdit(this);
            setAnimLenLabel = new QLabel(tr("Length:"));
            setAnimLen = new QLineEdit(this);
            setAnimLen->setInputMask(tr("9999"));
            acceptButton = new QPushButton(tr("OK"));
            connect(acceptButton, SIGNAL(clicked()), this, SLOT(accept()));
            cancelButton = new QPushButton(tr("Cancel"));
            connect(cancelButton, SIGNAL(clicked()), this, SLOT(reject()));

            QGridLayout *layout = new QGridLayout;
            layout->addWidget(setAnimNameLabel, 0, 0);
            layout->addWidget(setAnimName, 0, 1);
            layout->addWidget(setAnimLenLabel, 1, 0);
            layout->addWidget(setAnimLen, 1, 1);
            layout->addWidget(acceptButton, 2, 0);
            layout->addWidget(cancelButton, 2, 1);
            setLayout(layout);
            exec();
        }
    private:
        QPushButton *acceptButton;
        QPushButton *cancelButton;
        QLabel *setAnimNameLabel;
        QLineEdit *setAnimName;
        QLabel *setAnimLenLabel;
        QLineEdit *setAnimLen;
    };
};

struct timeline_data
{
    int currentFrame;
    std::vector<animation*> animations;
    animation* currentAnimation;
};

struct animation
{
    std::string name;
    std::map<int, SkeletonPose*> keyframes;
    int endFrame;
};
