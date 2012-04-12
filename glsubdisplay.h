#pragma once
#include <map>
#include <glm/glm.hpp>

class QObject;
class QMouseEvent;
class QTimer;
class QWheelEvent;
class SkeletonPose;

class GLSubDisplay
{
public:
    GLSubDisplay(QObject *parent) : parent_(parent) { }
    virtual ~GLSubDisplay() { }

    // x and y specify lower left corner
    virtual void setViewport(int x, int y, int w, int h) { x_ = x; y_ = y; w_ = w; h_ = h; }
    virtual bool contains(int x, int y) { return x > x_ && x < x_ + w_ && y > y_ && y < y_ + h_; }
    virtual void render() const = 0;

    virtual void mousePressEvent(QMouseEvent *) { }
    virtual void mouseReleaseEvent(QMouseEvent *) { }
    virtual void mouseMoveEvent(QMouseEvent *) { }
    virtual void wheelEvent(QWheelEvent *) { }

protected:
    QObject *parent_;
    int x_, y_, w_, h_;

    glm::vec2 getCoord(int x, int y) const;
};

/*
class 3DViewDisplay : public GLSubDisplay
{
public:
    virtual void render() const;

private:
    Arcball *arcball_;
};
*/

struct timeline_data
{
    int startFrame, endFrame, currentFrame;
    std::map<int, SkeletonPose*> keyframes;
};
class TimelineDisplay : public GLSubDisplay
{
public:
    TimelineDisplay(QObject *parent, timeline_data *data);

    virtual void render() const;
    virtual void mousePressEvent(QMouseEvent *event);

private:
    timeline_data *data_;
};

