#version 410 core

uniform mat4 mvpMatrix;
uniform mat4 modelMatrix;
uniform mat4 lightVP;
uniform mat3 normalModelMatrix;

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;

// ONLY Per-Vertex data should be passed as 'out'
out vec3 gPosition;
out vec3 gNormal;
out vec2 gTexCoord;
out vec4 gLightClip;

void main()
{
    gl_Position = mvpMatrix * vec4(position, 1);
    
    vec4 worldPos = modelMatrix * vec4(position, 1.0);
    gPosition     = worldPos.xyz;
    gNormal       = normalModelMatrix * normal;
    gTexCoord     = texCoord;

    gLightClip    = lightVP * worldPos;

}