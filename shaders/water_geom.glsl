#version 410 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

// ===== inputs from vertex shader (per vertex) =====
in vec3 gPosition[];
in vec3 gNormal[];
in vec2 gTexCoord[];

// ===== Uniforms (single value per draw call =====    
uniform vec3 glightPos;   
uniform vec3 gcamPos;     
uniform vec3 gcolor;      
uniform bool nm;

// ===== outputs to fragment shader =====
out vec3 fragPosition;
out vec3 fragNormal;
out vec2 fragTexCoord;
out vec3 lightPos;      
out vec3 camPos;
out vec3 lightColor;      

void main()
{
    if(nm){
        
        vec3 edge0 = gPosition[1] - gPosition[0];
        vec3 edge1 = gPosition[2] - gPosition[0];
        vec2 deltaUV0 = gTexCoord[1] - gTexCoord[0];
        vec2 deltaUV1 = gTexCoord[2] - gTexCoord[0];

        float invDet = 1.0f / (deltaUV0.x * deltaUV1.y - deltaUV1.x * deltaUV0.y);

        vec3 tangent = vec3(invDet * (deltaUV1.y * edge0 - deltaUV0.y * edge1));
        vec3 bitangent = vec3(invDet * (-deltaUV1.x * edge0 + deltaUV0.x * edge1));

        vec3 T = normalize(tangent);
        vec3 B = normalize(bitangent);
        vec3 N = normalize(cross(edge1, edge0));

        mat3 TBN = mat3(T, B, N);
        TBN = transpose(TBN); // TBN is an orthogonal matrix

       vec3 lightPos_T = TBN * glightPos;
       vec3 camPos_T   = TBN * gcamPos;


        // === VERTEX EMISSION LOOP ===
       for (int i = 0; i < 3; ++i){

            gl_Position = gl_in[i].gl_Position; 

            // Transform World Space position to TANGENT SPACE
            fragPosition = TBN * gPosition[i];
    
            // Pass other data per vertex
            fragNormal = gNormal[i];
            fragTexCoord = gTexCoord[i];
    
            // Pass the TBN-transformed uniform data
            lightPos = lightPos_T;
            camPos = camPos_T;
            lightColor = gcolor; 

            EmitVertex();
            }
     }
     else
     {
        // === WORLD SPACE PATH (No Normal Mapping) ===
        
        for (int i = 0; i < 3; ++i)
        {
            
            gl_Position = gl_in[i].gl_Position; 
            fragPosition = gPosition[i]; // World Space Position
            lightPos = glightPos;       // World Space Light Position
            camPos = gcamPos;           // World Space Camera Position

            // Pass other data
            fragNormal = gNormal[i];    // World Space Normal
            lightColor = gcolor;
            fragTexCoord = gTexCoord[i];
            
            EmitVertex();
        }
    }
    EndPrimitive();
}