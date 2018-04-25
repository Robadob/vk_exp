#ifndef __Context_h__
#define __Context_h__
#include <SDL/SDL.h>
#undef main //SDL breaks the regular main entry point, this fixes
#include <vulkan/vulkan.hpp>
#include <atomic>
class GraphicsPipeline;
#ifdef _DEBUG
static VKAPI_ATTR VkBool32 VKAPI_CALL debugLayerCallback(
	VkDebugReportFlagsEXT flags,
	VkDebugReportObjectTypeEXT objType,
	uint64_t obj,
	size_t location,
	int32_t code,
	const char* layerPrefix,
	const char* msg,
	void* userData);
#endif
/**
 * TODO
 * Add support for Debug Markers
 * https://vulkan.lunarg.com/doc/sdk/1.0.26.0/windows/debug_marker.html
 * Improve device/surface selection intelligence (see method level comments)
 */
class Context
{
	std::atomic<bool> isInit = false;
	SDL_Rect m_windowedBounds;//Storage of position/size of window before fullscreen
	SDL_Window *m_window = nullptr;
	vk::Instance m_instance = nullptr;
	vk::DispatchLoaderDynamic m_dynamicLoader;
	vk::SurfaceKHR m_surface = nullptr;
	vk::PhysicalDevice m_physicalDevice = nullptr;
	vk::PhysicalDeviceFeatures m_physicalDeviceFeatures;
	vk::Device m_device = nullptr;
	vk::Queue m_graphicsQueue = nullptr;
	vk::Queue m_presentQueue = nullptr;
	unsigned int m_graphicsQueueId = 0;
	unsigned int m_presentQueueId = 0;
	vk::Extent2D m_swapchainDims;
	vk::SurfaceFormatKHR m_surfaceFormat;
	vk::SwapchainKHR m_swapchain = nullptr;
	std::vector<vk::Image> m_scImages;
	std::vector<vk::ImageView> m_scImageViews;
	vk::PipelineCache m_pipelineCache = nullptr;
	vk::CommandPool m_commandPool = nullptr;
	vk::Semaphore m_imageAvailableSemaphore = nullptr;
	vk::Semaphore m_renderingFinishedSemaphore = nullptr;
	std::vector<vk::Framebuffer> m_scFramebuffers;
	std::vector<vk::CommandBuffer> m_commandBuffers;
	std::vector<vk::Fence> m_fences;
	vk::Image m_textureImage;
	vk::DeviceMemory m_textureImageMemory;
	vk::ImageView m_textureImageView;
	vk::Sampler m_textureSampler;
	vk::Buffer m_vertexBuffer = nullptr;
	vk::DeviceMemory m_vertexBufferMemory = nullptr;
	vk::Buffer m_indexBuffer = nullptr;
	vk::DeviceMemory m_indexBufferMemory = nullptr;
	vk::Buffer m_uniformBuffer = nullptr;
	vk::DeviceMemory m_uniformBufferMemory = nullptr;
	vk::DescriptorPool m_descriptorPool = nullptr;
	vk::DescriptorSet m_descriptorSet = nullptr;
	vk::DescriptorSetLayout m_descriptorSetLayout = nullptr;
	vk::Image m_depthImage = nullptr;
	vk::DeviceMemory m_depthImageMemory = nullptr;
	vk::ImageView m_depthImageView = nullptr;

