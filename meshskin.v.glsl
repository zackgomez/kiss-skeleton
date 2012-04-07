#version 150

attribute vec4 position;
attribute int  joint;
attribute vec3 normal;
attribute vec2 texcoord;

uniform mat4 projectionMatrix;
uniform mat4 viewMatrix;
uniform mat4 modelMatrix;

uniform mat4 jointMatrices[32];

varying vec4 frag_pos;
varying vec2 frag_texcoord;
varying vec3 frag_norm;

void main()
{
    mat4 vertMat = modelMatrix * jointMatrices[joint];
    mat3 normalMat = transpose(inverse(mat3(vertMat)));
    frag_pos = vertMat * position;
    frag_texcoord = texcoord;
    frag_norm = normalMat * normal;

    gl_Position = projectionMatrix * viewMatrix * frag_pos;
}
