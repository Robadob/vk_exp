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
	vk::SwapchainKHR m_swapchain = nullptr;
	std::vector<vk::Image> m_scImages;
	vk::CommandPool m_commandPool = nullptr;
	std::vector<vk::CommandBuffer> m_commandBuffers;
	std::vector<vk::Fence> m_fences;
#ifdef _DEBUG
	vk::DebugReportCallbackEXT m_debugCallback = nullptr;
#endif
public:
	void init(unsigned int width = 1280, unsigned int height = 720, const char * title = "vk_exp");
	void destroy();
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
	std::pair<unsigned int, unsigned int> selectPhysicalDevice();
	void createLogicalDevice(unsigned int graphicsQIndex);
	void createSwapchain();
	void createCommandPool(unsigned int graphicsQIndex);
	void createFences();
	//Destroy
	void destroyFences();
	void destroyCommandPool();
	void destroySwapChain();
	void destroyLogicalDevice();
	void destroySurface();
	void destroyInstance();
	void destroyWindow();
};

#endif //__Context_h__
