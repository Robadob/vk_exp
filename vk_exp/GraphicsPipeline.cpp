#include "GraphicsPipeline.h"

#include <fstream>
#include "Context.h"

GraphicsPipeline::GraphicsPipeline(Context &ctx, const char * vertPath, const char * fragPath)
	: m_context(ctx)
	, m_pipelineLayout(pipelineLayout())
	, m_renderPass(renderPass())
{
	auto v = readFile(vertPath);
	auto f = readFile(fragPath);
	auto _v = createShader(v);
	auto _f = createShader(f);
	auto s = createPipelineInfo(_v, _f);

	auto vi = vertexInput();
	auto ia = inputAssembly();
	auto vs = viewportState();
	auto rs = rasterizerState();
	auto ms = multisampleState();
	auto cbs = colorBlendState();
	//auto ds = dynamicState();//Required if we wish to change viewport size at runtime
	
	vk::GraphicsPipelineCreateInfo pipelineInfo(
		{},					/*Flags*/
		2,					/*Stage Count*/
		s.data(),				/*Shader Stages*/
		&vi,				/*Vertex Input State*/
		&ia,				/*Input Assembly State*/
		nullptr,			/*Tessellation State*/
		&vs,				/*Viewport State*/
		&rs,				/*Rasterization State*/
		&ms,				/*Multisample State*/
		nullptr,			/*Depth Stencil State*/
		&cbs,				/*Color Blend State*/
		nullptr,			/*Dynamic State*/
		m_pipelineLayout,	/*Pipeline Layout*/
		m_renderPass,		/*Renderpass*/
		0,					/*Subpass*/
		nullptr,			/*Bass Pipeline Handle*/
		-1					/*Base Pipeline Index*/
	);
	m_pipeline = m_context.Device().createGraphicsPipeline(nullptr, pipelineInfo);
	m_context.Device().destroyShaderModule(_v);
	m_context.Device().destroyShaderModule(_f);
}
GraphicsPipeline::~GraphicsPipeline()
{
	m_context.Device().destroyPipeline(m_pipeline);
	m_context.Device().destroyPipelineLayout(m_pipelineLayout);
	m_context.Device().destroyRenderPass(m_renderPass);
}

std::vector<char> GraphicsPipeline::readFile(const char * file)
{
	//Read file
	std::ifstream f(file, std::ios::binary | std::ios::ate);

	if (!f.is_open()) {
		throw std::exception("Failed to open shader file!");
	}

	size_t fileSize = (size_t)f.tellg();
	std::vector<char> buffer(fileSize);

	f.seekg(0);
	f.read(buffer.data(), fileSize);

	f.close();
	return buffer;
}

vk::ShaderModule GraphicsPipeline::createShader(const std::vector<char>& code) const
{
	vk::ShaderModuleCreateInfo createInfo(
		{},												/*Flags*/
		code.size(),									/*Shader Size*/
		reinterpret_cast<const uint32_t*>(code.data())	/*Shader Data*/
	);
	return m_context.Device().createShaderModule(createInfo);
}

std::vector<vk::PipelineShaderStageCreateInfo> GraphicsPipeline::createPipelineInfo(vk::ShaderModule &v, vk::ShaderModule &f)
{
	vk::PipelineShaderStageCreateInfo vss(
		{},									/*Flags*/
		vk::ShaderStageFlagBits::eVertex,	/*Shader Type*/
		v,									/*Shader Module*/
		"main",								/*Entry Point*/
		nullptr								/*Specialisation Info (Used to set constants)*/
	);
	vk::PipelineShaderStageCreateInfo fss(
	{},										/*Flags*/
		vk::ShaderStageFlagBits::eFragment,	/*Shader Type*/
		f,									/*Shader Module*/
		"main",								/*Entry Point*/
		nullptr								/*Specialisation Info (Used to set constants)*/
	);
	return std::vector<vk::PipelineShaderStageCreateInfo>{ vss, fss };;
}
vk::PipelineVertexInputStateCreateInfo GraphicsPipeline::vertexInput() const
{
	return vk::PipelineVertexInputStateCreateInfo
	(
		{},			/*Flags*/
		0,			/*Binding Description Count*/
		nullptr,	/*Binding Description*/
		0,			/*Attribute Description Count*/
		nullptr		/*Attribute Description*/
	);
}
vk::PipelineInputAssemblyStateCreateInfo GraphicsPipeline::inputAssembly() const
{
	return vk::PipelineInputAssemblyStateCreateInfo
	(
		{},										/*Flags*/
		vk::PrimitiveTopology::eTriangleList,	/*Primitive Topology*/
		false									/*Restart Enable*/
	);
}

