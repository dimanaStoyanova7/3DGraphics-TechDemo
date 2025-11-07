#version 410

uniform mat4 modelMatrix;
uniform mat4 lightVP;   // light's ViewProjection

layout(location = 0) in vec3 position;

void main()
{
    gl_Position = lightVP * modelMatrix * vec4(position, 1.0);
}