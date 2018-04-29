#ifndef __ModelNode_h__
#define __ModelNode_h__
#include <vector>
#include <memory>
#include "Mesh.h"
#include "BoundingBox.h"
#include "Bone.h"

struct ModelData;
class ModelNode;

/**
* Hierarchical component of a model
*/
class ModelNode
{
public:
	static std::shared_ptr<ModelNode> make_shared(std::shared_ptr<ModelData> data, unsigned int to, const char * name)
	{
		std::shared_ptr<ModelNode> rtn = std::shared_ptr<ModelNode>(new ModelNode(data, to, name));
		rtn->me = rtn;
		return rtn;
	}

private:
	std::weak_ptr<ModelNode> me;
	std::shared_ptr<ModelNode> shared_ptr() const
	{
		return me.lock();
	}
	ModelNode(std::shared_ptr<ModelData> data, unsigned int to, const char *name)
		: transformOffset(to)
		, boneOffset(UINT_MAX)
		, name(name)
		, data(data)
	{ }
public:
	void setBoneOffset(unsigned int bo){ boneOffset = bo; }
	BoundingBox3D calculateBoundingBox(glm::mat4 transform = glm::mat4());
	void render(vk::CommandBuffer &cb, vk::PipelineLayout &layout, glm::mat4 transform);
	void addChild(std::shared_ptr<ModelNode> child)
	{
		this->children.push_back(child);
		child->setParent(me.lock());
	}
	void addMesh(std::shared_ptr<Mesh> mesh)
	{
		this->meshes.push_back(mesh);
		mesh->setParent(me.lock());
	}
	void setParent(std::shared_ptr<ModelNode> parent)
	{
		this->parent = parent;
	}
	std::string getName() const { return name; }

	const std::vector<std::shared_ptr<ModelNode>> *getChildren() const {return &children;};
private:
	unsigned int transformOffset;
	unsigned int boneOffset;
	//name is how we match bones to the hierarchy
	std::string name;
	std::weak_ptr<ModelNode> parent;
	std::shared_ptr<ModelData> data;
	std::vector<std::shared_ptr<ModelNode>> children;
	std::vector<std::shared_ptr<Mesh>> meshes;
};

#endif