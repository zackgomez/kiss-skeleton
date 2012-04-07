#version 130

attribute vec4 position;
attribute int  joint;

uniform mat4 projectionMatrix;
uniform mat4 modelViewMatrix;

uniform mat4 jointMatrices[32];

varying vec4 frag_pos;

void main()
{
    frag_pos = modelViewMatrix * jointMatrices[joint] * position;

    gl_Position = projectionMatrix * frag_pos;
}
