#include <QApplication>
#include <QDesktopWidget>

#include "glwidget.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    GLWidget glwidget;
    glwidget.show();
    return app.exec();
}

