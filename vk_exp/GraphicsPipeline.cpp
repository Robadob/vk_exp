#include "GraphicsPipeline.h"

#include <fstream>
#include "Context.h"


GraphicsPipeline::GraphicsPipeline(Context &ctx, const char * vertPath, const char * fragPath)
	: m_context(ctx)
{
	m_pipelineLayout = pipelineLayout();
	m_renderPass = renderPass();

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
	auto dss = depthStencilState();
	auto cbs = colorBlendState();
	//auto ds = dynamicState();//Required if we wish to change viewport size at runtime

	vk::GraphicsPipelineCreateInfo pipelineInfo;
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
		pipelineInfo.pDepthStencilState = &dss;
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
	m_pipeline = nullptr;
	m_context.Device().destroyPipelineLayout(m_pipelineLayout);
	m_pipelineLayout = nullptr;
	m_context.Device().destroyRenderPass(m_renderPass);
	m_renderPass = nullptr;
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
	vk::ShaderModuleCreateInfo createInfo;
	{
		createInfo.flags = {};
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
	}
	return m_context.Device().createShaderModule(createInfo);
}

std::vector<vk::PipelineShaderStageCreateInfo> GraphicsPipeline::createPipelineInfo(vk::ShaderModule &v, vk::ShaderModule &f)
{
	vk::PipelineShaderStageCreateInfo vss;
	{
		vss.flags = {};
		vss.stage = vk::ShaderStageFlagBits::eVertex;
		vss.module = v;
		vss.pName = "main";
		vss.pSpecializationInfo = nullptr;
	}
	vk::PipelineShaderStageCreateInfo fss;
	{
		fss.flags = {};
		fss.stage = vk::ShaderStageFlagBits::eFragment;
		fss.module = f;
		fss.pName = "main";
		fss.pSpecializationInfo = nullptr;
	}
	return std::vector<vk::PipelineShaderStageCreateInfo>{ vss, fss };
}
vk::PipelineVertexInputStateCreateInfo GraphicsPipeline::vertexInput()
{
	t_vibd = Vertex::getBindingDesc();
	t_viad = Vertex::getAttributeDesc();
	vk::PipelineVertexInputStateCreateInfo rtn;
	{
		rtn.flags = {};
		rtn.vertexBindingDescriptionCount = 1;
		rtn.pVertexBindingDescriptions = &t_vibd;
		rtn.vertexAttributeDescriptionCount = (unsigned int)t_viad.size();
		rtn.pVertexAttributeDescriptions = t_viad.data();
	}
	return rtn;
}
vk::PipelineInputAssemblyStateCreateInfo GraphicsPipeline::inputAssembly() const
{
	vk::PipelineInputAssemblyStateCreateInfo rtn;
	{
		rtn.flags = {};
		rtn.topology = vk::PrimitiveTopology::eTriangleList;
		rtn.primitiveRestartEnable = false;
	}
	return rtn;
}

vk::PipelineViewportStateCreateInfo GraphicsPipeline::viewportState()
{
	auto ex = m_context.SurfaceDims();
	t_viewport = vk::Viewport(
		0.0f, 0.0f,							/*x,y*/
		(float)ex.width, (float)ex.height,	/*Width, Height*/
		0.0f, 1.0f							/*Depth Min, Max*/
	);
	t_scissors = vk::Rect2D(
		{ 0,0 },	/*Offset*/
		ex			/*Extent*/
	);
	vk::PipelineViewportStateCreateInfo rtn;
	{
		rtn.flags = {};
		rtn.viewportCount = 1;
		rtn.pViewports = &t_viewport;
		rtn.scissorCount = 1;
		rtn.pScissors = &t_scissors;
	}
	return rtn;
}
vk::PipelineRasterizationStateCreateInfo GraphicsPipeline::rasterizerState() const
{
	vk::PipelineRasterizationStateCreateInfo rtn;
	{
		rtn.flags = {};
		rtn.depthClampEnable = false;
		rtn.rasterizerDiscardEnable = false;
		rtn.polygonMode = vk::PolygonMode::eFill;
		rtn.lineWidth = 1.0f;
		rtn.cullMode = vk::CullModeFlagBits::eBack;
		rtn.frontFace = vk::FrontFace::eCounterClockwise;
		rtn.depthBiasEnable = false;
		rtn.depthBiasConstantFactor = 0.0f;
		rtn.depthBiasClamp = 0.0f;
		rtn.depthBiasSlopeFactor = 0.0f;
	}
	return rtn;
}

