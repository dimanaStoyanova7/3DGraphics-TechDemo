#version 410

layout(std140) uniform Material // Must match the GPUMaterial defined in src/mesh.h
{
    vec3 kd;
	vec3 ks;
	float shininess;
	float transparency;
};

uniform sampler2D colorMap;
uniform bool hasTexCoords;
uniform bool useMaterial;

in vec3 fragPosition;
in vec3 fragNormal;
in vec2 fragTexCoord;

uniform vec3 lightPos;    // world-space lamp position
uniform vec3 lightColor;  // e.g., (10,9,7) for a bright warm light

layout(location = 0) out vec4 fragColor;

void main()
{
    vec3 N = normalize(fragNormal);
    vec3 baseColor;

    if (hasTexCoords)       baseColor = texture(colorMap, fragTexCoord).rgb;
    else if (useMaterial)   baseColor = kd;
    else                    baseColor = normalize(fragNormal) * 0.5 + 0.5;

    // Simple point light (no attenuation for clarity; add if you like)
    vec3 L = normalize(lightPos - fragPosition);
    float NdotL = max(dot(N, L), 0.0);

    // Ambient + diffuse (add specular if desired)
    vec3 ambient = 0.05 * baseColor;
    vec3 diffuse = baseColor * lightColor * NdotL;

    // Optional Blinn-Phong specular
    vec3 V = normalize(-fragPosition);        // assuming camera at origin in world; for full correctness pass camera pos
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), max(shininess, 1.0));
    vec3 specular = ks * lightColor * spec;

    fragColor = vec4(ambient + diffuse + specular, 1.0);
}
