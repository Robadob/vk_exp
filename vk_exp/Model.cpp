#include "Model.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include <tinyobjloader/tiny_obj_loader.h>
#include "StringUtils.h"
#include <unordered_map>
#include <glm/gtx/hash.hpp>

struct Vertex
{
	glm::vec3 v;
	glm::vec3 n;
	glm::vec2 tc;
	bool operator==(const Vertex& other) const {
		return v == other.v && n == other.n && tc == other.tc;
	}
};
namespace std {
	template<> struct hash<Vertex> {
		size_t operator()(Vertex const& vertex) const {
			return ((hash<glm::vec3>()(vertex.v) ^
				(hash<glm::vec3>()(vertex.n) << 1)) >> 1) ^
				(hash<glm::vec2>()(vertex.tc) << 1);
		}
	};
}
Model::Model(Context &ctxt, const char *objPath)
	: m_context(ctxt)
	, m_filepath(objPath)
{
	if (su::endsWith(su::toLower(m_filepath), ".obj"))
	{//Model is type .obj
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string err;

		if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, m_filepath.c_str())) {
			throw std::runtime_error(err);
		}		
		{//Deduplicate vertices
			std::unordered_map<Vertex, unsigned int> uniqueVertices;//Vertices index per model
			for (const auto& shape : shapes)
			{
				//std::unordered_map<Vertex, unsigned int> uniqueVertices;//Vertices index per shape
				unsigned int shapeVertexStart = m_data.m_vertices.size();
				unsigned int shapeIndexStart = m_data.m_indices.size();
				for (const auto& index : shape.mesh.indices) {
					Vertex v = {
									reinterpret_cast<glm::vec3*>(attrib.vertices.data())[index.vertex_index],
									reinterpret_cast<glm::vec3*>(attrib.normals.data())[index.normal_index],
									reinterpret_cast<glm::vec2*>(attrib.texcoords.data())[index.vertex_index]
					};
					if (uniqueVertices.count(v) == 0)
					{
						uniqueVertices[v] = uniqueVertices.size();
						m_data.m_vertices.push_back(v.v);
						m_data.m_normals.push_back(v.n);
						m_data.m_texcoords.push_back(v.tc);
					}
					//m_data.m_indices.push_back(uniqueVertices[v]);//Vertices index per shape
					m_data.m_indices.push_back(m_data.m_vertices.size());//Vertices index per model
				}
				m_meshes.push_back(Mesh(m_data, m_data.m_vertices.size() - shapeVertexStart, m_data.m_indices.size() - shapeIndexStart);//Vertices index per model , shapeVertexStart, shapeIndexStart));//Vertices index per shape
			}
		}
	}
	else
	{
		throw std::runtime_error("Unsupported file type passed to Model constructor.");
	}
	//Create Vertex Buffers

	//Create Index Buffers
}