vk::PipelineMultisampleStateCreateInfo GraphicsPipeline::multisampleState() const
{
	vk::PipelineMultisampleStateCreateInfo rtn;
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

vk::PipelineDepthStencilStateCreateInfo GraphicsPipeline::depthStencilState() const
{
	vk::PipelineDepthStencilStateCreateInfo rtn;
	{
		rtn.depthTestEnable = true;
		rtn.depthWriteEnable = true;
		rtn.depthCompareOp = vk::CompareOp::eLess;
		rtn.depthBoundsTestEnable = false;
		rtn.minDepthBounds = 0.0f; // Optional
		rtn.maxDepthBounds = 1.0f; // Optional
		rtn.stencilTestEnable = false;
		rtn.front = vk::StencilOp::eKeep; // Optional
		rtn.back = vk::StencilOp::eKeep; // Optional
	}
	return rtn;
}

vk::PipelineColorBlendStateCreateInfo GraphicsPipeline::colorBlendState()
{
	//vk::PipelineColorBlendAttachmentState t_cbas;
	{
		t_cbas.blendEnable = true;//Enable Alpha Blending!
		t_cbas.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
		t_cbas.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
		t_cbas.colorBlendOp = vk::BlendOp::eAdd;
		t_cbas.srcAlphaBlendFactor = vk::BlendFactor::eOne;
		t_cbas.dstAlphaBlendFactor = vk::BlendFactor::eZero;
		t_cbas.alphaBlendOp = vk::BlendOp::eAdd;
		t_cbas.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
	}
	vk::PipelineColorBlendStateCreateInfo rtn;
	{
		rtn.flags = {};
		rtn.logicOpEnable = false;
		rtn.logicOp = vk::LogicOp::eCopy;
		rtn.attachmentCount = 1;
		rtn.pAttachments = &t_cbas;
		rtn.blendConstants[0] = 0.0f;//These interact with blend factors which have a 'constant colour'
		rtn.blendConstants[1] = 0.0f;//Otherwise they have no effect
		rtn.blendConstants[2] = 0.0f;
		rtn.blendConstants[3] = 0.0f;
	}
	return rtn;
}

vk::PipelineLayout GraphicsPipeline::pipelineLayout()
{
	vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
	{
		pipelineLayoutInfo.flags = {};
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = &m_context.DescriptorSetLayout();
		pipelineLayoutInfo.pushConstantRangeCount = 0;
		pipelineLayoutInfo.pPushConstantRanges = nullptr;
	}
	return m_context.Device().createPipelineLayout(pipelineLayoutInfo);
}
vk::RenderPass GraphicsPipeline::renderPass() const
{
	vk::AttachmentDescription colorAttachment;
	{
		colorAttachment.flags = {};
		colorAttachment.format = m_context.SurfaceFormat().format;
		colorAttachment.samples = vk::SampleCountFlagBits::e1;
		colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
		colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
		colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
		colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
		colorAttachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;
	}
	vk::AttachmentReference colorAttachmentRef;
	{
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;
	}
	vk::AttachmentDescription depthAttachment;
	{
		depthAttachment.flags = {};
		depthAttachment.format = m_context.findDepthFormat();
		depthAttachment.samples = vk::SampleCountFlagBits::e1;
		depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
		depthAttachment.storeOp = vk::AttachmentStoreOp::eDontCare;
		depthAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
		depthAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		depthAttachment.initialLayout = vk::ImageLayout::eUndefined;
		depthAttachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
	}
	vk::AttachmentReference depthAttachmentRef;
	{
		depthAttachmentRef.attachment = 1;
		depthAttachmentRef.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
	}
	std::array<vk::AttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
	vk::SubpassDescription subpass;
	{
		subpass.flags = {};
		subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
		subpass.inputAttachmentCount = 0;
		subpass.pInputAttachments = nullptr;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;
		subpass.pResolveAttachments = nullptr;
		subpass.pDepthStencilAttachment = &depthAttachmentRef;
		subpass.preserveAttachmentCount = 0;
		subpass.pPreserveAttachments = nullptr;
	}
	vk::SubpassDependency spDependency;
	{
		spDependency.srcSubpass = VK_SUBPASS_EXTERNAL;//Previous renderpass
		spDependency.dstSubpass = 0;//Index of subpass
		spDependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		spDependency.srcAccessMask = vk::AccessFlags();
		//Wait for swapchain to finish reading from image
		spDependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		spDependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
	}
	vk::RenderPassCreateInfo rpInfo;
	{
		rpInfo.flags = {};
		rpInfo.attachmentCount = (unsigned int)attachments.size();
		rpInfo.pAttachments = attachments.data();
		rpInfo.subpassCount = 1;
		rpInfo.pSubpasses = &subpass;
		rpInfo.dependencyCount = 1;
		rpInfo.pDependencies = &spDependency;
	}
	return m_context.Device().createRenderPass(rpInfo);
}