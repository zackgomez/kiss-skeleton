#pragma once
#include <QGLWidget>
#include <glm/glm.hpp>
#include "Arcball.h"

class Arcball;

class GLWidget : public QGLWidget
{
    Q_OBJECT

public:
        GLWidget(QWidget *parent = 0);
        ~GLWidget();

        QSize minimumSizeHint() const;
        QSize sizeHint() const;

protected:
        void initializeGL();
        void paintGL();
        void resizeGL(int width, int height);
        void mousePressEvent(QMouseEvent *event);
        void mouseReleaseEvent(QMouseEvent *event);
        void mouseMoveEvent(QMouseEvent *event);
        void wheelEvent(QWheelEvent *event);

private:
        Arcball *arcball;
        int windowWidth, windowHeight, timelineHeight;
        bool rotating, translating, zooming;
        glm::vec2 dragStart;
        float zoomStart;

        void renderEditGrid() const;

        glm::mat4 getProjectionMatrix() const;
        glm::mat4 getViewMatrix() const;
        glm::vec2 clickToScreenPos(int x, int y) const;

        static const float ZOOM_SPEED;
        static const float WHEEL_ZOOM_SPEED;
};

