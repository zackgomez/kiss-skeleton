#include "arcball.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

Arcball::Arcball(const glm::vec3 &eye, float radius, float aspect, float near,
        float far, float fov) :
    eye_(eye), radius_(radius),
    znear_(near), zfar_(far),
    aspect_(aspect), fov_(fov),
    projMat_(1.f), viewMat_(1.f),
    curRot_(1.f)
{
    setProjectionMatrix();
    viewMat_ = glm::translate(viewMat_, eye_);
}

void Arcball::setAspect(float aspect)
{
    aspect_ = aspect;
    setProjectionMatrix();
}

void Arcball::setZoom(float zoom)
{
    eye_.z = zoom;
}

float Arcball::getZoom() const
{
    return eye_.z;
}

void Arcball::start(const glm::vec3 &ndc)
{
    lastRot_ = curRot_;
    start_ = ndcToSphere(ndc);
}

void Arcball::move(const glm::vec3 &ndc)
{
    glm::vec3 cur = ndcToSphere(ndc);
    if (start_ == glm::vec3() || cur == glm::vec3())
        return;

    if (cur == start_)
        return;

    float cos2a = glm::dot(start_, cur);
    float sina = sqrtf((1.f - cos2a) / 2.f);
    float cosa = sqrtf((1.f + cos2a) / 2.f);
    glm::vec3 cross = glm::normalize(glm::cross(start_, cur)) * sina;

    glm::mat4 nextRot = quatrot(cross.x, cross.y, cross.z, cosa);

    curRot_ = nextRot * lastRot_;
}

const glm::mat4 &Arcball::getProjectionMatrix() const
{
    return projMat_;
}

const glm::mat4 &Arcball::getViewMatrix() const
{
    viewMat_ = glm::translate(glm::mat4(1.f), eye_);
    viewMat_ = viewMat_ * curRot_;
    // TODO possible adjust origin from 0,0,0 by translating by -origin

    return viewMat_;
}

void Arcball::setProjectionMatrix()
{
    projMat_ = glm::perspective(fov_ / glm::min(1.f, aspect_), aspect_, znear_, zfar_);
}

glm::vec3 Arcball::ndcToSphere(const glm::vec3 &ndc) const
{
    glm::vec3 ball = ndc;
    float mag = glm::length(ball);
    
    // outside of sphere, just points right at point
    if (mag > radius_)
        ball = glm::normalize(ball);
    // inside sphere, need to compute z coordinate
    else
    {
        mag = glm::min(1.f, mag);
        ball.z = sqrtf(1.f - mag*mag);
    }

    return ball;
}

glm::vec3 Arcball::applyMatrix(const glm::mat4 &mat, const glm::vec3 &vec)
{
    glm::vec4 v(vec, 1);
    v = mat * v;
    v /= v.w;
    return glm::vec3(v);
}

glm::mat4 Arcball::quatrot(float x, float y, float z, float w)
{
    glm::mat4 qmat(1.f);

    float x2 = x*x;
    float y2 = y*y;
    float z2 = z*z;
    float xy = x*y;
    float xz = x*z;
    float yz = y*z;
    float wx = w*x;
    float wy = w*y;
    float wz = w*z;

    float *q = glm::value_ptr(qmat);

    q[0] = 1 - 2*y2 - 2*z2;
    q[1] = 2*xy + 2*wz;
    q[2] = 2*xz - 2*wy;

    q[4] = 2*xy - 2*wz;
    q[5] = 1 - 2*x2 - 2*z2;
    q[6] = 2*yz + 2*wx;

    q[8] = 2*xz + 2*wy;
    q[9] = 2*yz - 2*wx;
    q[10]= 1 - 2*x2 - 2*y2;

    return qmat;
}
