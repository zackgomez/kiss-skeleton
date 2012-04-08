#version 150

attribute vec4 position;
attribute vec3 norm;
attribute vec2 texcoord;
attribute ivec4 joint;
attribute vec4 weight;

uniform mat4 projectionMatrix;
uniform mat4 viewMatrix;
uniform mat4 modelMatrix;

uniform mat4 jointMatrices[32];

varying vec4 frag_pos;
varying vec2 frag_texcoord;
varying vec3 frag_norm;
varying float frag_weight;

void main()
{
    mat4 vertMat = mat4(0.f);
    frag_weight = 0.f;
    for (int i = 0; i < 4; i++)
    {
        vertMat += weight[i] * jointMatrices[joint[i]];
        frag_weight += weight[i];
    }

    mat3 normalMat = transpose(inverse(mat3(vertMat)));

    frag_pos = vertMat * position;
    frag_texcoord = texcoord;
    frag_norm = norm;

    gl_Position = projectionMatrix * viewMatrix * frag_pos;
}
