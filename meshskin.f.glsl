#version 120

varying vec4 frag_pos;
varying vec2 frag_texcoord;
varying vec3 frag_norm;
uniform vec4 color;

const vec3 lightPos = vec3(0, 30, 50);

void main()
{
    //gl_FragData[0] = vec4(normalize(frag_texcoord), 0.f, 1.f);
    //gl_FragData[0] = vec4(normalize(frag_norm).y, 0.f, 0.f, 1.f);
    //gl_FragData[0] = vec4(normalize(frag_norm), 1.f);

    //gl_FragData[0] = color * dot(normalize(frag_norm), vec3(0, 0, -1));


    vec3 ambient = vec3(0.1f);
    vec3 diffuse = max(dot(normalize(lightPos - frag_pos.xyz), frag_norm), 0.f) * vec3(0.5f);
    gl_FragData[0] = vec4(ambient + diffuse, 1.f);

    if (color != vec4(0.f))
        gl_FragData[0] = color;
}

