#include "Mesh.h"
#include "Model.h"

void Mesh::render(vk::CommandBuffer &cb, vk::PipelineLayout &layout, glm::mat4 &transform) const
{
    //data->materials[materialIndex]->use(transform);
	//Render
	cb.pushConstants(layout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4), &transform);
	cb.drawIndexed(indexCount, 1, indexOffset, 0, 0);
}
/**
 * This fails to use model transform?
 */
BoundingBox3D Mesh::calculateBoundingBox(glm::mat4 transform)
{
    BoundingBox3D rtn;
    for (unsigned int i = 0; i < indexCount; ++i)
    {
        rtn.include(data->vertices[data->faces[indexOffset + i]]);
    }
    return rtn;
}