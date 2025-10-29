#pragma once

#include <framework/disable_all_warnings.h>
#include <framework/mesh.h>
#include <framework/shader.h>
DISABLE_WARNINGS_PUSH()
#include <glm/vec3.hpp>
DISABLE_WARNINGS_POP()

#include <exception>
#include <filesystem>
#include <framework/opengl_includes.h>
#include "texture.h"

struct MeshLoadingException : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Alignment directives are to comply with std140 alignment requirements (https://www.khronos.org/opengl/wiki/Interface_Block_(GLSL)#Memory_layout)
struct GPUMaterial {
    GPUMaterial(const Material& material);

    alignas(16) glm::vec3 kd{ 1.0f };
	alignas(16) glm::vec3 ks{ 0.0f };
	float shininess{ 1.0f };
	float transparency{ 1.0f };

    // Optional texture that replaces kd; use as follows:
    // 
    // if (material.kdTexture) {
    //   material.kdTexture->getTexel(...);
    // }
    //std::shared_ptr<Image> kdTexture;
};

class GPUMesh {
public:
    GPUMesh(const Mesh& cpuMesh, bool isMovable = false);
    // construct with transformation
    GPUMesh(const Mesh& cpuMesh, const glm::mat4& transform, bool isMovable = false);
    // Cannot copy a GPU mesh because it would require reference counting of GPU resources.
    GPUMesh(const GPUMesh&) = delete;
    GPUMesh(GPUMesh&&);
    ~GPUMesh();

    // Generate a number of GPU meshes from a particular model file.
    // Multiple meshes may be generated if there are multiple sub-meshes in the file
    static std::vector<GPUMesh> loadMeshGPU(std::filesystem::path filePath, bool normalize = false, bool isMovable = false);

    // load with transformation
    static std::vector<GPUMesh> loadMeshGPU(glm::mat4& transform, std::filesystem::path filePath, bool normalize = false, bool isMovable = false);

    // Cannot copy a GPU mesh because it would require reference counting of GPU resources.
    GPUMesh& operator=(const GPUMesh&) = delete;
    GPUMesh& operator=(GPUMesh&&);

    bool hasTextureCoords() const;
    std::string texturePath;
    void setMeshID(long id_) { m_id = id_; }
    long getMeshID() const { return m_id; }

    void setIsMovable(bool newValue) { m_isMovable = newValue; }
    bool getIsMovable() { return m_isMovable; }
    
    // Bind VAO and call glDrawElements.
    void draw(const Shader& drawingShader);
    

    
    GLsizei m_numIndices{ 0 };

private:
    void moveInto(GPUMesh&&);
    void freeGpuMemory();

private:
    static constexpr GLuint INVALID = 0xFFFFFFFF;
    
    // applies a transformation to cpu mesh
    Mesh applyTransform(const Mesh& mesh, const glm::mat4& transform);
    static long next_id;

    long m_id;
    bool m_isMovable{ false };
    bool m_hasTextureCoords { false };
    GLuint m_ibo { INVALID };
    GLuint m_vbo { INVALID };
    GLuint m_vao { INVALID };
    GLuint m_uboMaterial { INVALID };
};
