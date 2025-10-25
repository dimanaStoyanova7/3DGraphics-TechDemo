#version 410 core
layout (location=0) in vec3 in_pos;
layout (location=1) in vec3 in_nrm;
layout (location=2) in vec2 in_uv;

uniform mat4 mvpMatrix;
uniform mat4 modelMatrix;
uniform mat3 normalModelMatrix;

out VS_OUT {
    vec3 worldPos;
    vec3 worldNrm;
} vs_out;

void main() {
    vec4 wp = modelMatrix * vec4(in_pos, 1.0);
    vs_out.worldPos = wp.xyz;
    vs_out.worldNrm = normalize(normalModelMatrix * in_nrm);

    gl_Position = mvpMatrix * vec4(in_pos, 1.0);
}
