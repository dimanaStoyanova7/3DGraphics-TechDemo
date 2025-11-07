#include "mesh.h"
#include "texture.h"
#include <framework/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <fmt/format.h>
DISABLE_WARNINGS_POP()
#include <iostream>
#include <vector>
#include <iostream>


long GPUMesh::next_id = 0;

GPUMaterial::GPUMaterial(const Material& material) :
    kd(material.kd),
    ks(material.ks),
    shininess(material.shininess),
    transparency(material.transparency),
    roughness(material.roughness),
    metallic(material.metallic),
    ao(material.ao)
	//kdTexture(material.kdTexture)
{}

GPUMesh::GPUMesh(const Mesh& cpuMesh, bool isMovable)
{
    m_id = next_id++;
    m_isMovable = isMovable;
    // Create uniform buffer to store mesh material (https://learnopengl.com/Advanced-OpenGL/Advanced-GLSL)
    GPUMaterial gpuMaterial(cpuMesh.material);
    glGenBuffers(1, &m_uboMaterial);
    glBindBuffer(GL_UNIFORM_BUFFER, m_uboMaterial);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(GPUMaterial), &gpuMaterial, GL_STATIC_READ);

    // Figure out if this mesh has texture coordinates
    //m_hasTextureCoords = static_cast<bool>(cpuMesh.material.kdTexture);
    m_hasTextureCoords = !cpuMesh.material.kdTexture.empty();
    if (m_hasTextureCoords) {
         //std::cout << "Loading diffuse texture for mesh: " << cpuMesh.material.kdTexture.generic_string() << std::endl;
        texturePath = cpuMesh.material.kdTexture.generic_string();
    }

    m_hasAmbientTexture = !cpuMesh.material.ambientTexture.empty();
    if (m_hasAmbientTexture) {
         //std::cout << "Loading ambient texture for mesh: " << cpuMesh.material.ambientTexture.generic_string() << std::endl;
        ambientTexture = cpuMesh.material.ambientTexture.generic_string();
    }

    m_hasMetalnessTexture = !cpuMesh.material.metalnessTexture.empty();
    if (m_hasMetalnessTexture) {
         //std::cout << "Loading metalness texture for mesh: " << cpuMesh.material.metalnessTexture.generic_string() << std::endl;
        metalnessTexture = cpuMesh.material.metalnessTexture.generic_string();
    }

    m_hasRoughnessTexture = !cpuMesh.material.roughnessTexture.empty();
    if (m_hasRoughnessTexture) {
         //std::cout << "Loading roughness texture for mesh: " << cpuMesh.material.roughnessTexture.generic_string() << std::endl;
        roughnessTexture = cpuMesh.material.roughnessTexture.generic_string();
    }

    m_hasNormalMap = !cpuMesh.material.normalMap.empty();
    if (m_hasNormalMap) {
         //std::cout << "Loading normal map for mesh: " << cpuMesh.material.normalMap.generic_string() << std::endl;
        normalMap = cpuMesh.material.normalMap.generic_string();
    }


    // Create VAO and bind it so subsequent creations of VBO and IBO are bound to this VAO
    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    // Create vertex buffer object (VBO)
    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(cpuMesh.vertices.size() * sizeof(decltype(cpuMesh.vertices)::value_type)), cpuMesh.vertices.data(), GL_STATIC_DRAW);

    // Create index buffer object (IBO)
    glGenBuffers(1, &m_ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(cpuMesh.triangles.size() * sizeof(decltype(cpuMesh.triangles)::value_type)), cpuMesh.triangles.data(), GL_STATIC_DRAW);

    // Tell OpenGL that we will be using vertex attributes 0, 1 and 2.
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    // We tell OpenGL what each vertex looks like and how they are mapped to the shader (location = ...).
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoord));
    // Reuse all attributes for each instance
    glVertexAttribDivisor(0, 0);
    glVertexAttribDivisor(1, 0);
    glVertexAttribDivisor(2, 0);

    // Each triangle has 3 vertices.
    m_numIndices = static_cast<GLsizei>(3 * cpuMesh.triangles.size());
}


GPUMesh::GPUMesh(const Mesh& cpuMesh, const glm::mat4& transform, bool isMovable)
    : GPUMesh(applyTransform(cpuMesh, transform), isMovable) // delegates properly
{
}

Mesh GPUMesh::applyTransform(const Mesh& mesh, const glm::mat4& transform) {
    Mesh result = mesh; // make a copy
    for (Vertex& v : result.vertices) {
        v.position = glm::vec3(transform * glm::vec4(v.position, 1.0f));
        v.normal = glm::mat3(glm::transpose(glm::inverse(transform))) * v.normal;
    }
    return result;
}


