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

	auto pipelineInfo = vk::GraphicsPipelineCreateInfo();
	{
		pipelineInfo.flags = {};
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = s.data();
		pipelineInfo.pVertexInputState = &vi;
		pipelineInfo.pInputAssemblyState = &ia;
		pipelineInfo.pTessellationState = nullptr;
		pipelineInfo.pViewportState = &vs;
		pipelineInfo.pRasterizationState = &rs;
		pipelineInfo.pMultisampleState = &ms;
		pipelineInfo.pDepthStencilState = nullptr;
		pipelineInfo.pColorBlendState = &cbs;
		pipelineInfo.pDynamicState = nullptr;
		pipelineInfo.layout = m_pipelineLayout;
		pipelineInfo.renderPass = m_renderPass;
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = nullptr;
		pipelineInfo.basePipelineIndex = -1;
	}
	m_pipeline = m_context.Device().createGraphicsPipeline(m_context.PipelineCache(), pipelineInfo);
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
	auto createInfo = vk::ShaderModuleCreateInfo();
	{
		createInfo.flags = {};
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
	}
	return m_context.Device().createShaderModule(createInfo);
}

std::vector<vk::PipelineShaderStageCreateInfo> GraphicsPipeline::createPipelineInfo(vk::ShaderModule &v, vk::ShaderModule &f)
{
	auto vss = vk::PipelineShaderStageCreateInfo();
	{
		vss.flags = {};
		vss.stage = vk::ShaderStageFlagBits::eVertex;
		vss.module = v;
		vss.pName = "main";
		vss.pSpecializationInfo = nullptr;
	}
	auto fss = vk::PipelineShaderStageCreateInfo();
	{
		fss.flags = {};
		fss.stage = vk::ShaderStageFlagBits::eFragment;
		fss.module = f;
		fss.pName = "main";
		fss.pSpecializationInfo = nullptr;
	}
	return std::vector<vk::PipelineShaderStageCreateInfo>{ vss, fss };;
}
vk::PipelineVertexInputStateCreateInfo GraphicsPipeline::vertexInput() const
{
	auto rtn = vk::PipelineVertexInputStateCreateInfo();
	{
		rtn.flags = {};
		rtn.vertexBindingDescriptionCount = 0;
		rtn.pVertexBindingDescriptions = nullptr;
		rtn.vertexAttributeDescriptionCount = 0;
		rtn.pVertexAttributeDescriptions = nullptr;
	}
	return rtn;
}
vk::PipelineInputAssemblyStateCreateInfo GraphicsPipeline::inputAssembly() const
{
	auto rtn = vk::PipelineInputAssemblyStateCreateInfo();
	{
		rtn.flags = {};
		rtn.topology = vk::PrimitiveTopology::eTriangleList;
		rtn.primitiveRestartEnable = false;
	}
	return rtn;
}

vk::PipelineViewportStateCreateInfo GraphicsPipeline::viewportState() const
{
	auto ex = m_context.SurfaceDims();
	auto vp = vk::Viewport(
		0.0f, 0.0f,							/*x,y*/
		(float)ex.width, (float)ex.height,	/*Width, Height*/
		0.0f, 1.0f							/*Depth Min, Max*/
	);
	auto s = vk::Rect2D(
	{ 0,0 },	/*Offset*/
		ex			/*Extent*/
	);
	auto rtn = vk::PipelineViewportStateCreateInfo();
	{
		rtn.flags = {};
		rtn.viewportCount = 1;
		rtn.pViewports = &vp;
		rtn.scissorCount = 1;
		rtn.pScissors = &s;
	}
	return rtn;
}
vk::PipelineRasterizationStateCreateInfo GraphicsPipeline::rasterizerState() const
{
	auto rtn = vk::PipelineRasterizationStateCreateInfo();
	{
		rtn.flags = {};
		rtn.depthClampEnable = false;
		rtn.rasterizerDiscardEnable = false;
		rtn.polygonMode = vk::PolygonMode::eFill;
		rtn.cullMode = vk::CullModeFlagBits::eBack;
		rtn.frontFace = vk::FrontFace::eCounterClockwise;
		rtn.depthBiasEnable = false;
		rtn.depthBiasConstantFactor = 0.0f;
		rtn.depthBiasClamp = 0.0f;
		rtn.depthBiasSlopeFactor = 0.0f;
		rtn.lineWidth = 1.0f;
	}
	return rtn;
}

