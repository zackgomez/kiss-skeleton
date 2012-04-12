#include "glsubdisplay.h"
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <QObject>
#include <QMouseEvent>
#include <QTimer>
#include <iostream>
#include "zgfx.h"

glm::vec2 GLSubDisplay::getCoord(int x, int y) const
{
    return glm::vec2(
            (x - x_) / (float) w_,
            (y - y_) / (float) h_ );
}

TimelineDisplay::TimelineDisplay(QObject *parent, timeline_data *data) :
    GLSubDisplay(parent),
    data_(data)
{
}

void TimelineDisplay::render() const
{
    // Render in NDC coordinates, no projection matrix needed
    glViewport(x_, y_, w_, h_);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    // TODO Move that initialization to GLSubDisplay base class

    // Background Color
    glColor3f(0.25f, 0.25f, 0.25f);
    glBegin(GL_QUADS);
        glVertex2f(-1, 1);
        glVertex2f(-1, -1);
        glVertex2f(1, -1);
        glVertex2f(1, 1);
    glEnd();

    // Foreground
    glColor3f(0.5f, 0.5f, 0.5f);
    glBegin(GL_QUADS);
        glVertex2f(-1, 0.5f);
        glVertex2f(-1, -0.5f);
        glVertex2f(1, -0.5f);
        glVertex2f(1, 0.5f);
    glEnd();
    
    glBegin(GL_LINES);
    for(int i = data_->startFrame; i <= data_->endFrame; i++)
    {
        // TODO some kind of blending here to show currently (occupied) frame
        // Highlight current frame red
        if (i == data_->currentFrame)
            glColor3f(1.0f, 0.f, 0.f);
        else
            glColor3f(1.0f, 1.0f, 1.0f);
        if (data_->keyframes.count(i) > 0)
            glColor3f(1.0f, 1.0f, 0.0f);

        float x = -1.0f + 2.f * i / (data_->endFrame - data_->startFrame + 2);
        glVertex2f(x, 0.5f);
        glVertex2f(x, -0.5f);
    }
    glEnd();
}

void TimelineDisplay::mousePressEvent(QMouseEvent *event)
{
    glm::vec2 coord = getCoord(event->x(), event->y());

    std::cout << "timeline action @ " << coord << '\n';
    if (event->button() == Qt::LeftButton)
    {
        if (coord.y > 0.25f && coord.y < 0.75f)
        {
            int range = data_->endFrame - data_->startFrame;
            int frameOffset = std::min(coord.x, 1.f) * range + 1;
            data_->currentFrame = data_->startFrame + frameOffset;
            std::cout << "Selected frame " << data_->currentFrame << std::endl;
        }
    }
}

