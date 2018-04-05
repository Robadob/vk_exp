#include "Context.h"
#include <sstream>
#include <fstream>

/**
 * Public fns
 */
void Context::init(unsigned int width, unsigned int height, const char * title)
{
	if (isInit)
		return;
	try
	{
		//SDL_Vulkan_LoadLibrary(nullptr);
		//Create hidden window so we can init Vulkan context
		createWindow(width, height, title);
		//Create the basic vulkan instance
		createInstance(title);
#ifdef _DEBUG
		//Create debug output callbacks
		createDebugCallbacks();
#endif
		//Create a rendering surface for the window
		createSurface();
		//Select the most suitable GPU
		auto qs = selectPhysicalDevice();//0:graphicsQIndex, 1:presentQIndex
		//Create a logical device from the physical device
		createLogicalDevice(std::get<0>(qs));
		//Grab the graphical and present queues
		m_graphicsQueue = m_device.getQueue(std::get<0>(qs), 0);
		m_presentQueue = m_device.getQueue(std::get<1>(qs), 0);
		//Create semaphores for queue sync
		m_imageAvailableSemaphore = m_device.createSemaphore({});
		m_renderingFinishedSemaphore = m_device.createSemaphore({});
		//Create the swapchain for double/triple buffering (support for this isn't actually required by the spec???!)
		createSwapchain();
		//Create views for the swap chain images
		createSwapchainImages();
		//Create/Load pipeline cache
		setupPipelineCache();
		//Create the command pool and buffers
		createCommandPool(std::get<0>(qs));
		//Create memory fences for swapchain images
		createFences();
		SDL_ShowWindow(m_window);
		isInit = true;
	}
	catch (std::exception ex)
	{
		fprintf(stderr, "Vulkan Context init failed.\n%s\n", ex.what());
		destroy();
	}
}
void Context::destroy()
{
	if(m_window)
		SDL_HideWindow(m_window);
	destroyFences();
	destroyCommandPool();
	backupPipelineCache();
	destroyPipelineCache();
	destroySwapChainImages();
	destroySwapChain();
	if(m_renderingFinishedSemaphore)
	{
		m_device.destroySemaphore(m_renderingFinishedSemaphore);
		m_renderingFinishedSemaphore = nullptr;
	}
	if(m_imageAvailableSemaphore)
	{
		m_device.destroySemaphore(m_imageAvailableSemaphore);
		m_imageAvailableSemaphore = nullptr;
	}
	destroyLogicalDevice();
	destroySurface();
#ifdef _DEBUG
	destroyDebugCallbacks();
#endif
	destroyInstance();
	destroyWindow();
	//SDL_Vulkan_UnloadLibrary();
	isInit = false;
}
/**
 * Creation utility fns
 */
