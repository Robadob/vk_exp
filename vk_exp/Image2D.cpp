#include "Image2D.h"
#include "Context.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#include "Buffer.h"
#include "StringUtils.h"
#ifdef _WIN32
#include <filesystem>
using namespace std::tr2::sys;
#else
#include <experimental/filesystem>
using namespace std::experimental::filesystem::v1;
#endif

Image2D::Image2D(Context &context, const char* imagePath, const char *modelFolder)
	: m_format(vk::Format::eR8G8B8A8Unorm)
	, m_context(context)
	, m_currentLayout(vk::ImageLayout::eUndefined)
	, m_path(findValidPath(imagePath, modelFolder))
{
	if (m_path.empty())
		throw std::runtime_error("Image2D() imagePath not a valid file path.");
	int texWidth, texHeight, texChannels;
	stbi_uc* pixels = stbi_load(m_path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	m_extent = vk::Extent2D(texWidth, texHeight);
	//assert(texChannels == 4);//texChanells is number in file, not number returned, atm we upcast all to rgba
	m_size = m_extent.width * m_extent.height * 4;
	if (!pixels) {
		throw std::runtime_error("Image2D() Failed to load texture image!");
	}
	StagingBuffer staging(m_context, m_size, pixels);
	stbi_image_free(pixels);
	createImage(
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
		vk::MemoryPropertyFlagBits::eDeviceLocal
	);
	transitionImageLayout(vk::ImageLayout::eTransferDstOptimal);
	copyTo(staging);
	transitionImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
	vk::ImageViewCreateInfo viewInfo;
	{
		viewInfo.image = m_image;
		viewInfo.viewType = vk::ImageViewType::e2D;
		viewInfo.format = m_format;
		viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;
		viewInfo.components = vk::ComponentMapping();//eIdentity
	}
	m_imageView = m_context.Device().createImageView(viewInfo);

	vk::SamplerCreateInfo samplerInfo;
	{
		samplerInfo.magFilter = vk::Filter::eLinear;
		samplerInfo.minFilter = vk::Filter::eLinear;
		samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
		samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
		samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
		samplerInfo.anisotropyEnable = m_context.PhysicalDeviceFeatures().samplerAnisotropy;
		samplerInfo.maxAnisotropy = 16;
		samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
		samplerInfo.unnormalizedCoordinates = false;
		samplerInfo.compareEnable = false;
		samplerInfo.compareOp = vk::CompareOp::eAlways;
		samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 0.0f;
	}
	m_sampler = m_context.Device().createSampler(samplerInfo);
}
Image2D::~Image2D()
{
	m_context.Device().destroySampler(m_sampler);
	m_sampler = nullptr;
	m_context.Device().destroyImageView(m_imageView);
	m_imageView = nullptr;
	m_context.Device().destroyImage(m_image);
	m_image = nullptr;
	m_context.Device().freeMemory(m_imageMemory);
	m_imageMemory = nullptr;
}
void Image2D::createImage(const vk::ImageTiling &tiling, const vk::ImageUsageFlags &usage, const vk::MemoryPropertyFlags &properties)
{
	vk::ImageCreateInfo imgCreate;
	{
		imgCreate.imageType = vk::ImageType::e2D;
		imgCreate.extent.width = m_extent.width;
		imgCreate.extent.height = m_extent.height;
		imgCreate.extent.depth = 1;
		imgCreate.mipLevels = 1;
		imgCreate.arrayLayers = 1;
		imgCreate.format = m_format;
		imgCreate.tiling = tiling;
		imgCreate.initialLayout = vk::ImageLayout::eUndefined;
		imgCreate.usage = usage;
		imgCreate.sharingMode = vk::SharingMode::eExclusive;
		imgCreate.samples = vk::SampleCountFlagBits::e1;
		imgCreate.flags = {}; // Optional
	}
	m_image = m_context.Device().createImage(imgCreate);
	vk::MemoryRequirements memReqs = m_context.Device().getImageMemoryRequirements(m_image);
	vk::MemoryAllocateInfo memAllocInfo;
	{
		memAllocInfo.allocationSize = memReqs.size;
		memAllocInfo.memoryTypeIndex = m_context.findMemoryType(memReqs.memoryTypeBits, properties);
	}
	m_imageMemory = m_context.Device().allocateMemory(memAllocInfo);
	m_context.Device().bindImageMemory(m_image, m_imageMemory, 0);
	m_currentLayout = imgCreate.initialLayout;
}
void Image2D::transitionImageLayout(const vk::ImageLayout &newLayout)
{
	vk::CommandBuffer cb = m_context.beginSingleTimeCommands();
	vk::PipelineStageFlags srcStage, dstStage;
	vk::ImageMemoryBarrier barrier;
	{
		barrier.oldLayout = m_currentLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = m_image;
		barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		//Configure stuff based on layout transition
		if (m_currentLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal)
		{
			barrier.srcAccessMask = {};
			barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
			srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
			dstStage = vk::PipelineStageFlagBits::eTransfer;
		}
		else if (m_currentLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
		{
			barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
			barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
			srcStage = vk::PipelineStageFlagBits::eTransfer;
			dstStage = vk::PipelineStageFlagBits::eFragmentShader;
		}
		else if (m_currentLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
		{
			barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
			if (hasStencilComponent()) {
				barrier.subresourceRange.aspectMask |= vk::ImageAspectFlagBits::eStencil;
			}
			barrier.srcAccessMask = {};
			barrier.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
			srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
			dstStage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
		}
		else
		{
			throw std::invalid_argument("Unexpected layout transition!");
		}
	}
	cb.pipelineBarrier(
		srcStage, dstStage,
		{},
		0, nullptr,
		0, nullptr,
		1, &barrier
	);
	m_context.endSingleTimeCommands(cb);
	m_currentLayout = newLayout;
}
void Image2D::copyTo(StagingBuffer &s)
{
	vk::CommandBuffer cb = m_context.beginSingleTimeCommands();
	vk::BufferImageCopy region;
	{
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;

		region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = vk::Offset3D(0, 0, 0);
		region.imageExtent = vk::Extent3D(m_extent.width, m_extent.height, 1);
	}
	cb.copyBufferToImage(s.get(), m_image, vk::ImageLayout::eTransferDstOptimal, 1, &region);
	m_context.endSingleTimeCommands(cb);
}

bool Image2D::hasStencilComponent() const
{
	return m_format == vk::Format::eD32SfloatS8Uint || m_format == vk::Format::eD24UnormS8Uint;
}

std::string Image2D::findValidPath(const char* imagePath, const char *modelFolder)
{
	//Default arg passed
	if (!modelFolder)
		return imagePath;
	//Raw exists
	if (exists(path(imagePath)))
		return imagePath;
	//Exists in model dir
	if (modelFolder&&exists(path((std::string(modelFolder) + imagePath).c_str())))
		return std::string(modelFolder) + imagePath;
	//Exists in model dir [path incorrect]
	if (modelFolder&&exists(std::experimental::filesystem::path((std::string(modelFolder) + su::filenameFromPath(imagePath)).c_str())))
		return std::string(modelFolder) + su::filenameFromPath(imagePath);
	return std::string();
}