#include "water.h"
#include <glm/glm.hpp>

// Constructor
Water::Water(int cols_, int rows_, float sizeX_, float sizeZ_)
    : cols(cols_), rows(rows_), sizeX(sizeX_), sizeZ(sizeZ_)
{
    watter_grid = generateMesh();

    // Assign material info
    watter_grid.material.kd = kd;
    watter_grid.material.ks = ks;
    watter_grid.material.shininess = shininess;
    watter_grid.material.transparency = transparency;
    watter_grid.material.kdTexture = texture;
    watter_grid.material.normalMap = normalMap;
}

Mesh Water::generateMesh() {
    Mesh mesh;
    mesh.vertices.reserve(cols * rows);

    const float dx = (cols > 1) ? sizeX / (cols - 1) : 0.0f;
    const float dz = (rows > 1) ? sizeZ / (rows - 1) : 0.0f;
    const float x0 = -sizeX * 0.5f;
    const float z0 = -sizeZ * 0.5f;

    // === Create vertices ===
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            Vertex v;
            v.position = glm::vec3(x0 + c * dx, y, z0 + r * dz);  // flat at height y
            v.normal = glm::vec3(0.0f, 1.0f, 0.0f);               // up normal
            v.texCoord = glm::vec2(
                (cols > 1) ? float(c) / float(cols - 1) : 0.0f,
                (rows > 1) ? float(r) / float(rows - 1) : 0.0f
            );
            mesh.vertices.push_back(v);
        }
    }

    // === Create triangles (two per quad) ===
    for (int r = 0; r < rows - 1; ++r) {
        for (int c = 0; c < cols - 1; ++c) {
            uint32_t i0 = r * cols + c;
            uint32_t i1 = r * cols + (c + 1);
            uint32_t i2 = (r + 1) * cols + c;
            uint32_t i3 = (r + 1) * cols + (c + 1);

            mesh.triangles.push_back(glm::uvec3(i0, i2, i1));  // first triangle
            mesh.triangles.push_back(glm::uvec3(i1, i2, i3));  // second triangle
        }
    }


    //setup material
    mesh.material.kdTexture = texture;
    mesh.material.normalMap = normalMap;
    mesh.material.kd  = kd;
    mesh.material.ks = ks;
    mesh.material.shininess = shininess;
    mesh.material.transparency = transparency;

    return mesh;
}
