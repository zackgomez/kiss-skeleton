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
    QMenu *skinningMenu;

    QAction *exitAct;
    QAction *openAct;

    GLWidget *glwidget;

    void createActions();
    void createMenus();

};

