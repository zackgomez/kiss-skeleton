#include "timeline.h"
#include <QMouseEvent>
#include <QTimer>
#include <iostream>
#include "zgfx.h"

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
    
    if (data_->currentAnimation)
    {
        glBegin(GL_LINES);
        for(int i = 1; i <= data_->currentAnimation->endFrame; i++)
        {
            // TODO some kind of blending here to show currently (occupied) frame
            // Highlight current frame red
            if (i == data_->currentFrame)
                glColor3f(1.0f, 0.f, 0.f);
            else
                glColor3f(1.0f, 1.0f, 1.0f);
            if (data_->currentAnimation->keyframes.count(i) > 0)
                glColor3f(1.0f, 1.0f, 0.0f);

            float x = -1.0f + 2.f * i / (data_->currentAnimation->endFrame + 1);
            glVertex2f(x, 0.5f);
            glVertex2f(x, -0.5f);
        }
    }
    glEnd();
}

void TimelineDisplay::mousePressEvent(QMouseEvent *event, int x, int y)
{
    if (!data_->currentAnimation) return;
    glm::vec2 coord = getCoord(x, y);

    std::cout << "timeline action @ " << coord << '\n';
    if (event->button() == Qt::LeftButton)
    {
        if (coord.y > 0.25f && coord.y < 0.75f)
        {
            int range = data_->currentAnimation->endFrame + 1;
            int frameOffset = std::min(coord.x, 1.f) * range + 0.5;
            frameOffset = std::min(std::max(frameOffset, 1), data_->currentAnimation->endFrame);
            data_->currentFrame = frameOffset;
            std::cout << "Selected frame " << data_->currentFrame << std::endl;
        }
    }
}

