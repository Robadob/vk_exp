#include "Shaders.h"
#include <fstream>
#include "Context.h"


Shaders::Shaders(Context &ctxt, const char* vPath, const char* fPath)
	: m_context(ctxt)
{
	auto v = readFile(vPath);
	auto f = readFile(fPath);
	auto _v = createShaderModule(v);
	auto _f = createShaderModule(f);
	m_pipelineShaderStageCreate = createPipelineInfo(_v, _f);
	m_vertexInputStateCreate = vertexInput();
}
Shaders::~Shaders()
{
	m_context.Device().destroyDescriptorSetLayout(m_descriptorSetLayout);
	m_descriptorSetLayout = nullptr;
	m_context.Device().destroyDescriptorPool(m_descriptorPool);
	m_descriptorPool = nullptr;
	m_descriptorSet = nullptr;
}

std::vector<char> Shaders::readFile(const char *file)
{
	//Read file
	std::ifstream f(file, std::ios::binary | std::ios::ate);

	if (!f.is_open()) {		
		throw std::runtime_error(std::string("Failed to open shader file '").append(file).append("'!\n"));
	}

	size_t fileSize = (size_t)f.tellg();
	std::vector<char> buffer(fileSize);

	f.seekg(0);
	f.read(buffer.data(), fileSize);

	f.close();
	return buffer;
}
vk::ShaderModule Shaders::createShaderModule(const std::vector<char>& code) const
{
	vk::ShaderModuleCreateInfo createInfo;
	{
		createInfo.flags = {};
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
	}
	return m_context.Device().createShaderModule(createInfo);
}


std::vector<vk::PipelineShaderStageCreateInfo> Shaders::createPipelineInfo(const vk::ShaderModule &v, const vk::ShaderModule &f) const
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
vk::PipelineVertexInputStateCreateInfo Shaders::vertexInput()
{
	//t_vibd = Vertex::getBindingDesc();
	//t_viad = Vertex::getAttributeDesc();
	vk::PipelineVertexInputStateCreateInfo rtn;
	{
		rtn.flags = {};
		rtn.vertexBindingDescriptionCount = 1;
		//rtn.pVertexBindingDescriptions = &t_vibd;
		//rtn.vertexAttributeDescriptionCount = (unsigned int)t_viad.size();
		//rtn.pVertexAttributeDescriptions = t_viad.data();
	}
	return rtn;
}

void Shaders::descriptorSets()
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
	/*Texture Sampler Descriptor Set Layout*/
	vk::DescriptorSetLayoutBinding textureSamplerLayoutBinding;
	{
		textureSamplerLayoutBinding.binding = 1;
		textureSamplerLayoutBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		textureSamplerLayoutBinding.descriptorCount = 1;
		textureSamplerLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;
		textureSamplerLayoutBinding.pImmutableSamplers = nullptr;
	}
	std::array<vk::DescriptorSetLayoutBinding, 2> bindings = { uniformBufferLayoutBinding, textureSamplerLayoutBinding };
	vk::DescriptorSetLayoutCreateInfo descSetCreateInfo;
	{
		descSetCreateInfo.bindingCount = (unsigned int)bindings.size();
		descSetCreateInfo.pBindings = bindings.data();
	}
	m_descriptorSetLayout = m_context.Device().createDescriptorSetLayout(descSetCreateInfo);
	/*Desc Pool*/
	std::array<vk::DescriptorPoolSize, 2> poolSizes;
	{
		poolSizes[0].type = vk::DescriptorType::eUniformBuffer;
		poolSizes[0].descriptorCount = 1;
		poolSizes[1].type = vk::DescriptorType::eCombinedImageSampler;
		poolSizes[1].descriptorCount = 1;
	}
	vk::DescriptorPoolCreateInfo poolCreateInfo;
	{
		poolCreateInfo.poolSizeCount = (unsigned int)poolSizes.size();
		poolCreateInfo.pPoolSizes = poolSizes.data();
		poolCreateInfo.maxSets = 1;
		poolCreateInfo.flags = {};
	}
	m_descriptorPool = m_context.Device().createDescriptorPool(poolCreateInfo);
	vk::DescriptorSetLayout layouts[] = { m_descriptorSetLayout };
	vk::DescriptorSetAllocateInfo descSetAllocInfo;
	{
		descSetAllocInfo.descriptorPool = m_descriptorPool;
		descSetAllocInfo.descriptorSetCount = 1;
		descSetAllocInfo.pSetLayouts = layouts;
	}
	m_descriptorSet = m_context.Device().allocateDescriptorSets(descSetAllocInfo)[0];
}

void Shaders::bindUniforms()
{
	vk::DescriptorBufferInfo bufferInfo;
	{
		//bufferInfo.buffer = m_uniformBuffer;
		bufferInfo.offset = 0;
		//bufferInfo.range = sizeof(UniformBufferObject);
	}
	vk::DescriptorImageInfo imageInfo;
	{
		imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		//imageInfo.imageView = m_textureImageView;
		//imageInfo.sampler = m_textureSampler;
	}
	std::array<vk::WriteDescriptorSet, 2> descWrites;
	{
		descWrites[0].dstSet = m_descriptorSet;
		descWrites[0].dstBinding = 0;
		descWrites[0].dstArrayElement = 0;
		descWrites[0].descriptorType = vk::DescriptorType::eUniformBuffer;
		descWrites[0].descriptorCount = 1;
		descWrites[0].pBufferInfo = &bufferInfo;
		descWrites[0].pImageInfo = nullptr;
		descWrites[0].pTexelBufferView = nullptr;
	}
	{
		descWrites[1].dstSet = m_descriptorSet;
		descWrites[1].dstBinding = 1;
		descWrites[1].dstArrayElement = 0;
		descWrites[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
		descWrites[1].descriptorCount = 1;
		descWrites[1].pImageInfo = &imageInfo;
		descWrites[1].pBufferInfo = nullptr;
		descWrites[1].pTexelBufferView = nullptr;
	}
	m_context.Device().updateDescriptorSets((unsigned int)descWrites.size(), descWrites.data(), 0, nullptr);
}