#include "tile.h"
#include <glm/glm.hpp>
#include <algorithm> // for std::clamp if needed

// Define the static member once in a single TU
long Tile::next_id = 0;

// Helpers (optional): interpret neighbor directions as 0:+X, 1:+Z, 2:-X, 3:-Z.
// You can change this scheme as you like.

// Constructor: just geometry; material uses defaults from header
Tile::Tile(glm::vec3 start_point_, glm::vec3 end_point_)
    : id(next_id++),
    startPoint(start_point_),
    endPoint(end_point_),
    // kd left default-initialized by header's default ctor if any; else set here:
    kd(0.7f, 0.7f, 0.7f),
    // ks/shininess/transparency already defaulted in header
    mesh_id(-1),
    meshes_ids(),
    kdTexture()  // empty path by default
{
    neighbors[0] = neighbors[1] = neighbors[2] = neighbors[3] = -1;
}

// Constructor with explicit material
Tile::Tile(glm::vec3 start_point_, glm::vec3 end_point_,
    glm::vec3 kd_, glm::vec3 ks_, float shininess_, float transparency_)
    : id(next_id++),
    startPoint(start_point_),
    endPoint(end_point_),
    kd(kd_),
    ks(ks_),
    shininess(shininess_),
    transparency(transparency_),
    mesh_id(-1),
    meshes_ids(),
    kdTexture()
{
    neighbors[0] = neighbors[1] = neighbors[2] = neighbors[3] = -1;
}

void Tile::setMaterial(glm::vec3 kd_, glm::vec3 ks_, float shininess_, float transparency_) {
    kd = kd_;
    ks = ks_;
    shininess = shininess_;
    transparency = transparency_;
}

void Tile::setNeighbor(int direction, long neighbor_id) {
    if (direction < 0 || direction >= 4) {
        // silently ignore or clamp; here we ignore invalid directions
        return;
    }
    neighbors[direction] = neighbor_id;
}

Mesh Tile::generateMesh() {
    Mesh mesh;

    // We assume a horizontal quad defined by opposite corners in XZ at a common Y.
    // If startPoint.y != endPoint.y, we use startPoint.y for the plane.
    float y = startPoint.y;

    float x0 = startPoint.x;
    float z0 = startPoint.z;
    float x1 = endPoint.x;
    float z1 = endPoint.z;

    // Build axis-aligned rectangle corners on Y = y plane
    glm::vec3 p0(x0, y, z0);
    glm::vec3 p1(x1, y, z0);
    glm::vec3 p2(x1, y, z1);
    glm::vec3 p3(x0, y, z1);

    glm::vec3 normal(0.0f, 1.0f, 0.0f);

    Vertex v0, v1, v2, v3;
    v0.position = p0; v0.normal = normal;
    v1.position = p1; v1.normal = normal;
    v2.position = p2; v2.normal = normal;
    v3.position = p3; v3.normal = normal;

    // Optional: set per-vertex color if your Vertex supports it
    // v0.color = kd; v1.color = kd; v2.color = kd; v3.color = kd;

    std::vector<Vertex>& vertices = mesh.vertices;
    vertices.push_back(v0);
    vertices.push_back(v1);
    vertices.push_back(v2);
    vertices.push_back(v3);

    // Winding: counter-clockwise when looking from +Y
    mesh.triangles.push_back(glm::uvec3(0, 1, 2));
    mesh.triangles.push_back(glm::uvec3(0, 2, 3));

    // Optional: set mesh material fields if your Mesh supports them
    mesh.material.kd = kd;
    mesh.material.ks = ks;
    mesh.material.shininess = shininess;
    mesh.material.transparency = transparency;
    if (!kdTexture.empty()) mesh.material.kdTexture = kdTexture;
    else {
        mesh.material.kdTexture = RESOURCE_ROOT "resources/tileMEtal.png";
    }

    return mesh;
}
