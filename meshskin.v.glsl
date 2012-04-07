#version 130

attribute vec4 position;
attribute int  joint;

uniform mat4 projectionMatrix;
uniform mat4 modelViewMatrix;

uniform mat4 jointMatrices[32];

varying vec4 frag_pos;
varying float frag_joint;

void main()
{
    frag_pos = modelViewMatrix * glm::vec4(joint,0,0,1);
    frag_pos = modelViewMatrix * jointMatrices[joint] * position;
    frag_joint = joint;

    gl_Position = projectionMatrix * frag_pos;
}
