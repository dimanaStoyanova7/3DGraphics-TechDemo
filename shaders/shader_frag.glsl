#version 410
layout(std140) uniform Material {
    vec3  kd;
    vec3  ks;
    float shininess;
    float transparency;
    float uMetallic;
    float uRoughness;
    float uAO;
};

uniform sampler2D        colorMap;
uniform sampler2D        ambientMap;
uniform sampler2D        metalnessMap;
uniform sampler2D        roughnessMap;
uniform sampler2D        normalMap;

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
uniform sampler2DShadow shadowMap;
uniform vec2  shadowTexelSize;   // ? vec2 safer
uniform float shadowBias;

in vec3 fragPosition;
in vec3 fragNormal;
in vec2 fragTexCoord;
in vec3 lightPos;
in vec3 camPos;
in vec3 lightColor;
in vec4 fLightClip;

layout(location=0) out vec4 fragColor;

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float r){ float a=r*r; float a2=a*a; float NdotH=max(dot(N,H),0.0); float NdotH2=NdotH*NdotH; float d=(NdotH2*(a2-1.0)+1.0); return a2/(PI*d*d); }
float GeometrySchlickGGX(float NdotV,float r){ float k=pow(r+1.0,2.0)/8.0; return NdotV/(NdotV*(1.0-k)+k); }
float GeometrySmith(vec3 N, vec3 V, vec3 L, float r){ return GeometrySchlickGGX(max(dot(N,V),0.0),r) * GeometrySchlickGGX(max(dot(N,L),0.0),r); }
vec3  fresnelSchlick(vec3 V, vec3 H, vec3 F0){ return F0 + (1.0 - F0) * pow(clamp(1.0 - dot(V,H), 0.0, 1.0), 5.0); }

float computeShadow(vec3 N, vec3 L){
    vec3 uvw = fLightClip.xyz / fLightClip.w;
    uvw = uvw * 0.5 + 0.5;
    if (any(lessThan(uvw, vec3(0.0))) || any(greaterThan(uvw, vec3(1.0)))) return 1.0;
    float ndotl = max(dot(N,L),0.0);
    float bias  = max(0.0005*(1.0-ndotl), shadowBias);
    float s = 0.0;
    for(int x=-1;x<=1;++x)
    for(int y=-1;y<=1;++y){
        vec2 offs = vec2(x,y) * shadowTexelSize;
        s += texture(shadowMap, vec3(uvw.xy + offs, uvw.z - bias));
    }
    return s/9.0;
}

void main(){
    vec3 Vdir = normalize(camPos - fragPosition);
    vec3 Lvec = lightPos - fragPosition;
    float dist = max(length(Lvec), 1e-4);
    vec3 Ldir = Lvec / dist;

    vec3 N = normalize(fragNormal);
    if (nm && hasNormalMap){
        N = texture(normalMap, fragTexCoord).rgb * 2.0 - 1.0; // tangent space normal
        N = normalize(N);
    }

    float r   = max(glightRadius, 1e-3);
    float q   = dist / r;
    float att = 1.0 / (1.0 + q*q);

    // material params
    float mtl = uMetallic;
    float rough = uRoughness;
    float ao = uAO;
    if (hasMetalnessTexture && !useMaterial)  mtl   = texture(metalnessMap,  fragTexCoord).r;
    if (hasRoughnessTexture && !useMaterial)  rough = texture(roughnessMap,  fragTexCoord).r;
    if (hasAmbientTexture   && !useMaterial)  ao    = texture(ambientMap,    fragTexCoord).r;

    // base color
    vec3 baseColor;
    if (useMaterial)       baseColor = kd;
    else if (hasTexCoords) baseColor = texture(colorMap, fragTexCoord).rgb;
    else                   baseColor = normalize(fragNormal)*0.5 + 0.5;

    float NdotL = max(dot(N, Ldir), 0.0);
    float shadow = computeShadow(N, Ldir);
    float directScale = mix(0.15, 1.0, shadow);

    if (pbr){
        vec3 F0 = mix(vec3(0.04), baseColor, mtl);
        vec3 H  = normalize(Ldir + Vdir);
        float NDF = DistributionGGX(N, H, rough);
        float G   = GeometrySmith(N, Vdir, Ldir, rough);
        vec3  F   = fresnelSchlick(Vdir, H, F0);
        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - mtl);
        vec3 specular = (NDF * G * F) / (4.0 * max(dot(N,Vdir),0.0) * NdotL + 1e-4);
        vec3 radiance = lightColor * (glightIntensity * att) * directScale;
        vec3 Lo = (kD * baseColor / PI + specular) * radiance * NdotL;
        vec3 ambient = vec3(0.03) * baseColor * ao;
        vec3 color = ambient + Lo;
        color = color / (color + vec3(1.0));
        color = pow(color, vec3(1.0/2.2));
        fragColor = vec4(color, 1.0);
    } else {
        vec3 H = normalize(Ldir + Vdir);
        vec3 ambient = 0.03 * baseColor * ao;
        vec3 diffuse = baseColor * lightColor * (NdotL * glightIntensity * att) * directScale;
        float specPow = max(shininess, 1.0);
        float specAmt = pow(max(dot(N, H), 0.0), specPow);
        vec3 specular = ks * lightColor * (specAmt * glightIntensity * att) * directScale;
        fragColor = vec4(ambient + diffuse + specular, 1.0);
    }
}
