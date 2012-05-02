#pragma once
#include <QObject>
#include <string>
#include <map>
#include <vector>
#include "glsubdisplay.h"

class SkeletonPose;
class QMouseEvent;

struct animation
{
    std::string name;
    std::map<int, SkeletonPose*> keyframes;
    int endFrame;
};

struct timeline_data
{
    int currentFrame;
    std::vector<animation*> animations;
    animation* currentAnimation;
};

class TimelineDisplay : public GLSubDisplay
{
public:
    TimelineDisplay(QObject *parent, timeline_data *data);

    // From GLSubDisplay (reimplement as QWidget stuff)
    virtual void render() const;
    virtual void mousePressEvent(QMouseEvent *event, int x, int y);

public slots:
    // Timeline functions
    void setFrame(int frame);

private:
    timeline_data *data_;

    // helper functions
    void setPoseFromFrame(int frame);
};