std::vector<const char*> Context::requiredInstanceExtensions() const
{
	unsigned int extensionCount = 0;
	std::vector<const char*> windowExtensions;
	if (!SDL_Vulkan_GetInstanceExtensions(m_window, &extensionCount, nullptr))
	{
		throw std::exception("requiredInstanceExtensions()1");
	}
	windowExtensions.resize(extensionCount);
	if (!SDL_Vulkan_GetInstanceExtensions(m_window, &extensionCount, windowExtensions.data()))
	{
		throw std::exception("requiredInstanceExtensions()2");
	}
#ifdef _DEBUG
	windowExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
#endif
	return windowExtensions;
}
void Context::createWindow(unsigned int width, unsigned int height, const char *title)
{
	m_window = SDL_CreateWindow
	(
		"vk_exp",
		SDL_WINDOWPOS_UNDEFINED, //xPos
		SDL_WINDOWPOS_UNDEFINED, //yPos
		width,
		height,
		SDL_WINDOW_HIDDEN | SDL_WINDOW_VULKAN //| SDL_WINDOW_BORDERLESS
	);
}
void Context::createInstance(const char * title)
{
	//Load list of minimum Vulkan extensions required for SDL surface creation
	std::vector<const char*> windowExtensions = requiredInstanceExtensions();
	//Create Vulkan instance
	auto appInfo = vk::ApplicationInfo();
	{
		appInfo.pApplicationName = title;
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "vk_exp";
		appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;
	}
#ifdef _DEBUG
	const std::vector<const char*> validationLayers = supportedValidationLayers();
#endif
	auto instanceCreateInfo = vk::InstanceCreateInfo();
	{
		instanceCreateInfo.flags = {};
		instanceCreateInfo.pApplicationInfo = &appInfo;
#ifdef _DEBUG
		instanceCreateInfo.enabledLayerCount = (unsigned int)validationLayers.size();
		instanceCreateInfo.ppEnabledLayerNames = validationLayers.data();
#else
		instanceCreateInfo.enabledLayerCount = 0;
		instanceCreateInfo.ppEnabledLayerNames = nullptr;
#endif
		instanceCreateInfo.enabledExtensionCount = (unsigned int)windowExtensions.size();
		instanceCreateInfo.ppEnabledExtensionNames = windowExtensions.data();
	}

	try
	{
		m_instance = vk::createInstance(instanceCreateInfo);//Can't modify extensions after instance load, so need to merge extensions lists
	}
	catch (vk::ExtensionNotPresentError ex)
	{
		fprintf(stderr, "Vulkan Context init failed, lacks extensions for window.\n%s\n", ex.what());
		//Validate/Debug log with required vs present extensions
		std::vector<vk::ExtensionProperties> a = vk::enumerateInstanceExtensionProperties();
		fprintf(stderr, "Supported Extensions:\n");
		for (auto &_a : a)
			fprintf(stderr, "\t%s: v%u\n", _a.extensionName, _a.specVersion);
		fprintf(stderr, "SDL Required Extensions:\n");
		for (auto &b : windowExtensions)
		{
			fprintf(stderr, "\t%s", b);
			bool found = false;
			for (auto &_a : a)
			{
				if (strcmp(_a.extensionName, b) == 0)
				{
					found = true;
					break;
				}
			}
			fprintf(stderr, "%s\n", found ? "" : " [MISSING]");
		}
		throw;
	}
}
void Context::createSurface()
{
	if (!SDL_Vulkan_CreateSurface(m_window, m_instance, reinterpret_cast<VkSurfaceKHR*>(&m_surface)))
	{
		fprintf(stderr,"%s\n",SDL_GetError());
		throw std::exception("createSurface()");
	}
}
std::pair<unsigned int, unsigned int> Context::selectPhysicalDevice()
{
	//Could improve this method to score available devices
	//https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Physical_devices_and_queue_families
	//Enumerate physical devices
	std::vector<vk::PhysicalDevice> physicalDevices = m_instance.enumeratePhysicalDevices();
	//Select most suitable physical device
	unsigned int chosenGraphicsQueueFamilyIndex = UINT_MAX;
	unsigned int chosenPresentQueueFamilyIndex = UINT_MAX;
	for (vk::PhysicalDevice &pd : physicalDevices)
	{
		bool hasSwapchainExtension = false;

		vk::PhysicalDeviceProperties pdp = pd.getProperties();
		if (VK_VERSION_MAJOR(pdp.apiVersion) < 1)
			continue;

		vk::PhysicalDeviceFeatures pdf = pd.getFeatures();
		std::vector<vk::QueueFamilyProperties> pdqf = pd.getQueueFamilyProperties();
		if (pdqf.empty())
			continue;
		unsigned int graphicsQueueFamilyIndex = UINT_MAX;
		unsigned int  presentQueueFamilyIndex = UINT_MAX;
		for (unsigned int q_index = 0; q_index < pdqf.size(); ++q_index)
		{
			vk::QueueFamilyProperties &_pdqf = pdqf[q_index];
			if (_pdqf.queueCount == 0)
				continue;
			if (_pdqf.queueFlags.operator&(vk::QueueFlags(VK_QUEUE_GRAPHICS_BIT)))
				graphicsQueueFamilyIndex = q_index;
			if (pd.getSurfaceSupportKHR(q_index, m_surface))
			{
				presentQueueFamilyIndex = q_index;
				if (_pdqf.queueFlags.operator&(vk::QueueFlags(VK_QUEUE_GRAPHICS_BIT)))
					break; // use this queue because it can present and do graphics
			}
		}
		if (graphicsQueueFamilyIndex == UINT_MAX) // no good queues found
			continue;
		if (presentQueueFamilyIndex == UINT_MAX) // no good queues found
			continue;
		std::vector<vk::ExtensionProperties> pde = pd.enumerateDeviceExtensionProperties();
		for (auto &_pde : pde)
		{
			if (0 == strcmp(_pde.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME))
			{
				hasSwapchainExtension = true;
				break;
			}
		}
		if (!hasSwapchainExtension)
			continue;
		m_physicalDevice = pd;
		chosenGraphicsQueueFamilyIndex = graphicsQueueFamilyIndex;
		chosenPresentQueueFamilyIndex = presentQueueFamilyIndex;
		break;
	}
	if (chosenGraphicsQueueFamilyIndex == UINT_MAX || chosenPresentQueueFamilyIndex == UINT_MAX)
	{//if(physicalDevice)
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Vulkan: no viable physical devices found");
		throw std::exception("selectPhysicalDevice()");
	}
	return std::make_pair(chosenGraphicsQueueFamilyIndex, chosenPresentQueueFamilyIndex);
}
void Context::createLogicalDevice(unsigned int graphicsQIndex)
{
	static const std::vector<const char*> deviceExtensionNames = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	};
	const float priority = 1.0f;
	auto deviceQueueCreateInfo = vk::DeviceQueueCreateInfo();
	{
		deviceQueueCreateInfo.flags = {};
		deviceQueueCreateInfo.queueFamilyIndex = graphicsQIndex;
		deviceQueueCreateInfo.queueCount = 1;
		deviceQueueCreateInfo.pQueuePriorities = &priority;
	}
	auto pdf = vk::PhysicalDeviceFeatures();
	{
		/**
		* Enable features like geometry shaders here
		*/
		//pdf.geometryShader = true;
	}
