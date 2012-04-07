#version 120

varying vec4 frag_pos;
varying float frag_joint;
uniform vec4 color;

void main()
{
    gl_FragData[0] = frag_joint > 16 ? glm::vec4(0.8f, 0.1, 0.1, 1.f) : color;
}
