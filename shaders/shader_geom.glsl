#version 410 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

// ===== inputs from vertex shader (per vertex) =====
in vec3 gPosition[];
in vec3 gNormal[];
in vec2 gTexCoord[];
in vec4 gLightClip[];          // <<< ADDED

// ===== Uniforms (single value per draw call) =====
uniform mat4 modelMatrix;
uniform vec3 glightPos;
uniform vec3 gcamPos;
uniform vec3 glightColor;
uniform bool nm;
uniform bool hasNormalMap;

// ===== outputs to fragment shader =====
out vec3 fragPosition;
out vec3 fragNormal;
out vec2 fragTexCoord;
out vec3 lightPos;
out vec3 camPos;
out vec3 lightColor;
out vec4 fLightClip;

void main()
{
    if(nm && hasNormalMap){
        // Edges of the triangle (Calculations look correct for TBN)
        vec3 edge0 = gPosition[1] - gPosition[0];
        vec3 edge1 = gPosition[2] - gPosition[0];
        vec2 deltaUV0 = gTexCoord[1] - gTexCoord[0];
        vec2 deltaUV1 = gTexCoord[2] - gTexCoord[0];
        float invDet = 1.0 / (deltaUV0.x * deltaUV1.y - deltaUV1.x * deltaUV0.y);

        vec3 tangent   = invDet * (deltaUV1.y * edge0 - deltaUV0.y * edge1);
        vec3 bitangent = invDet * (-deltaUV1.x * edge0 + deltaUV0.x * edge1);

        vec3 T = normalize(vec3(modelMatrix * vec4(tangent,   0.0)));
        vec3 N = normalize(vec3(modelMatrix * vec4(cross(edge1, edge0), 0.0)));
        T = normalize(T - dot(T, N) * N);
        // then retrieve perpendicular vector B with the cross product of T and N
        vec3 B = cross(N, T);

        mat3 TBN = transpose(mat3(T, B, N));
        vec3 lightPos_T = TBN * glightPos;
        vec3 camPos_T   = TBN * gcamPos;

        for (int i = 0; i < 3; ++i) {
            gl_Position  = gl_in[i].gl_Position;
            fragPosition = TBN * vec3(modelMatrix * vec4(gPosition[i], 1.0));
            fragNormal   = gNormal[i];
            fragTexCoord = gTexCoord[i];
            lightPos     = lightPos_T;
            camPos       = camPos_T;
            lightColor   = glightColor;
            fLightClip   = gLightClip[i];   
            EmitVertex();
            }
     }
     else
     {
        // === WORLD SPACE PATH (No Normal Mapping) ===
        
        for (int i = 0; i < 3; ++i)
        {
            gl_Position = gl_in[i].gl_Position; 
            fragPosition =  vec3 (modelMatrix * vec4(gPosition[i], 1.0)); 
            lightPos = glightPos;       
            camPos = gcamPos;           
            fragNormal = gNormal[i];    
            lightColor = glightColor;
            fragTexCoord = gTexCoord[i];
            fLightClip   = gLightClip[i];  
            EmitVertex();
        }
    }
    EndPrimitive();
}
