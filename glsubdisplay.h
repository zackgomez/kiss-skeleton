#pragma once
#include <map>
#include <vector>
#include <string>
#include <glm/glm.hpp>

class QObject;
class QMouseEvent;
class QWheelEvent;

class GLSubDisplay
{
public:
    GLSubDisplay(QObject *parent) : parent_(parent) { }
    virtual ~GLSubDisplay() { }

    // x and y specify lower left corner
    virtual void setViewport(int x, int y, int w, int h) { x_ = x; y_ = y; w_ = w; h_ = h; }
    virtual bool contains(int x, int y) { return x > x_ && x < x_ + w_ && y > y_ && y < y_ + h_; }
    virtual void render() const = 0;

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

