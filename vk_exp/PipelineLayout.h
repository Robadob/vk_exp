#ifndef __PipelineLayout_h__
#define __PipelineLayout_h__
#include <array>
#include <vulkan/vulkan.hpp>
class Context;
/*
 * Wrapper class for setting up layout of uniforms within shaders
 * Current version uses a fixed layout, eventually we will want to replace this with Spirv-cross?
 */
class PipelineLayout
{
	Context &m_context;
	//Push constants are updated during the command buffer execution
	void genPushConstants();
	std::array<vk::PushConstantRange, 1> m_pushConstants;
	//Descriptor sets are updated between command buffer executions, bound after pipeline
	void genDescriptorSetLayouts();
	std::array<vk::DescriptorSetLayout, 1> m_descriptorSetLayouts;
	vk::PipelineLayout m_pipelineLayout;
public:
	PipelineLayout(Context &context);
	~PipelineLayout();
	vk::PipelineLayout get() { return m_pipelineLayout; }
	unsigned int pushConstantRangesSize() const { return (unsigned int)m_pushConstants.size(); }
	unsigned int descriptorSetLayoutsSize() const { return (unsigned int)m_descriptorSetLayouts.size(); }
	const vk::PushConstantRange *pushConstantRanges() const { return m_pushConstants.data(); }
	const vk::DescriptorSetLayout *descriptorSetLayouts() const { return m_descriptorSetLayouts.data(); }		
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
