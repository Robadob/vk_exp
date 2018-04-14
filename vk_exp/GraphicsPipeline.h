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
	const vk::RenderPass& RenderPass() const { return m_renderPass;  }
	const vk::Pipeline& Pipeline() const { return m_pipeline; }
private:
	static std::vector<char> readFile(const char * file);
	vk::ShaderModule createShader(const std::vector<char>& code) const;
	static std::vector<vk::PipelineShaderStageCreateInfo> createPipelineInfo(vk::ShaderModule &v, vk::ShaderModule &f);
	Context &m_context;
	
	vk::PipelineVertexInputStateCreateInfo vertexInput() const ;
	vk::PipelineInputAssemblyStateCreateInfo inputAssembly() const;
	vk::PipelineViewportStateCreateInfo viewportState();
	vk::PipelineRasterizationStateCreateInfo rasterizerState() const;
	vk::PipelineMultisampleStateCreateInfo multisampleState() const;
	vk::PipelineColorBlendStateCreateInfo colorBlendState();
	vk::PipelineLayout pipelineLayout() const;
	vk::RenderPass renderPass() const;

	vk::Pipeline m_pipeline;
	vk::PipelineLayout m_pipelineLayout;
	vk::RenderPass m_renderPass;

	//Temp structs that need pointers passed to CreateInfo's
	vk::Viewport t_viewport;
	vk::Rect2D t_scissors;
	vk::PipelineColorBlendAttachmentState t_cbas;
};

#endif //__GraphicsPipeline_h__