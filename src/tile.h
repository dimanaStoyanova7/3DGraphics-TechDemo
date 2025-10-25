#include <framework/mesh.h>

class Tile{
public:
	long id;
	// --- Cordinates ---
	glm::vec3 startPoint;
	glm::vec3 endPoint;

	// -- texture properties ---
	glm::vec3 kd; // Diffuse color
	glm::vec3 ks{ 0.0f };
	float shininess{ 1.0f };
	float transparency{ 1.0f };
	std::filesystem::path kdTexture;


	Tile(glm::vec3 start_point_, glm::vec3 end_point_);
	Tile(glm::vec3 start_point_, glm::vec3 end_point_, glm::vec3 kd_, glm::vec3 ks_, float shininess_, float transparency_);
	void setMaterial(glm::vec3 kd_, glm::vec3 ks_, float shininess_, float transparency_);
	
	void setNeighbor(int direction, long neighbor_id);
	
	// given tile information generate mesh
	Mesh generateMesh();

	// to locate the tile later once in GPUMESH
	void setMeshID(long id_) { mesh_id = id_; }
	long getMeshID() const { return mesh_id; }

	// return a translation vector relative to tile position
	glm::vec3 positionInTile(float x, float z);

private:
	static long next_id;

	// to be used later mesh_id of the tile , ids of meshes inside of the tile, and neighbring tiles 
	// example usage: delition of all objects on 1 tile
	long mesh_id;
	std::vector<long> meshes_ids; 
	long neighbors[4];
};