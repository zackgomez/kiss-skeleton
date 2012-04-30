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

