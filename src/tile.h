#include <framework/mesh.h>

class Tile{
public:
	long id;
	glm::vec3 startPoint;
	glm::vec3 endPoint;
	glm::vec3 kd; // Diffuse color
	glm::vec3 ks{ 0.0f };
	float shininess{ 1.0f };
	float transparency{ 1.0f };

	// Optional texture that replaces kd; use as follows:
	// 
	// if (material.kdTexture) {
	//   material.kdTexture->getTexel(...);
	// }
	//std::shared_ptr<Image> kdTexture;

	std::filesystem::path kdTexture;
	Tile(glm::vec3 start_point_, glm::vec3 end_point_);
	Tile(glm::vec3 start_point_, glm::vec3 end_point_, glm::vec3 kd_, glm::vec3 ks_, float shininess_, float transparency_);
	void setMaterial(glm::vec3 kd_, glm::vec3 ks_, float shininess_, float transparency_);
	void setNeighbor(int direction, long neighbor_id);
	Mesh generateMesh();
	void setMeshID(long id_) { mesh_id = id_; }
	long getMeshID() const { return mesh_id; }
private:
	static long next_id;
	long mesh_id;
	std::vector<long> meshes_ids; 
	long neighbors[4];
};