#ifdef _DEBUG
	const std::vector<const char*> validationLayers = supportedValidationLayers();
#endif
	auto deviceCreateInfo = vk::DeviceCreateInfo();
	{
		deviceCreateInfo.flags = {};
		deviceCreateInfo.queueCreateInfoCount = 1;
		deviceCreateInfo.pQueueCreateInfos = &deviceQueueCreateInfo;
#ifdef _DEBUG
		deviceCreateInfo.enabledLayerCount = (unsigned int)validationLayers.size();
		deviceCreateInfo.ppEnabledLayerNames = validationLayers.data();
#else
		deviceCreateInfo.enabledLayerCount = 0;
		deviceCreateInfo.ppEnabledLayerNames = nullptr;
#endif
		deviceCreateInfo.enabledExtensionCount = (unsigned int)deviceExtensionNames.size();
		deviceCreateInfo.ppEnabledExtensionNames = deviceExtensionNames.data();
		deviceCreateInfo.pEnabledFeatures = &pdf;
	}
	m_device = m_physicalDevice.createDevice(deviceCreateInfo);
}
void Context::createSwapchain()
{
	vk::SurfaceCapabilitiesKHR surfaceCap = m_physicalDevice.getSurfaceCapabilitiesKHR(m_surface);
	if (!(surfaceCap.supportedUsageFlags&(vk::ImageUsageFlags(VK_IMAGE_USAGE_TRANSFER_DST_BIT))))
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
			"Vulkan surface doesn't support VK_IMAGE_USAGE_TRANSFER_DST_BIT\n");
		throw std::exception("createSwapchain()1");
	}
	//getSurfaceFormats();
	std::vector<vk::SurfaceFormatKHR> surfaceFormats = m_physicalDevice.getSurfaceFormatsKHR(m_surface);
	//if (!createSwapchain())
	//	return SDL_FALSE;
	//Select an image count???
	unsigned int swapchainDesiredImageCount = surfaceCap.minImageCount + 1;
	if (swapchainDesiredImageCount > surfaceCap.maxImageCount && surfaceCap.maxImageCount > 0)
		swapchainDesiredImageCount = surfaceCap.maxImageCount;
	//Select a format
	if (surfaceFormats.size() == 1 && surfaceFormats[0].format == vk::Format::eUndefined)
	{
		// aren't any preferred formats, so we pick
		m_surfaceFormat.colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
		m_surfaceFormat.format = vk::Format::eR8G8B8A8Unorm;
	}
	else
	{
		m_surfaceFormat = surfaceFormats[0];
		for (auto &cf : surfaceFormats)
		{
			if (cf.format == vk::Format::eR8G8B8A8Unorm)
			{
				m_surfaceFormat = cf;
				break;
			}
		}
	}
	//Select size
	int windowWidth = 0, windowHeight = 0;
	SDL_Vulkan_GetDrawableSize(m_window, &windowWidth, &windowHeight);
	m_swapchainDims = vk::Extent2D(windowWidth, windowHeight);
	if (windowWidth == 0 || windowHeight == 0)
		throw std::exception("createSwapchain()2");
	auto swapChainCreateInfo = vk::SwapchainCreateInfoKHR();
	{
		swapChainCreateInfo.flags = {};
		swapChainCreateInfo.surface = m_surface;
		swapChainCreateInfo.minImageCount = swapchainDesiredImageCount;
		swapChainCreateInfo.imageFormat = m_surfaceFormat.format;
		swapChainCreateInfo.imageColorSpace = m_surfaceFormat.colorSpace;
		swapChainCreateInfo.imageExtent = m_swapchainDims;
		swapChainCreateInfo.imageArrayLayers = 1; //Always 1 unless stereo rendering
		swapChainCreateInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst;
		swapChainCreateInfo.imageSharingMode = vk::SharingMode::eExclusive,
		swapChainCreateInfo.queueFamilyIndexCount = 0;//Unnecessary for eExclusive
		swapChainCreateInfo.pQueueFamilyIndices = nullptr;
		swapChainCreateInfo.preTransform = surfaceCap.currentTransform;
		swapChainCreateInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
		swapChainCreateInfo.presentMode = vk::PresentModeKHR::eFifo;
		swapChainCreateInfo.clipped = true;
		swapChainCreateInfo.oldSwapchain = vk::SwapchainKHR();
	}
	m_swapchain = m_device.createSwapchainKHR(swapChainCreateInfo);
}
void Context::createSwapchainImages()
{
	m_scImages = m_device.getSwapchainImagesKHR(m_swapchain);
	m_scImageViews.resize(m_scImages.size());
	auto swizzleIdent = vk::ComponentMapping();
	{
		swizzleIdent.r = vk::ComponentSwizzle::eIdentity;
		swizzleIdent.g = vk::ComponentSwizzle::eIdentity;
		swizzleIdent.b = vk::ComponentSwizzle::eIdentity;
		swizzleIdent.a = vk::ComponentSwizzle::eIdentity;
	}
	auto subresourceTarget = vk::ImageSubresourceRange();
	{
		subresourceTarget.aspectMask = vk::ImageAspectFlagBits::eColor;
		subresourceTarget.baseMipLevel = 0;
		subresourceTarget.levelCount = 1;
		subresourceTarget.baseArrayLayer = 0;
		subresourceTarget.layerCount = 1;
	}
	auto imgCreate = vk::ImageViewCreateInfo();
	{
		imgCreate.flags = {};
		imgCreate.image = vk::Image();
		imgCreate.viewType = vk::ImageViewType::e2D;
		imgCreate.format = m_surfaceFormat.format;
		imgCreate.components = swizzleIdent;
		imgCreate.subresourceRange = subresourceTarget;
	}
	for (size_t i = 0; i < m_scImageViews.size(); i++)
	{
		imgCreate.image = m_scImages[i];
		m_scImageViews[i] = m_device.createImageView(imgCreate);
	}
}
void Context::setupPipelineCache()
{
	//Generate pipeline cache filename for current device
	std::string cachepath = pipelineCacheFilepath();

	//Check if it exists/is accessible
	std::ifstream f(cachepath, std::ios::binary | std::ios::ate);

	auto pipelineCacheCreate = vk::PipelineCacheCreateInfo();
	if (f.is_open()) 
	{
		//Load binary blob
		size_t fileSize = (size_t)f.tellg();
		std::vector<char> buffer(fileSize);

		f.seekg(0);
		f.read(buffer.data(), fileSize);

		f.close();
		//Create cache object from blob
		{
			pipelineCacheCreate.flags = {};
			pipelineCacheCreate.initialDataSize = buffer.size();
			pipelineCacheCreate.pInitialData = buffer.data();
			printf("Loaded pipeline cachefile %s\n", cachepath.c_str());
		}
	}
	else
	{
		//Create new empty pipeline cache
		{
			pipelineCacheCreate.flags = {};
			pipelineCacheCreate.initialDataSize = 0;
			pipelineCacheCreate.pInitialData = nullptr;
		}
	}
	m_pipelineCache = m_device.createPipelineCache(pipelineCacheCreate);
}
void Context::createCommandPool(unsigned int graphicsQIndex)
{
	// createCommandPool();
	auto commandPoolCreateInfo = vk::CommandPoolCreateInfo();
	{
		commandPoolCreateInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer | vk::CommandPoolCreateFlagBits::eTransient;
		commandPoolCreateInfo.queueFamilyIndex = graphicsQIndex;
	}
	m_commandPool = m_device.createCommandPool(commandPoolCreateInfo);
	//createCommandBuffers();
	auto commandBufferAllocInfo = vk::CommandBufferAllocateInfo();
	{
		commandBufferAllocInfo.commandPool = m_commandPool;
		commandBufferAllocInfo.level = vk::CommandBufferLevel::ePrimary;
		commandBufferAllocInfo.commandBufferCount = (unsigned int)m_scImages.size();
	}
	m_commandBuffers = m_device.allocateCommandBuffers(commandBufferAllocInfo);
}
void Context::createFences()
{
	m_fences.resize(m_scImages.size());
	for (unsigned int i = 0; i < m_scImages.size(); i++)
	{
		try
		{
			m_fences[i] = m_device.createFence({ vk::FenceCreateFlagBits::eSignaled });
		}
		catch (...)
		{
			for (; i > 0; i--)
			{
				m_device.destroyFence(m_fences[i - 1]);
			}
			m_fences.clear();
			throw;
		}
	}
}
/**
 * Destruction utility fns
 */
