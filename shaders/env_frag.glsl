#version 410 core
in VS_OUT {
    vec3 worldPos;
    vec3 worldNrm;
} fs_in;

uniform samplerCube envMap;
uniform vec3  cameraPos;
uniform float fresnelStrength;   // 0..1
uniform float roughness;         // 0..1, uses cubemap mip LOD

out vec4 outColor;

void main() {
    //outColor = vec4(1, 0, 1, 1); return;
    vec3 N = normalize(fs_in.worldNrm);
    vec3 V = normalize(cameraPos - fs_in.worldPos);
    vec3 R = reflect(-V, N);

    float lod = roughness * 4.0;              // needs cubemap mips
    vec3 env = textureLod(envMap, R, lod).rgb;

    // Schlick fresnel with base reflectance ~mirror backing
    float F0 = 0.04;
    float VoN = max(dot(V, N), 0.0);
    float F = F0 + (1.0 - F0) * pow(1.0 - VoN, 5.0);
    F = mix(F, 1.0, fresnelStrength);


    outColor = vec4(env * F, 1.0);
    
}
