#ifndef __PipelineLayout_h__
#define __PipelineLayout_h__
#include <array>
#include <vulkan/vulkan.hpp>
class Context;
/*
 * Wrapper class for setting up layout of uniforms within shaders
 * Current version uses a fixed layout, eventually we will want to replace this with Spirv-cross?
 * @note https://developer.nvidia.com/vulkan-shader-resource-binding
 * Generally assumed that bindings will be in order of frequency:
 * 0: Global/Scene bindings: Updated once per frame. e.g. camera, lighting matrices
 * 1: Shader bindings: Updated once per pipeline
 * 2: Material bindings: Updated per Material
 * 3: Object transforms: Updated per Object
 * We dont currently need shader bindings, we would presumably use Specialization constants for this
 * We choose to do object transforms via push constants
 */
class PipelineLayout
{
	Context &m_context;
	//Push constants are updated during the command buffer execution
	void genPushConstants();
	std::array<vk::PushConstantRange, 1> m_pushConstants;
	//Descriptor sets are updated between command buffer executions, bound after pipeline
	void genDescriptorSetLayouts();
	std::array<vk::DescriptorSetLayout, 2> m_descriptorSetLayouts;//0:globals, 1:sampler
	vk::PipelineLayout m_pipelineLayout;
	vk::DescriptorSetLayoutBinding GlobalsUniformBufferBinding() const;
	vk::DescriptorSetLayoutBinding TextureSamplerBinding() const;
public:
	PipelineLayout(Context &context);
	~PipelineLayout();
	vk::PipelineLayout get() { return m_pipelineLayout; }
	unsigned int pushConstantRangesSize() const { return (unsigned int)m_pushConstants.size(); }
	unsigned int descriptorSetLayoutsSize() const { return (unsigned int)m_descriptorSetLayouts.size(); }
	const vk::PushConstantRange *pushConstantRanges() const { return m_pushConstants.data(); }
	const std::array<vk::DescriptorSetLayout, 2> descriptorSetLayouts() const { return m_descriptorSetLayouts; }
	const vk::DescriptorSetLayout *globalsUniformBufferSetLayout() const { return &m_descriptorSetLayouts[0]; }
	const vk::DescriptorSetLayout *textureSamplerSetLayout() const { return &m_descriptorSetLayouts[1]; }
};
//class PipelineLayoutFactory
//{
//	Context &m_context;
//public:
//	PipelineLayoutFactory(Context &context);
//	
//	PipelineLayout make();
//};

#endif //__PipelineLayout_h__
