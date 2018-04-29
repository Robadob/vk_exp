#ifndef __Model_h__
#define __Model_h__
#include "glm/glm.hpp"
#include <memory>
#include "ModelNode.h"
//#include "../shader/Shaders.h"
#include "Bone.h"
#include "Material.h"
#include "BoundingBox.h"
#include "../Buffer.h"
class GraphicsPipeline;
class Context;
struct VFCcount
{
	VFCcount(int i = 1) :v(0), f(0), c(i), b(0){}
	unsigned int v;
	unsigned int f;
	unsigned int c;
	unsigned int b;
	VFCcount &operator +=(const VFCcount &rhs)
	{
		v += rhs.v;
		f += rhs.f;
		c += rhs.c;
		b += rhs.b;
		return *this;
	}
};

struct ModelData
{
	ModelData(
		Context &context,
		size_t vertices,
		size_t normals,
		size_t colors,
		size_t texcoords,
		size_t bones,
		size_t materials,
		size_t faces,
		size_t transforms
	);
	~ModelData();
	Context &m_context;
	//Vertex attributes
	glm::vec3 *vertices;
	glm::vec3 *normals;
	glm::vec4 *colors;
	glm::vec2 *texcoords;

	//Bones
	Bone *bones;

	//Materials
    std::shared_ptr<Material> *materials;
	vk::DescriptorPool m_descriptorPool = nullptr;
	std::vector<vk::DescriptorSet> m_descriptorSets;

	//Component attributes
	unsigned int *faces;
    glm::mat4 *transforms;

    //Sizes
    size_t verticesSize;
    size_t normalsSize;
    size_t colorsSize;
    size_t texcoordsSize;
    size_t bonesSize;
    size_t materialsSize;
    size_t facesSize;
    size_t transformsSize;
};
class Model// : public Reloadable, public HasMatrices
{
public:
	/**
	*
	*/
    Model(Context &context, const char *modelPath, float scale = -1.0f);
	~Model();
	/**
	* Reloads the model from file, rewriting GPU buffers
	*/
	void reload();
	/**
	 * Render to the specific commandbuffer with the named pipeline
	 */
    void render(vk::CommandBuffer &cb, GraphicsPipeline &pipeline);

    void setLocation(glm::vec3 location){ this->location = location; }
    void setRotation(glm::vec4 rotation){ this->rotation = rotation; }
    glm::vec3 getLocation() const{ return location; }
    glm::vec4 getRotation() const{ return rotation; }
    BoundingBox3D getBoundingBox() const { return boundingBox; }
private:
    void updateBoundingBox();
	std::shared_ptr<ModelNode> buildHierarchy(const struct aiScene* scene, const struct aiNode* nd, VFCcount &vfc, std::string* t_boneNames=nullptr) const;
	unsigned int linkBones(std::shared_ptr<ModelNode> node, std::shared_ptr<ModelData> data, const std::string * const boneNames);
	void loadModel();
	void freeModel();

	std::vector<vk::VertexInputAttributeDescription> Model::AttributeDesc() const;
	vk::VertexInputBindingDescription Model::BindingDesc() const;

    BoundingBox3D boundingBox;

	std::shared_ptr<ModelNode> root;
	std::shared_ptr<ModelData> data;
	VFCcount vfc;
	const char *modelPath;
    const float loadScale;//Scale that vertices are scaled to at model load
	//VBOs
	VertexBuffer *m_vertexBuffer;
	IndexBuffer *m_indexBuffer;
	glm::vec3 location;
	glm::vec4 rotation;
	Context &m_context;


	///**
	//* Holds information for binding the vertex positions attribute
	//*/
	//Shaders::VertexAttributeDetail positions;
	///**
	//* Holds information for binding the vertex normals attribute
	//*/
	//Shaders::VertexAttributeDetail normals;
	///**
	//* Holds information for binding the vertex colours attribute
	//*/
	//Shaders::VertexAttributeDetail colors;
	///**
	//* Holds information for binding the vertex texture coordinates attribute
	//*/
	//Shaders::VertexAttributeDetail texcoords;
	//HasMatrices overrides
public:
	/**
	* Sets the pointer from which the View matrix should be loaded from
	* @param viewMat A pointer to the viewMatrix to be tracked
	* @note This pointer is likely provided by a Camera subclass
	*/
	virtual void setViewMatPtr(const glm::mat4  *viewMat);
	/**
	* Sets the pointer from which the Projection matrix should be loaded from
	* @param projectionMat A pointer to the projectionMatrix to be tracked
	* @note This pointer is likely provided by the Visualisation object
	*/
	virtual void setProjectionMatPtr(const glm::mat4 *projectionMat);
	/**
	* Sets the pointer from which the Model matrix should be loaded from
	* @param modelMat A pointer to the modelMatrix to be tracked
	* @note This pointer is likely provided by the Visualisation object
	*/
	void setModelMatPtr(const glm::mat4 *modelMat);
	/**
	* Overrides the model matrix (and all dependent matrices) until useProgram() is next called
	* @param modelMat Pointer to the overriding modelMat
	*/
	virtual void overrideModelMat(const glm::mat4 *modelMat);
};

#endif //__Model_h__