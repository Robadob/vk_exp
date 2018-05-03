#include "ModelNode.h"
#include "Model.h"

void ModelNode::render(vk::CommandBuffer &cb, vk::PipelineLayout &layout, glm::mat4 transform)
{
    //Calculate & apply transform
    transform = data->transforms[transformOffset] * transform;
    //transform = transform * data->transforms[transformOffset];

    //printf("MC: %d, CC: %d\n", meshes.size(), children.size());
    //Render all meshes
    for (auto &&mesh : meshes)
    {
        mesh->render(cb, layout, transform);
    }
    //Recursively render all children
    for (auto &&child : children)
    {
        child->render(cb, layout, transform);
    }
}

BoundingBox3D ModelNode::calculateBoundingBox(glm::mat4 transform)
{
    BoundingBox3D rtn;
    //Calculate & apply transform
    transform = data->transforms[transformOffset] * transform; 
    //transform = transform * data->transforms[transformOffset];

    //Include all meshes
    for (auto &&mesh : meshes)
    {
        rtn.include(mesh->calculateBoundingBox(transform));
    }

    //Recursively include all children
    for (auto &&child : children)
    {
        rtn.include(child->calculateBoundingBox(transform));
    }
    return rtn;
}