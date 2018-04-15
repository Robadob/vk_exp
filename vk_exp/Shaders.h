#ifndef __Shaders_h__
#define __Shaders_h__
#include <vector>
#include <vulkan/vulkan.hpp>
class Context;


class Shaders
{
public:
	Shaders(Context &ctxt, const char* vPath, const char* fPath);
	~Shaders();
private:
	/*
	 * Load spir-v shader blob from file
	 */
	static std::vector<char> readFile(const char *file);
	/**
	 * Create vulkan shader module obj from spir-v shader blob
	 */
	vk::ShaderModule createShaderModule(const std::vector<char>& code) const;
	/*
	 * Define the pipeline shader stage creation details
	 */
	std::vector<vk::PipelineShaderStageCreateInfo> createPipelineInfo(const vk::ShaderModule &v, const vk::ShaderModule &f) const;
	/**
	 * Define the vertex attribute details
	 * @note Use SPIR-V Cross reflection to perform this dynamically?
	 */
	vk::PipelineVertexInputStateCreateInfo vertexInput();
	/**
	 * Do something with descriptor sets for uniforms
	 * @note Use SPIR-V Cross reflection to perform this dynamically?
	 */
	void descriptorSets();
	/**
	 * Actually set the uniforms within the shaders
	 */
	void Shaders::bindUniforms();
	Context &m_context;
	std::vector<vk::PipelineShaderStageCreateInfo> m_pipelineShaderStageCreate;
	vk::PipelineVertexInputStateCreateInfo m_vertexInputStateCreate;
	vk::DescriptorSetLayout m_descriptorSetLayout = nullptr;
	vk::DescriptorPool m_descriptorPool = nullptr;
	vk::DescriptorSet m_descriptorSet = nullptr;
};
#endif //__Shaders_h__
