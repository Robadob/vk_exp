#ifndef __GraphicsPipeline_h__
#define __GraphicsPipeline_h__
#include <vector>
#include <vulkan/vulkan.hpp>

class Context;

class GraphicsPipeline
{
public:
	GraphicsPipeline(Context &ctx, const char * vertPath, const char * fragPath);
	~GraphicsPipeline();
private:
	static std::vector<char> readFile(const char * file);
	vk::ShaderModule createShader(const std::vector<char>& code) const;
	static std::vector<vk::PipelineShaderStageCreateInfo> createPipelineInfo(vk::ShaderModule &v, vk::ShaderModule &f);
	const Context &m_context;


	vk::PipelineVertexInputStateCreateInfo vertexInput() const ;
	vk::PipelineInputAssemblyStateCreateInfo inputAssembly() const;
	vk::PipelineViewportStateCreateInfo viewportState() const;
	vk::PipelineRasterizationStateCreateInfo rasterizerState() const;
	vk::PipelineMultisampleStateCreateInfo multisampleState() const;
	vk::PipelineColorBlendStateCreateInfo colorBlendState() const;
	vk::PipelineLayout pipelineLayout() const;
	vk::RenderPass renderPass() const;

	vk::Pipeline m_pipeline;
	vk::PipelineLayout m_pipelineLayout;
	vk::RenderPass m_renderPass;
};

#endif //__GraphicsPipeline_h__