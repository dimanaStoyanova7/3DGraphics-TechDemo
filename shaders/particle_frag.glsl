#version 410 core
in float vAlpha;
uniform vec3 tint; // 0..1
out vec4 fragColor;

void main() {
    // circular soft sprite using gl_PointCoord
    vec2 uv = gl_PointCoord * 2.0 - 1.0;
    float r2 = dot(uv, uv);
    if (r2 > 1.0) discard;
    float falloff = exp(-3.0 * r2);      // soft center
    float a = vAlpha * falloff;

    // HDR-ish glow (your tone mapping will tame it)
    vec3 color = tint * 3.0;
    fragColor = vec4(color, a);
}
