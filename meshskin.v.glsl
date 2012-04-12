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

void main()
{
    mat4 vertMat = mat4(0.f);
    float total_weight = 0.f;
    for (int i = 0; i < 4; i++)
    {
        if (weight[i] != 0.f)
        {
            vertMat += weight[i] * jointMatrices[joint[i]];
            total_weight += weight[i];
        }
    }
    // XXX deal with verts w/o weighting, just don't transform
    if (total_weight == 0.f)
        vertMat = mat4(1.f);

    mat3 normalMat = transpose(inverse(mat3(vertMat)));

    frag_pos = vertMat * position;
    frag_texcoord = texcoord;
    frag_norm = normalMat * norm;

    gl_Position = projectionMatrix * viewMatrix * frag_pos;
}
