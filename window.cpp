#include <QtGui>

#include "glwidget.h"
#include "window.h"

Window::Window()
{
    glWidget = new GLWidget;

    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->addWidget(glWidget);
    setLayout(mainLayout);

    glWidget->setFocus(Qt::OtherFocusReason);
    glWidget->setWindowFlags(Qt::FramelessWindowHint);
    setWindowFlags(Qt::FramelessWindowHint);

    setWindowTitle(tr("kiss-skeleton"));
}

void Window::keyPressEvent(QKeyEvent *e)
{
    QWidget::keyPressEvent(e);
}