	GraphicsPipeline *m_gfxPipeline = nullptr;
#ifdef _DEBUG
	vk::DebugReportCallbackEXT m_debugCallback = nullptr;
#endif
public:
	void init(unsigned int width = 1280, unsigned int height = 720, const char * title = "vk_exp");
	bool ready() const { return isInit.load(); }
	void destroy();
	const vk::Device &Device() const { return m_device; }
	const vk::Extent2D &SurfaceDims() const { return m_swapchainDims; }
	const vk::SurfaceFormatKHR &SurfaceFormat() const { return m_surfaceFormat; }
	const vk::PipelineCache &PipelineCache() const { return m_pipelineCache; }
	const vk::DescriptorSetLayout &DescriptorSetLayout() const { return m_descriptorSetLayout; }
	/**
	 * Could switch to the active rebuild swapChainCreateInfo.oldSwapchain = m_swapChain; method
	 */
	void rebuildSwapChain();
	/**
	 * Could use push constants, more efficint for dynamic uniforms
	 */
	void updateUniformBuffer();
	void getNextImage();
private:
	/**
	 * Creation utility
	 */
#ifdef _DEBUG
	static std::vector<const char*> supportedValidationLayers();
	void createDebugCallbacks();
	void destroyDebugCallbacks();
#endif
	std::vector<const char*> requiredInstanceExtensions() const;
	void createWindow(unsigned int width, unsigned int height, const char *title);
	void createInstance(const char * title);
	void createSurface();
	/**
	 * This could be improved to score devices better
	 * and confirm externally passed vk::PhysicalDeviceFeature requirements
	 */
	std::pair<unsigned int, unsigned int> selectPhysicalDevice();
	/**
	 * This should be improved to pass external vk::PhysicalDeviceFeature requirements
	 */
	void createLogicalDevice(unsigned int graphicsQIndex, unsigned int presentQIndex);
	vk::PresentModeKHR selectPresentMode();//Used by CreateSwapchain
	void createDescriptorPool();
	/**
	 * Could improve selection of swap surface
	 */
	void createSwapchain();
	void createSwapchainImages();
	void setupPipelineCache();
	void createGraphicsPipeline();
	void createCommandPool(unsigned int graphicsQIndex);//Redundant arg?
	void createDepthResources();
	void createFramebuffers();
	void createCommandBuffers();
	void createFences();
	void fillCommandBuffers();
	void createTextureImage();
	void createTextureImageView();
	void createTextureSampler();
	/**
	 * Could use a seperate transfer queue
	 */
	void createVertexBuffer();
	void createIndexBuffer();
	void createUniformBuffer();
	void updateDescriptorSet();//This binds resources to the descriptor set
	//Destroy
	void destroyVertexBuffer();
	void destroyIndexBuffer();
	void destroyUniformBuffer();
	void destroyTextureSampler();
	void destroyTextureImageView();
	void destroyTextureImage();
	void destroyFences();
	void destroyCommandPool();
	void destroyFramebuffers();
	void destroyDepthResources();
	void backupPipelineCache();
	void destroyPipelineCache();
	void destroySwapChainImages();
	void destroySwapChain();
	void destroyDescriptorPool();
	void destroyLogicalDevice();
	void destroySurface();
	void destroyInstance();
	void destroyWindow();
	//util
	std::string pipelineCacheFilepath();
	void createSwapchainStuff();
	void destroySwapchainStuff();
	unsigned int findMemoryType(const unsigned int &typeFilter, const vk::MemoryPropertyFlags& properties) const;
	vk::Format findSupportedFormat(const std::vector<vk::Format>& candidates, const vk::ImageTiling &tiling, vk::FormatFeatureFlags features);
	static bool hasStencilComponent(const vk::Format &format);
	void createBuffer(const vk::DeviceSize &size, const vk::BufferUsageFlags &usage, const vk::MemoryPropertyFlags &properties, vk::Buffer& buffer, vk::DeviceMemory& bufferMemory) const;
	void copyBuffer(const vk::Buffer &src, const vk::Buffer &dest, const vk::DeviceSize &size, const vk::DeviceSize &srcOffset = 0, const vk::DeviceSize &dstOffset = 0) const;
	void createImage(const uint32_t &width, const uint32_t &height, const vk::Format &format, const vk::ImageTiling &tiling, const vk::ImageUsageFlags &usage, const vk::MemoryPropertyFlags &properties, vk::Image& image, vk::DeviceMemory& imageMemory) const;
	vk::CommandBuffer beginSingleTimeCommands() const;
	void endSingleTimeCommands(vk::CommandBuffer &cb) const;
	void transitionImageLayout(vk::Image &image, const vk::Format &format, const vk::ImageLayout &oldLayout, const vk::ImageLayout &newLayout) const;
	void copyBufferToImage(const vk::Buffer &buffer, vk::Image &image, const uint32_t &width, const uint32_t &height) const;
	vk::ImageView createImageView(const vk::Image &image, const vk::Format &format, const vk::ImageAspectFlags aspectFlags) const;
	public:
	vk::Format findDepthFormat();
	void toggleFullScreen();
	bool isFullscreen();
};

#endif //__Context_h__
