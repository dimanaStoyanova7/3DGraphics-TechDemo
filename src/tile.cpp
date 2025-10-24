#include "tile.h"
#include <glm/glm.hpp>
#include <algorithm> 
//#include <iostream>
#include <glm/gtc/matrix_transform.hpp>


long Tile::next_id = 0;



Tile::Tile(glm::vec3 start_point_, glm::vec3 end_point_)
    : id(next_id++),
    startPoint(start_point_),
    endPoint(end_point_),

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
        return;
    }
    neighbors[direction] = neighbor_id;
}

Mesh Tile::generateMesh() {
    Mesh mesh;

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

    glm::vec2 texCord0(0.0, 0.0);
    glm::vec2 texCord1(0.0, 1.0);
    glm::vec2 texCord2(1.0, 1.0);
    glm::vec2 texCord3(1.0, 0.0);


    Vertex v0, v1, v2, v3;
    v0.position = p0; v0.normal = normal; v0.texCoord = texCord0;
    v1.position = p1; v1.normal = normal; v1.texCoord = texCord1;
    v2.position = p2; v2.normal = normal; v2.texCoord = texCord2;
    v3.position = p3; v3.normal = normal; v3.texCoord = texCord3;

    std::vector<Vertex>& vertices = mesh.vertices;
    vertices.push_back(v0);
    vertices.push_back(v1);
    vertices.push_back(v2);
    vertices.push_back(v3);

    // Winding: counter-clockwise when looking from +Y
    mesh.triangles.push_back(glm::uvec3(2, 1, 0));
    mesh.triangles.push_back(glm::uvec3(3, 2, 0));

    mesh.material.kd = kd;
    mesh.material.ks = ks;
    mesh.material.shininess = shininess;
    mesh.material.transparency = transparency;
    if (!kdTexture.empty()) mesh.material.kdTexture = kdTexture;
    else {
        mesh.material.kdTexture = RESOURCE_ROOT "resources/tileMetal.png";
    }

    return mesh;
}


glm::vec3 Tile::positionInTile(float x, float z) {
    if (x > 1 || x < 0 || z > 1 || z < 0)return startPoint;
    glm::mat4 transformation = glm::mat4(1.0);
    glm::vec3 tilePostion =startPoint + x * (glm::vec3(endPoint.x - startPoint.x, 0.0, 0.0)) + z * (glm::vec3(0.0, 0.0, endPoint.z - startPoint.z));

    return tilePostion;
}


