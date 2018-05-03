#ifndef __Mesh_h__
#define __Mesh_h__
#include <memory>
#include "BoundingBox.h"
#include <vulkan/vulkan.hpp>

struct ModelData;
class ModelNode;

/**
* Renderable Mesh of a component
*/
class Mesh
{
    friend class ModelNode;
public:
    static std::shared_ptr<Mesh> make_shared(std::shared_ptr<ModelData> data, unsigned int fo, unsigned int fc, unsigned int mi, vk::PrimitiveTopology ft)
    {
        std::shared_ptr<Mesh> rtn = std::shared_ptr<Mesh>(new Mesh(data, fo, fc, mi, ft));
        rtn->me = rtn;
        return rtn;
    }
    void render(vk::CommandBuffer &cb, vk::PipelineLayout &layout, glm::mat4 &transform) const;
    BoundingBox3D calculateBoundingBox(glm::mat4 transform);
private:
    std::weak_ptr<Mesh> me;
    std::shared_ptr<Mesh> shared_ptr() const
    {
        return me.lock();
    }
    Mesh(std::shared_ptr<ModelData> data, unsigned int ic, unsigned int io, unsigned int mi, vk::PrimitiveTopology ft)
        : indexCount(ic)
        , indexOffset(io)
        , materialIndex(mi)
        , faceType(ft)
        , data(data)
    {
        if (faceType != vk::PrimitiveTopology::eTriangleList)
            throw std::runtime_error("Only Triangle strip meshes are currently supported.\nAlterantiv primitive topologies require unique pipelines.");
    }
    void setParent(std::shared_ptr<ModelNode> parent)
    {
        this->parent = parent;
    }
private:
    unsigned int indexCount;
    unsigned int indexOffset;
    unsigned int materialIndex;

    vk::PrimitiveTopology faceType;
    std::shared_ptr<ModelData> data;
    std::weak_ptr<ModelNode> parent;
};
#endif //__Mesh_h__