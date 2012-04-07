#version 120

varying vec4 frag_pos;
uniform vec4 color;

void main()
{
    gl_FragData[0] = color;
}
