#ifndef __Image2D_h__
#define __Image2D_h__
#include <vulkan/vulkan.hpp>
class StagingBuffer;
class Context;
class Image2D
{
	vk::Image m_image = nullptr;
	vk::DeviceMemory m_imageMemory = nullptr;
	vk::ImageView m_imageView = nullptr;
	vk::Sampler m_sampler = nullptr;
	vk::Extent2D m_extent;
	vk::DeviceSize m_size;
	vk::Format m_format;
	Context &m_context;
	vk::ImageLayout m_currentLayout;
	void createImage(const vk::ImageTiling &tiling, const vk::ImageUsageFlags &usage, const vk::MemoryPropertyFlags &properties);
	void transitionImageLayout(const vk::ImageLayout &newLayout);
	void Image2D::copyTo(StagingBuffer &s);
	bool hasStencilComponent() const;
	static std::string findValidPath(const char* imagePath, const char *modelFolder);
	std::string m_path;
public:
	Image2D(Context &context, const char* imagePath, const char *modelFolder=nullptr);
	~Image2D();
	vk::Image Image() { return m_image; }
	vk::ImageView ImageView() { return m_imageView; }
	vk::Sampler Sampler() { return m_sampler;  }
	vk::Format Format() { return m_format;  }
};
#endif //__Image2D_h__