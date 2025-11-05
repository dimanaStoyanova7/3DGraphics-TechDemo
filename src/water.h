#include <framework/mesh.h>

class Water {
public:
	Mesh generateMesh();
	Water(int cols_, int rows_, float sizeX_, float sizeZ_);

	glm::vec3 kd{ 0.0f, 0.2f, 0.8f };
	glm::vec3 ks{ 0.8f };             
	float shininess{ 67.0f };
	float transparency{ 0.7f };;
	std::string texture{ RESOURCE_ROOT "resources/water/albedo.jpg"};
	std::string normalMap{ RESOURCE_ROOT "resources/water/normal.jpg" };

	Mesh getMesh() { return watter_grid;}

private:
	int cols;
	int rows;
	float sizeX;
	float sizeZ;
	int y = -10.0f;
	Mesh watter_grid;

};