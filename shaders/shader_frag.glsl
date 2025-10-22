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

    // --- Base color selection (matches your partner's behavior) ---
    vec3 baseColor = vec3(1,0,0); // default red, if nothing else applies
    if (hasTexCoords) {
        baseColor = texture(colorMap, fragTexCoord).rgb;
    } else if (useMaterial) {
        baseColor = kd;
    } else {
        // normal visualization (debug)
        baseColor = normalize(fragNormal) * 0.5 + 0.5;
    }

    // --- Simple point light shading (your addition) ---
    vec3 L = normalize(lightPos - fragPosition);
    float NdotL = max(dot(N, L), 0.0);

    vec3 ambient  = 0.05 * baseColor;
    vec3 diffuse  = baseColor * lightColor * NdotL;

    // Optional Blinn-Phong specular (uses ks & shininess from UBO)
    // For full correctness, pass camera/world eye position; here we assume near origin.
    vec3 V = normalize(-fragPosition);
    vec3 H = normalize(L + V);
    float specPow = max(shininess, 1.0);
    float specAmt = pow(max(dot(N, H), 0.0), specPow);
    vec3 specular = ks * lightColor * specAmt;

    fragColor = vec4(ambient + diffuse + specular, 1.0);
}