#pragma once
#include <glm/glm.hpp>

class Arcball
{
public:
    Arcball(const glm::vec3 &eye, float radius, float aspect, float near, float far, float fov);

    void start(const glm::vec3 &ndc);
    void move(const glm::vec3 &ndc);
	void translate(const glm::vec2 &change);

    float getZoom() const;
    glm::vec3 getOrigin() const;
    glm::vec3 getUp() const;
    glm::vec3 getRight() const;

    void setAspect(float aspect);
    void setZoom(float);
    void setOrigin(const glm::vec3 &origin);

    const glm::mat4 &getProjectionMatrix() const;
    const glm::mat4 &getViewMatrix() const;

private:
    glm::vec3 origin_;
    glm::vec3 eye_;
    float radius_;
    float znear_, zfar_;
    float aspect_, fov_;

    glm::mat4 projMat_;
    mutable glm::mat4 viewMat_;
    glm::mat4 curRot_, lastRot_;

    glm::vec3 start_;
	glm::vec3 originStart_;

    void setProjectionMatrix();
    glm::vec3 ndcToSphere(const glm::vec3 &ndc) const;
    static glm::vec3 applyMatrix(const glm::mat4 &mat, const glm::vec3 &vec, bool homo = true);
    static glm::mat4 quatrot(float x, float y, float z, float w);
};