vk::PipelineMultisampleStateCreateInfo GraphicsPipeline::multisampleState() const
{
	auto rtn = vk::PipelineMultisampleStateCreateInfo();
	{
		rtn.flags = {};
		rtn.rasterizationSamples = vk::SampleCountFlagBits::e1;
		rtn.sampleShadingEnable = false;
		rtn.minSampleShading = 1.0f;
		rtn.pSampleMask = nullptr;
		rtn.alphaToCoverageEnable = false;
		rtn.alphaToOneEnable = false;
	}
	return rtn;
}

vk::PipelineColorBlendStateCreateInfo GraphicsPipeline::colorBlendState() const
{
	auto cbas = vk::PipelineColorBlendAttachmentState();
	{
		cbas.blendEnable = false;
		cbas.srcColorBlendFactor = vk::BlendFactor::eOne;
		cbas.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
		cbas.colorBlendOp = vk::BlendOp::eAdd;
		cbas.srcAlphaBlendFactor = vk::BlendFactor::eOne;
		cbas.dstAlphaBlendFactor = vk::BlendFactor::eZero;
		cbas.alphaBlendOp = vk::BlendOp::eAdd;
	}
	auto rtn = vk::PipelineColorBlendStateCreateInfo();
	{
		rtn.flags = {};
		rtn.logicOpEnable = false;
		rtn.logicOp = vk::LogicOp::eCopy;
		rtn.attachmentCount = 1;
		rtn.pAttachments = &cbas;
		rtn.blendConstants[0] = 0.0f;
		rtn.blendConstants[1] = 0.0f;
		rtn.blendConstants[2] = 0.0f;
		rtn.blendConstants[3] = 0.0f;
	}
	return rtn;
}

vk::PipelineLayout GraphicsPipeline::pipelineLayout() const
{
	auto pipelineLayoutInfo = vk::PipelineLayoutCreateInfo();
	{
		pipelineLayoutInfo.flags = {};
		pipelineLayoutInfo.setLayoutCount = 0;
		pipelineLayoutInfo.pSetLayouts = nullptr;
		pipelineLayoutInfo.pushConstantRangeCount = 0;
		pipelineLayoutInfo.pPushConstantRanges = nullptr;
	}
	return m_context.Device().createPipelineLayout(pipelineLayoutInfo);
}
vk::RenderPass GraphicsPipeline::renderPass() const
{
	auto attachDesc = vk::AttachmentDescription();
	{
		attachDesc.flags = {};
		attachDesc.format = m_context.SurfaceFormat().format;
		attachDesc.samples = vk::SampleCountFlagBits::e1;
		attachDesc.loadOp = vk::AttachmentLoadOp::eLoad;
		attachDesc.storeOp = vk::AttachmentStoreOp::eStore;
		attachDesc.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
		attachDesc.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		attachDesc.initialLayout = vk::ImageLayout::eUndefined;
		attachDesc.finalLayout = vk::ImageLayout::ePresentSrcKHR;
	}
	auto colorAttachmentRef = vk::AttachmentReference();
	{
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;
	}
	auto subpass = vk::SubpassDescription();
	{
		subpass.flags = {};
		subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
		subpass.inputAttachmentCount = 0;
		subpass.pInputAttachments = nullptr;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;
		subpass.pResolveAttachments = nullptr;
		subpass.pDepthStencilAttachment = nullptr;
		subpass.preserveAttachmentCount = 0;
		subpass.pPreserveAttachments = nullptr;
	}
	auto rpInfo = vk::RenderPassCreateInfo();
	{
		rpInfo.flags = {};
		rpInfo.attachmentCount = 1;
		rpInfo.pAttachments = &attachDesc;
		rpInfo.subpassCount = 0;
		rpInfo.pSubpasses = &subpass;
		rpInfo.dependencyCount = 0;
		rpInfo.pDependencies = nullptr;
	}
	return m_context.Device().createRenderPass(rpInfo);
}