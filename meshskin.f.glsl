#version 120

varying vec4 frag_pos;
varying vec2 frag_texcoord;
varying vec3 frag_norm;
uniform vec4 color;

void main()
{
    //gl_FragData[0] = vec4(normalize(frag_texcoord), 0.f, 1.f);
    //gl_FragData[0] = vec4(normalize(frag_norm), 1.f);
    gl_FragData[0] = color;
}
