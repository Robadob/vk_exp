#ifndef __GraphicsPipeline_h__
#define __GraphicsPipeline_h__
#include <vector>
#include <vulkan/vulkan.hpp>
class Context;

#include <glm/glm.hpp>
struct Vertex {
	glm::vec2 pos;
	glm::vec3 color;
	static vk::VertexInputBindingDescription getBindingDesc()
	{
		vk::VertexInputBindingDescription rtn;
		{
			rtn.binding = 0;
			rtn.stride = sizeof(Vertex);
			rtn.inputRate = vk::VertexInputRate::eVertex; //1 item per vertex (alt is per instance)
		}
		return rtn;
	}
	static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDesc()
	{
		std::array<vk::VertexInputAttributeDescription, 2> rtn;
		{//Vertex
			rtn[0].binding = 0;
			rtn[0].location = 0;
			rtn[0].format = vk::Format::eR32G32Sfloat;	//RG 32bit (signed) floating point
			rtn[0].offset = offsetof(Vertex, pos);		//Position of pos within Vertex
		}
		{//Colour
			rtn[1].binding = 0;
			rtn[1].location = 1;
			rtn[1].format = vk::Format::eR32G32B32Sfloat;	//RGB 32bit (signed) floating point
			rtn[1].offset = offsetof(Vertex, color);		//Position of pos within Vertex
		}
		return rtn;
	}
};
static const std::vector<Vertex> tempVertices = {
	{ { -0.5f, -0.5f },{ 1.0f, 0.0f, 0.0f } },
	{ { 0.5f, -0.5f },{ 0.0f, 1.0f, 0.0f } },
	{ { 0.5f, 0.5f },{ 0.0f, 0.0f, 1.0f } },
	{ { -0.5f, 0.5f },{ 1.0f, 1.0f, 1.0f } }
};
static const std::vector<uint16_t> tempIndices = {
	2, 1, 0, 0, 3, 2
};
/**
 * Consider using SPIRV-Cross for Shader introspection to detect layout/binding points by name
 * https://github.com/KhronosGroup/SPIRV-Cross
 * https://www.khronos.org/assets/uploads/developers/library/2016-vulkan-devday-uk/4-Using-spir-v-with-spirv-cross.pdf
 */
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
	
	vk::PipelineVertexInputStateCreateInfo vertexInput();
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
	vk::VertexInputBindingDescription t_vibd;
	std::array<vk::VertexInputAttributeDescription, 2> t_viad;
};

#endif //__GraphicsPipeline_h__