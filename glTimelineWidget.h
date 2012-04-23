#pragma once
#include <map>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <QWidget>

class QObject;
class QMouseEvent;
class QTimer;
class QWheelEvent;
class SkeletonPose;

class GLTimelineDisplay: public QWidget
{
    Q_OBJECT

public:
    GLTimelineDisplay(QObject *parent) : parent_(parent) { }
    virtual ~GLTimelineDisplay() { }

    // x and y specify lower left corner
    virtual void setViewport(int x, int y, int w, int h) { x_ = x; y_ = y; w_ = w; h_ = h; }
    virtual bool contains(int x, int y) { return x > x_ && x < x_ + w_ && y > y_ && y < y_ + h_; }
    virtual void paintEvent(QPaintEvent *event) const = 0;

    virtual void mousePressEvent(QMouseEvent *, int, int) { }
    virtual void mouseReleaseEvent(QMouseEvent *, int, int) { }
    virtual void mouseMoveEvent(QMouseEvent *, int, int) { }
    virtual void wheelEvent(QWheelEvent *, int, int) { }

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


class TimelineDisplay : public GLTimelineDisplay
{
public:
    TimelineDisplay(QObject *parent, timeline_data *data);

    virtual void paintEvent(QPaintEvent *event) const;
    virtual void mousePressEvent(QMouseEvent *event, int x, int y);

private:
    // Timeline vars
    timeline_data *tdata_;
    TimelineDisplay *tdisplay_;
    // Timeline functions
    void setFrame(int frame);
};

