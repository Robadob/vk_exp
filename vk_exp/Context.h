#ifndef __Context_h__
#define __Context_h__
#include <SDL/SDL.h>
#undef main //SDL breaks the regular main entry point, this fixes
#include <vulkan/vulkan.hpp>
#include <SDL/SDL_vulkan.h>
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
	bool isInit = false;
	SDL_Window *m_window = nullptr;
	vk::Instance m_instance = nullptr;
	vk::SurfaceKHR m_surface = nullptr;
	vk::PhysicalDevice m_physicalDevice = nullptr;
	vk::Device m_device = nullptr;
	vk::Queue m_graphicsQueue = nullptr;
	vk::Queue m_presentQueue = nullptr;
	vk::Semaphore m_imageAvailableSemaphore = nullptr;
	vk::Semaphore m_renderingFinishedSemaphore = nullptr;
	vk::Extent2D m_swapchainDims;
	vk::SurfaceFormatKHR m_surfaceFormat;
	vk::SwapchainKHR m_swapchain = nullptr;
	std::vector<vk::Image> m_scImages;
	std::vector<vk::ImageView>m_scImageViews;
	vk::CommandPool m_commandPool = nullptr;
	std::vector<vk::CommandBuffer> m_commandBuffers;
	std::vector<vk::Fence> m_fences;
#ifdef _DEBUG
	vk::DebugReportCallbackEXT m_debugCallback = nullptr;
#endif
public:
	void init(unsigned int width = 1280, unsigned int height = 720, const char * title = "vk_exp");
	void destroy();
	const vk::Device &Device() const { return m_device; }
	const vk::Extent2D &SurfaceDims() const { return m_swapchainDims; }
	const vk::SurfaceFormatKHR &SurfaceFormat() const { return m_surfaceFormat; }
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
	void createLogicalDevice(unsigned int graphicsQIndex);
	/**
	 * Could improve selection of swap surface
	 */
	void createSwapchain();
	void createSwapchainImages();
	void createCommandPool(unsigned int graphicsQIndex);
	void createFences();
	void createGraphicsPipeline();
	//Destroy
	void destroyFences();
	void destroyCommandPool();
	void destroySwapChainImages();
	void destroySwapChain();
	void destroyLogicalDevice();
	void destroySurface();
	void destroyInstance();
	void destroyWindow();
};

#endif //__Context_h__
