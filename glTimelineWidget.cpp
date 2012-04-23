#include "glTimelineWidget.h"
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <QObject>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <iostream>
#include "zgfx.h"

glm::vec2 GLTimelineDisplay::getCoord(int x, int y) const
{
    return glm::vec2(
            (x - x_) / (float) w_,
            (y - y_) / (float) h_ );
}

TimelineDisplay::TimelineDisplay(QObject *parent, timeline_data *data) :
    GLTimelineDisplay(parent),
    data_(data)
{
}

void TimelineDisplay::paintEvent(QPaintEvent *event) const
{
    
    QPainter painter(this); 
    painter.setBrush(QBrush(Qt::gray, Qt::SolidPattern));
    painter.drawRect(0, 0, width(), height());
    if (data_->currentAnimation != NULL)
    {
        for(int i = 1; i <= data_->currentAnimation->endFrame; i++)
        {
            // Highlight current frame red
            if (i == data_->currentFrame)
                painter.setPen(QPen(Qt::red, 1, Qt::SolidLine));
            else
                painter.setPen(QPen(Qt::white, 1, Qt::SolidLine));
            if (data_->currentAnimation->keyframes.count(i) > 0)
                if (i == data_->currentFrame)
                    painter.setPen(QPen(Qt::orange, 1, Qt::SolidLine));
                else
                    painter.setPen(QPen(Qt::yellow, 1, Qt::SolidLine));

            float x = -1.0f + 2.f * i / (data_->currentAnimation->endFrame + 1);
            painter.drawLine(x, 0, x, height());
        }
    }
}

void TimelineDisplay::mousePressEvent(QMouseEvent *event, int x, int y)
{
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