void Context::destroyFences()
{
	for (auto &fence : m_fences)
	{
		m_device.destroyFence(fence);
	}
	m_fences.clear();
}
void Context::destroyCommandPool()
{
	if (m_commandBuffers.size()&& m_commandPool)
	{
		m_device.freeCommandBuffers(m_commandPool, (unsigned int)m_commandBuffers.size(), m_commandBuffers.data());
		m_commandBuffers.clear();
	}
	if(m_commandPool)
	{
		m_device.destroyCommandPool(m_commandPool);
		m_commandPool = nullptr;
	}
}
void Context::backupPipelineCache()
{	
	//Generate pipeline cache filename for current device
	std::string cachepath = pipelineCacheFilepath();
	//Check if it exists/is accessible
	std::ofstream f(cachepath, std::ios::binary | std::ios::trunc);	
	if (f.is_open())
	{
		//Grab binary blob
		std::vector<unsigned char> buffer = m_device.getPipelineCacheData(m_pipelineCache);
		//Write binary blob
		f.write((char*)buffer.data(), buffer.size());

		f.close();
		fprintf(stderr, "Updated pipeline cache file '%s'.\n", cachepath.c_str());
	}
	else
		fprintf(stderr, "Failed to open file '%s' to update pipeline cache.\n", cachepath.c_str());
}
void Context::destroyPipelineCache()
{
	m_device.destroyPipelineCache(m_pipelineCache);
	m_pipelineCache = nullptr;
}

