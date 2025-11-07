#version 410 core
layout(location=0) in vec3 inPos;   
layout(location=1) in float inLife;

uniform mat4 vp;
uniform float baseSize;
uniform float lifeMax;  

out float vAlpha;

void main() {
    gl_Position = vp * vec4(inPos, 1.0);

    float t = clamp(inLife, 0.0, lifeMax) / lifeMax;
    vAlpha = t * t;

    float w = max(gl_Position.w, 0.001);
    gl_PointSize = max(100.0, baseSize / w);
}
