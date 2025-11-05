#version 410 core

uniform mat4 mvpMatrix;
uniform mat4 modelMatrix;

uniform mat3 normalModelMatrix;

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;

// ONLY Per-Vertex data should be passed as 'out'
out vec3 gPosition;
out vec3 gNormal;
out vec2 gTexCoord;


uniform float time;         
uniform float uG = 9.81;     


float omegaFromLambda(float lambda) {
    // deep-water dispersion:  ?^2 = g * k,  with k = 2?/?
    float k = 6.28318530718 / lambda;
    return sqrt(uG * k);
}


float waveDirectional(vec2 xz) {
    // params
    const float A = 0.35;                 // amplitude
    const float lambda = 18.0;            // wavelength (world units)
    const vec2 dir = normalize(vec2(1.0, 0.25)); // travel direction in XZ
    const float phi = 0.0;                // phase offset

    float k = 6.28318530718 / lambda;
    float w = omegaFromLambda(lambda);
    float theta = k * dot(dir, xz) - w * time + phi;
    return A * sin(theta);
}


float waveStanding(vec2 xz) {
    const float A = 0.20;
    const float lambda = 12.0;
    const vec2 axis = normalize(vec2(1.0, 0.0)); 
    float k = 6.28318530718 / lambda;
    float w = omegaFromLambda(lambda);

    
    return A * sin(k * dot(axis, xz)) * cos(w * time);
}


float waveRadial(vec2 xz) {
    const float A = 0.15;
    const float lambda = 10.0;
    const vec2 center = vec2(0.0, 0.0);
    float r = length(xz - center);

    float k = 6.28318530718 / lambda;
    float w = omegaFromLambda(lambda);


    float falloff = 1.0 / (1.0 + 0.05 * r);
    return A * sin(k * r - w * time) * falloff;
}


float waveChoppy(vec2 xz) {
    const float A = 0.12;                 
    const float lambda = 8.0;
    const vec2 dir = normalize(vec2(0.2, 1.0));
    const float phi = 1.3;

    float k = 6.28318530718 / lambda;
    float w = omegaFromLambda(lambda);
    float theta = k * dot(dir, xz) - w * time + phi;

    // add a small 3*freq term for pointier crests
    return A * (sin(theta) + 0.3 * sin(3.0 * theta));
}

// Sum of all waves 
float wavesHeight(vec2 xz) {
    return waveDirectional(xz)
         + waveStanding(xz)
         + waveRadial(xz)
         + waveChoppy(xz);
}



void main()
{

    vec3 newPostion = vec3(position);
    newPostion.y += wavesHeight(newPostion.xz);

    gl_Position = mvpMatrix * vec4(newPostion, 1);
    
    gPosition   = (modelMatrix * vec4(newPostion, 1)).xyz;
    gNormal     = normalModelMatrix * normal;
    gTexCoord   = texCoord;

}