void Context::destroySwapChainImages()
{
	for (auto &a : m_scImageViews)
		m_device.destroyImageView(a);
	m_scImageViews.clear();
	m_scImages.clear();
}
void Context::destroySwapChain()
{
	if(m_swapchain)
	{
		m_device.destroySwapchainKHR(m_swapchain);
		m_swapchain = nullptr;
	}
	m_swapchainDims = vk::Extent2D(0, 0);
	m_surfaceFormat = vk::SurfaceFormatKHR();
}
void Context::destroyLogicalDevice()
{
	if(m_device)
	{
		m_device.waitIdle();
		m_device.destroy();
		m_device = nullptr;
	}
}
void Context::destroySurface()
{
	if(m_instance&&m_surface)
	{
		m_instance.destroySurfaceKHR(m_surface);
		m_surface = nullptr;
	}
}
void Context::destroyInstance()
{
	if(m_instance)
	{
		m_instance.destroy();
		m_instance = nullptr;
	}
}
void Context::destroyWindow()
{
	if(m_window)
	{
		SDL_DestroyWindow(m_window);
		m_window = nullptr;
	}
}
/**
 * Debug fns
 */
#ifdef _DEBUG
static VKAPI_ATTR VkBool32 VKAPI_CALL debugLayerCallback(
	VkDebugReportFlagsEXT flags,
	VkDebugReportObjectTypeEXT objType,
	uint64_t obj,
	size_t location,
	int32_t code,
	const char* layerPrefix,
	const char* msg,
	void* userData) {

	fprintf(stderr, "Validation Layer: %s\n", msg);

	return VK_FALSE;
}
std::vector<const char*> Context::supportedValidationLayers()
{
	static const std::vector<const char*> VALIDATION_LAYERS = {
		"VK_LAYER_LUNARG_standard_validation"
	};
	std::vector<const char*> rtn;
	std::vector<vk::LayerProperties> layerProps = vk::enumerateInstanceLayerProperties();
	for (auto &dl : VALIDATION_LAYERS)
	{
		for (auto &lp : layerProps)
		{
			if (strcmp(dl, lp.layerName) == 0)
			{
				rtn.push_back(dl);
				goto outer_loop_validation_layers;
			}
		}
		fprintf(stderr, "Validation Layer '%s' was not found.\n", dl);
	outer_loop_validation_layers:;
	}
	return rtn;
}
void Context::createDebugCallbacks()
{
	vk::DebugReportCallbackCreateInfoEXT debugCallbackCreateInfo(
		vk::DebugReportFlagBitsEXT::eError | vk::DebugReportFlagBitsEXT::eWarning,
		debugLayerCallback
	);
	//Must dynamically load extension fn
	//m_debugCallback = m_instance.createDebugReportCallbackEXT(debugCallbackCreateInfo);
	auto func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(m_instance, "vkCreateDebugReportCallbackEXT");
	if (func)
	{
		VkResult result = func(m_instance, reinterpret_cast<VkDebugReportCallbackCreateInfoEXT*>(&debugCallbackCreateInfo), nullptr, reinterpret_cast<VkDebugReportCallbackEXT*>(&m_debugCallback));
		if (result != VK_SUCCESS)
			fprintf(stderr, "Error occurred during dynamic call of 'vkCreateDebugReportCallbackEXT'\n");
	}
	else
		fprintf(stderr,"Could not load extension for vkCreateDebugReportCallbackEXT!\n");
}
void Context::destroyDebugCallbacks()
{
	//Must dynamically load extension fn
	//m_instance.destroyDebugReportCallbackEXT(m_debugCallback);
	auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(m_instance, "vkDestroyDebugReportCallbackEXT");
	if (func)
	{
		func(m_instance, *reinterpret_cast<VkDebugReportCallbackEXT_T**>(&m_debugCallback), nullptr);		
	}
	else
		fprintf(stderr, "Could not load extension for vkDestroyDebugReportCallbackEXT!\n");
	m_debugCallback = nullptr;
}
#endif


void Context::createGraphicsPipeline()
{
	
}


std::string Context::pipelineCacheFilepath()
{
	if (!m_physicalDevice)
		throw std::exception("Pipeline cache filename requires knowledge of device.");
	//Get physical device vendorId/deviceId
	auto deviceProp = m_physicalDevice.getProperties();
	std::ostringstream cachepath;
	cachepath << "pipeline";
	cachepath << deviceProp.vendorID;
	cachepath << ".";
	cachepath << deviceProp.deviceID;
	cachepath << ".cache";
	return cachepath.str();
}