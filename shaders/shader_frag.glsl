#version 410

layout(std140) uniform Material
{
    vec3 kd;
	vec3 ks;
	float shininess;
	float transparency;
    float metallic;
    float roughness;
    float ao;

};

uniform sampler2D colorMap;
uniform sampler2D ambientMap;
uniform sampler2D metalnessMap;
uniform sampler2D roughnessMap;
uniform sampler2D normalMap;

uniform bool hasTexCoords;
uniform bool hasAmbientTexture;
uniform bool hasMetalnessTexture;
uniform bool hasRoughnessTexture;
uniform bool hasNormalMap;

uniform bool pbr;
uniform bool nm;
uniform bool useMaterial;

uniform float glightRadius;
uniform float glightIntensity;
uniform sampler2DShadow shadowMap;  // compare-depth sampler
uniform float shadowTexelSize;      // 1.0 / shadowMapSize
uniform float shadowBias;           // e.g. 0.0015

in vec3 fragPosition;
in vec3 fragNormal;
in vec2 fragTexCoord;
in vec3 lightPos;
in vec3 camPos;
in vec3 lightColor;
in vec4 fLightClip;                

const float PI = 3.14159265359;
layout(location = 0) out vec4 fragColor;

vec3 fresnelSchlick(vec3 V, vec3 H, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - dot(V,H), 0.0, 1.0), 5.0);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a  = roughness*roughness;
    float a2 = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return a2 / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

// ---- Shadow sampling (NEW) ----
float computeShadow(vec3 N, vec3 L)
{
    // Project to [0,1] space
    vec3 uvw = fLightClip.xyz / fLightClip.w;
    uvw = uvw * 0.5 + 0.5;

    // outside light frustum? treat as lit
    if (uvw.x <= 0.0 || uvw.x >= 1.0 ||
        uvw.y <= 0.0 || uvw.y >= 1.0 ||
        uvw.z <= 0.0 || uvw.z >= 1.0) return 1.0;

    // slope-scaled bias (optional small tweak):
    float ndotl = max(dot(N, L), 0.0);
    float bias  = max(0.0005 * (1.0 - ndotl), shadowBias);

    // 3x3 PCF using sampler2DShadow
    float s = 0.0;
    for (int x = -1; x <= 1; ++x)
    for (int y = -1; y <= 1; ++y)
    {
        vec2 offs = vec2(x, y) * shadowTexelSize;
        s += texture(shadowMap, vec3(uvw.xy + offs, uvw.z - bias));
    }
    return s / 9.0;
}

void main()
{
    vec3  V     = normalize(camPos - fragPosition);
    vec3  Lvec  = lightPos - fragPosition;
    float dist  = max(length(Lvec), 1e-4);
    vec3  L     = Lvec / dist;

    vec3  N = normalize(fragNormal);
    if (nm) {
        N = texture(normalMap, fragTexCoord).rgb * 2.0 - 1.0;
        N = normalize(N);
    }

    float r  = max(glightRadius, 1e-3);
    float q  = dist / r;
    float attenuation = 1.0 / (1.0 + q*q);

    // --- Material parameters (textures or defaults) ---
    float metallic  = 0.0;
    float roughness = 0.5;
    float ao        = 1.0;

    if (hasMetalnessTexture && !useMaterial)  metallic  = texture(metalnessMap, fragTexCoord).r;
    if (hasRoughnessTexture && !useMaterial)  roughness = texture(roughnessMap, fragTexCoord).r;
    if (hasAmbientTexture   && !useMaterial)  ao        = texture(ambientMap,   fragTexCoord).r;

    // --- Base color selection ---
    vec3 baseColor = vec3(1.0, 0.0, 0.0);        // fallback (debug)
    if (useMaterial)       baseColor = kd;
    else if (hasTexCoords) baseColor = texture(colorMap, fragTexCoord).rgb;
    else                   baseColor = normalize(fragNormal) * 0.5 + 0.5;

    float NdotL = max(dot(N, L), 0.0);

    float shadowTerm = computeShadow(N, L);    
    float directScale = mix(0.15, 1.0, shadowTerm);

    if (pbr) {
        vec3 F0 = mix(vec3(0.04), baseColor, metallic);
        vec3 H  = normalize(L + V);
        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);
        vec3  F   = fresnelSchlick(V, H, F0);

        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

        vec3 numerator    = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 1e-4;
        vec3  specular    = numerator / denominator;

        vec3 radiance = lightColor * (glightIntensity * attenuation) * directScale; // <<< scaled
        vec3 Lo       = (kD * baseColor / PI + specular) * radiance * NdotL;

        vec3 ambient  = vec3(0.03) * baseColor * ao;
        vec3 color    = ambient + Lo;
        color = color / (color + vec3(1.0));
        color = pow(color, vec3(1.0/2.2));
        fragColor = vec4(color, 1.0);
    } else {
        vec3 H = normalize(L + V);
        vec3 ambient  = 0.03 * baseColor * ao;
        vec3 diffuse  = baseColor * lightColor * (NdotL * glightIntensity * attenuation) * directScale; // <<< scaled
        float specPow = max(shininess, 1.0);
        float specAmt = pow(max(dot(N, H), 0.0), specPow);
        vec3 specular = ks * lightColor * (specAmt * glightIntensity * attenuation) * directScale;      // <<< scaled
        fragColor = vec4(ambient + diffuse + specular, 1.0);
    }
}