vk::PipelineViewportStateCreateInfo GraphicsPipeline::viewportState() const
{
	auto ex = m_context.SurfaceDims();
	vk::Viewport vp = vk::Viewport(
		0.0f, 0.0f,							/*x,y*/
		(float)ex.width, (float)ex.height,	/*Width, Height*/
		0.0f, 1.0f							/*Depth Min, Max*/
	);
	auto s = vk::Rect2D(
		{ 0,0 },	/*Offset*/
		ex			/*Extent*/
	);
	return vk::PipelineViewportStateCreateInfo(
		{},		/*Flags*/
		1,		/*Viewport Count*/
		&vp,	/*Viewports*/
		1,		/*Scissor Count*/
		&s		/*Scissors*/
	);
}
vk::PipelineRasterizationStateCreateInfo GraphicsPipeline::rasterizerState() const
{
	return vk::PipelineRasterizationStateCreateInfo(
		{},									/*Flags*/
		false,								/*Depth Clamp Enable*/
		false,								/*Rasterizer Discard Enable*/
		vk::PolygonMode::eFill,				/*Polygon Mode*/
		vk::CullModeFlagBits::eBack,		/*Cull Mode*/
		vk::FrontFace::eCounterClockwise,	/*Front Face*/
		false,								/*Depth Bias Enable*/
		0.0f,								/*Depth Bias Constant Factor*/
		0.0f,								/*Depth Bias Clamp*/
		0.0f,								/*Depth Bias Slope Factor*/
		1.0f								/*Line Width*/
	);
}

vk::PipelineMultisampleStateCreateInfo GraphicsPipeline::multisampleState() const
{
	return vk::PipelineMultisampleStateCreateInfo(
		{},								/*Flags*/
		vk::SampleCountFlagBits::e1,	/*Rasterization Samples*/
		false,							/*Sample Shading Enable*/
		1.0f,							/*Min Sample Shading*/
		nullptr,						/*Sample Mask*/
		false,							/*Alpha To Coverage Enable*/
		false							/*Alpha to One Enable*/
	);
}

vk::PipelineColorBlendStateCreateInfo GraphicsPipeline::colorBlendState() const
{
	auto cbas = vk::PipelineColorBlendAttachmentState(
		false,									/*Blend Enable*/
		vk::BlendFactor::eOne,					/*Source Color Blend Factor*/
		vk::BlendFactor::eOneMinusSrcAlpha,		/*Destination Color Blend Factor*/
		vk::BlendOp::eAdd,						/*Color Blend Operation*/
		vk::BlendFactor::eOne,					/*Source Alpha Blend Factor*/
		vk::BlendFactor::eZero,					/*Destination Alpha Blend Factor*/
		vk::BlendOp::eAdd						/*Alpha Blend Operation*/
	);
	return vk::PipelineColorBlendStateCreateInfo(
		{},						/*Flags*/
		false,					/*Enable logic operations*/
		vk::LogicOp::eCopy,		/*Logic Operation*/
		1,						/*Attachment Count*/
		&cbas,					/*Color Blend Attachments*/
		{0.0f,0.0f,0.0f,0.0f}	/*Blend Constants*/
	);
}

vk::PipelineLayout GraphicsPipeline::pipelineLayout() const
{
	vk::PipelineLayoutCreateInfo pipelineLayoutInfo(
		{},			/*Flags*/
		0,			/*Set Layout Count*/
		nullptr,	/*Descriptor Set Layout*/
		0,			/*Push Constant Range Count*/
		nullptr		/*Push Constant Range Layout*/
	);
	return m_context.Device().createPipelineLayout(pipelineLayoutInfo);
}
vk::RenderPass GraphicsPipeline::renderPass() const
{
	vk::AttachmentDescription attachDesc(
		{},									/*Flags*/
		m_context.SurfaceFormat().format,	/*Format*/
		vk::SampleCountFlagBits::e1,		/*Samples*/
		vk::AttachmentLoadOp::eLoad,		/*Attachment Load Operation*/
		vk::AttachmentStoreOp::eStore,		/*Store Operation*/
		vk::AttachmentLoadOp::eDontCare,	/*Stencil Load Operation*/
		vk::AttachmentStoreOp::eDontCare,	/*Stencil Store Op*/
		vk::ImageLayout::eUndefined,		/*Initial Image Layout*/
		vk::ImageLayout::ePresentSrcKHR		/*Final Image Layout*/
	);
	vk::AttachmentReference colorAttachmentRef(
		0,											/*Attachment Index*/
		vk::ImageLayout::eColorAttachmentOptimal	/*Image Layout*/
	);
	vk::SubpassDescription subpass(
		{},									/*Flags*/
		vk::PipelineBindPoint::eGraphics,	/*Pipeline Bind Point*/
		0,									/*Input Attachment Count*/
		nullptr,							/*Input Attachments*/
		1,									/*Color Attachment Count*/
		&colorAttachmentRef,				/*Color Attachments*/
		nullptr,							/*Resolve Attachments*/
		nullptr,							/*Depth Stencil Attachment*/
		0,									/*Preserve Attachment Count*/
		nullptr								/*Preserve Attachments*/
	);

	vk::RenderPassCreateInfo rpInfo(
		{},						/*Flags*/
		1,						/*Attachment Count*/
		&attachDesc,			/*Attachments*/
		0,						/*Subpass Count*/
		&subpass,				/*Subpasses*/
		0,						/*Dependency Count*/
		nullptr					/*Subpass Dependencies*/
	);
	return m_context.Device().createRenderPass(rpInfo);
}