#ifndef __Mesh_h__
#define __Mesh_h__
#include <string>
#include <vector>
#include <glm/glm.hpp>

class Context;
struct ModelData
{
	std::vector<glm::vec3> m_vertices;
	std::vector<glm::vec3> m_normals;
	std::vector<glm::vec2> m_texcoords;
	std::vector<unsigned int> m_indices;
};
class Mesh
{
public:
	Mesh(const ModelData & modelData, unsigned int vertexCount, unsigned int IndexCount, unsigned int vertexDataOffset = 0, unsigned int indexDataOffset = 0)
		: m_data(modelData)
		, m_vertexCount(vertexCount)
		, m_vertexOffset(vertexDataOffset)
		, m_indexCount(IndexCount)
		, m_indexOffset(indexDataOffset)
		
	{
		
	}
private:
	const ModelData &m_data;
	unsigned int m_vertexCount, m_vertexOffset;
	unsigned int m_indexCount, m_indexOffset;
};
class Model
{
public:
	Model(Context &ctxt, const char *objPath);
private:
	Context &m_context;
	const std::string m_filepath;
	std::vector<Mesh> m_meshes;
	ModelData m_data;
};

#endif //__Mesh_h__