GPUMesh::GPUMesh(GPUMesh&& other)
{
    moveInto(std::move(other));
}

GPUMesh::~GPUMesh()
{
    freeGpuMemory();
}

GPUMesh& GPUMesh::operator=(GPUMesh&& other)
{
    moveInto(std::move(other));
    return *this;
}

std::vector<GPUMesh> GPUMesh::loadMeshGPU(std::filesystem::path filePath, bool normalize, bool isMovable) {
    if (!std::filesystem::exists(filePath))
        throw MeshLoadingException(fmt::format("File {} does not exist", filePath.string().c_str()));

    // Generate GPU-side meshes for all sub-meshes
    std::vector<Mesh> subMeshes = loadMesh(filePath, { .normalizeVertexPositions = normalize });

    std::vector<GPUMesh> gpuMeshes;
    for (const Mesh& mesh : subMeshes) { gpuMeshes.emplace_back(mesh, isMovable); }
    
    return gpuMeshes;
}

std::vector<GPUMesh> GPUMesh::loadMeshGPU( glm::mat4& transform, std::filesystem::path filePath, bool normalize, bool isMovable) {
    if (!std::filesystem::exists(filePath))
        throw MeshLoadingException(fmt::format("File {} does not exist", filePath.string().c_str()));

    // Generate GPU-side meshes for all sub-meshes
    std::vector<Mesh> subMeshes = loadMesh(filePath, { .normalizeVertexPositions = normalize });

    std::vector<GPUMesh> gpuMeshes;
    for (const Mesh& mesh : subMeshes) { gpuMeshes.emplace_back(mesh, transform, isMovable); }

    return gpuMeshes;
}


bool GPUMesh::hasTextureCoords() const
{
    return m_hasTextureCoords;
}

void GPUMesh::draw(const Shader& drawingShader)
{
    // Bind material data uniform (we assume that the uniform buffer objects is always called 'Material')
    // Yes, we could define the binding inside the shader itself, but that would break on OpenGL versions below 4.2
    drawingShader.bindUniformBlock("Material", 0, m_uboMaterial);
    
    // Draw the mesh's triangles
    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, m_numIndices, GL_UNSIGNED_INT, nullptr);
}

#include <utility> // std::move

void GPUMesh::moveInto(GPUMesh&& other)
{
    // If this already owns GPU objects, free them first.
    freeGpuMemory();

    // --- move POD/handles ---
    m_numIndices = other.m_numIndices;
    m_hasTextureCoords = other.m_hasTextureCoords;
    m_hasAmbientTexture = other.m_hasAmbientTexture;
    m_hasMetalnessTexture = other.m_hasMetalnessTexture;
    m_hasRoughnessTexture = other.m_hasRoughnessTexture;
    m_hasNormalMap = other.m_hasNormalMap;

    m_ibo = other.m_ibo;
    m_vbo = other.m_vbo;
    m_vao = other.m_vao;
    m_uboMaterial = other.m_uboMaterial;

    m_id = other.m_id;
    m_isMovable = other.m_isMovable;

    // --- move strings/paths ---
    texturePath = std::move(other.texturePath);
    ambientTexture = std::move(other.ambientTexture);
    metalnessTexture = std::move(other.metalnessTexture);
    roughnessTexture = std::move(other.roughnessTexture);
    normalMap = std::move(other.normalMap);

    // --- reset 'other' to a null/empty state so its destructor is safe ---
    other.m_numIndices = 0;
    other.m_hasTextureCoords = false;
    other.m_hasAmbientTexture = false;
    other.m_hasMetalnessTexture = false;
    other.m_hasRoughnessTexture = false;
    other.m_hasNormalMap = false;

    other.m_ibo = INVALID;
    other.m_vbo = INVALID;
    other.m_vao = INVALID;
    other.m_uboMaterial = INVALID;

    other.m_id = -1;
    other.m_isMovable = false;

    other.texturePath.clear();
    other.ambientTexture.clear();
    other.metalnessTexture.clear();
    other.roughnessTexture.clear();
    other.normalMap.clear();
}

void GPUMesh::freeGpuMemory()
{
    if (m_vao != INVALID)
        glDeleteVertexArrays(1, &m_vao);
    if (m_vbo != INVALID)
        glDeleteBuffers(1, &m_vbo);
    if (m_ibo != INVALID)
        glDeleteBuffers(1, &m_ibo);
    if (m_uboMaterial != INVALID)
        glDeleteBuffers(1, &m_uboMaterial);
}
