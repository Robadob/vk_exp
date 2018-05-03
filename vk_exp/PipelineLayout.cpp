#include "PipelineLayout.h"
#include "Context.h"
#include <glm/glm.hpp>

PipelineLayout::PipelineLayout(Context& context)
    :m_context(context)
{
    genPushConstants();
    genDescriptorSetLayouts();
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
    {
        pipelineLayoutInfo.flags = {};
        pipelineLayoutInfo.setLayoutCount = (unsigned int)m_descriptorSetLayouts.size();
        pipelineLayoutInfo.pSetLayouts = m_descriptorSetLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount = (unsigned int)m_pushConstants.size();
        pipelineLayoutInfo.pPushConstantRanges = m_pushConstants.data();
    }
    m_pipelineLayout = m_context.Device().createPipelineLayout(pipelineLayoutInfo);
}
PipelineLayout::~PipelineLayout()
{
    for(auto &a: m_descriptorSetLayouts)
    {
        m_context.Device().destroyDescriptorSetLayout(a);
        a = nullptr;
    }
    m_context.Device().destroyPipelineLayout(m_pipelineLayout);
    m_pipelineLayout = nullptr;
}
void PipelineLayout::genPushConstants()
{
    assert(m_pushConstants.size() == 1);
    {
        m_pushConstants[0].offset = 0;//Must be multiple of 4
        m_pushConstants[0].size = sizeof(glm::mat4);//Must be multiple of 4, VkPhysicalDeviceLimits::maxPushConstantsSize minus offset
        m_pushConstants[0].stageFlags = vk::ShaderStageFlagBits::eVertex;
    }
}

void PipelineLayout::genDescriptorSetLayouts()
{
    assert(m_descriptorSetLayouts.size() == 2);
    std::array<vk::DescriptorSetLayoutBinding, 1> bindings;
    bindings[0]= { GlobalsUniformBufferBinding() };
    vk::DescriptorSetLayoutCreateInfo descSetCreateInfo;
    {
        descSetCreateInfo.bindingCount = (unsigned int)bindings.size();
        descSetCreateInfo.pBindings = bindings.data();
    }
    m_descriptorSetLayouts[0] = m_context.Device().createDescriptorSetLayout(descSetCreateInfo);
    bindings[0] = { TextureSamplerBinding() };
    m_descriptorSetLayouts[1] = m_context.Device().createDescriptorSetLayout(descSetCreateInfo);
}
vk::DescriptorSetLayoutBinding PipelineLayout::GlobalsUniformBufferBinding() const
{
    /*Uniform Buffer Descriptor Set Layout*/
    vk::DescriptorSetLayoutBinding uniformBufferLayoutBinding;
    {
        uniformBufferLayoutBinding.binding = 0;
        uniformBufferLayoutBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
        uniformBufferLayoutBinding.descriptorCount = 1;
        uniformBufferLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex;
        uniformBufferLayoutBinding.pImmutableSamplers = nullptr;
    }
    return uniformBufferLayoutBinding;
}
vk::DescriptorSetLayoutBinding PipelineLayout::TextureSamplerBinding() const
{
    /*Texture Sampler Descriptor Set Layout*/
    vk::DescriptorSetLayoutBinding textureSamplerLayoutBinding;
    {
        textureSamplerLayoutBinding.binding = 0;
        textureSamplerLayoutBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        textureSamplerLayoutBinding.descriptorCount = 1;
        textureSamplerLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;
        textureSamplerLayoutBinding.pImmutableSamplers = nullptr;
    }
    return textureSamplerLayoutBinding;
}