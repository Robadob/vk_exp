#include "Context.h"
#include <sstream>
#include <fstream>
#include "GraphicsPipeline.h"
#include <SDL/SDL_vulkan.h>
#include "vk.h"
#include <set>
#include <chrono>
#include <glm/gtc/matrix_transform.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#include "model/Model.h"

/**
 * Public fns
 */
void Context::init(unsigned int width, unsigned int height, const char * title)
{
	if (isInit.load())
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
		m_graphicsQueueId = std::get<0>(qs);
		m_presentQueueId = std::get<1>(qs);
		//Create a logical device from the physical device
		createLogicalDevice(m_graphicsQueueId, m_presentQueueId);
		//Grab the graphical and present queues
		m_graphicsQueue = m_device.getQueue(m_graphicsQueueId, 0);
		m_presentQueue = m_device.getQueue(m_presentQueueId, 0);
		//Create/Load pipeline cache
		setupPipelineCache();
		//Create Swapchain and dependencies
		createSwapchainStuff();
		//Create semaphores for queue sync
		m_imageAvailableSemaphore = m_device.createSemaphore({});
		m_renderingFinishedSemaphore = m_device.createSemaphore({});
		//Create memory fences for swapchain images
		createFences();
		createTextureImage();
		createTextureImageView();
		createTextureSampler();
		//Build a Vertex Buf with model data
		createVertexBuffer();
		createIndexBuffer();
		createUniformBuffer();
		updateDescriptorSet();

		m_assimpModel = new Model(*this, "C:\\Users\\Robadob\\Downloads\\assimp-3.3.1\\test\\models-nonbsd\\md5\\Bob.md5mesh", 1.0f);

		SDL_ShowWindow(m_window);
		isInit.store(true);
		/**
		 * Start Drawing!
		 **/
		fillCommandBuffers();
	}
	catch (std::exception ex)
	{
		fprintf(stderr, "Vulkan Context init failed.\n%s\n", ex.what());
		getchar();
		destroy();
	}
}
void Context::destroy()
{
	if(m_window)
		SDL_HideWindow(m_window);
	m_presentQueue.waitIdle();
	destroyVertexBuffer();
	destroyIndexBuffer();
	destroyUniformBuffer();
	destroyTextureSampler();
	destroyTextureImageView();
	destroyTextureImage();
	destroyFences();
	if (m_renderingFinishedSemaphore)
	{
		m_device.destroySemaphore(m_renderingFinishedSemaphore);
		m_renderingFinishedSemaphore = nullptr;
	}
	if (m_imageAvailableSemaphore)
	{
		m_device.destroySemaphore(m_imageAvailableSemaphore);
		m_imageAvailableSemaphore = nullptr;
	}
	backupPipelineCache();
	destroyPipelineCache();
	destroySwapchainStuff();
	destroyLogicalDevice();
	destroySurface();
#ifdef _DEBUG
	destroyDebugCallbacks();
#endif
	destroyInstance();
	destroyWindow();
	isInit.store(false);
}

void Context::rebuildSwapChain()
{
	if (ready())
	{
		m_device.waitIdle();
		destroySwapchainStuff();
		createSwapchainStuff(); 
		fillCommandBuffers();
	}
}
/**
 * Creation utility fns
 */
void Context::createSwapchainStuff()
{
	//Create the swapchain for double/triple buffering (support for this isn't actually required by the spec???!)
	createSwapchain();
	//Create views for the swap chain images
	createSwapchainImages();
	//Create GFX pipeline? (framebuffer/commandpool dependent on this for renderpass)
	createGraphicsPipeline();
	//Create the descriptor pool (memory behind uniforms in shaders)
	createDescriptorPool();//Relies on descriptor layouts (gfx pipeline)
	//Create the command pool
	createCommandPool(m_graphicsQueueId);
	//Create the depth buffer stuff
	createDepthResources();
	//Create Framebuffer
	createFramebuffers();
	//Create the command buffers, begin Renderpasses (inside each command buff)
	createCommandBuffers();
}
void Context::destroySwapchainStuff()
{
	destroyCommandPool();
	destroyFramebuffers();
	destroyDepthResources();
	destroyDescriptorPool();
	delete m_gfxPipeline;
	m_gfxPipeline = nullptr;
	destroySwapChainImages();
	destroySwapChain();
}
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
	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
		return;
	}
	m_window = SDL_CreateWindow
	(
		"vk_exp",
		SDL_WINDOWPOS_UNDEFINED, //xPos
		SDL_WINDOWPOS_UNDEFINED, //yPos
		width,
		height,
		SDL_WINDOW_HIDDEN | SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE//| SDL_WINDOW_BORDERLESS
	);
}
void Context::createInstance(const char * title)
{
	//Create Vulkan instance
	vk::ApplicationInfo appInfo;
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
	//Load list of minimum Vulkan extensions required for SDL surface creation
	std::vector<const char*> windowExtensions = requiredInstanceExtensions();
	vk::InstanceCreateInfo instanceCreateInfo;
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
		m_dynamicLoader = vk::DispatchLoaderDynamic(m_instance);
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
		m_physicalDeviceFeatures = pd.getFeatures();
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
void Context::createLogicalDevice(unsigned int graphicsQIndex, unsigned int presentQIndex)
{
	static const std::vector<const char*> deviceExtensionNames = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	};
	const float priority = 1.0f;
	std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
	std::set<unsigned int> uniqueQueueFamilies = { graphicsQIndex, presentQIndex };
	for (int queueFamily : uniqueQueueFamilies) {
		vk::DeviceQueueCreateInfo deviceQueueCreateInfo;
		{//Graphics q
			deviceQueueCreateInfo.flags = {};
			deviceQueueCreateInfo.queueFamilyIndex = queueFamily;
			deviceQueueCreateInfo.queueCount = 1;
			deviceQueueCreateInfo.pQueuePriorities = &priority;
		}
		queueCreateInfos.push_back(deviceQueueCreateInfo);
	}
	vk::PhysicalDeviceFeatures pdf;
	{
		/**
		* Enable features like geometry shaders here
		*/
		pdf.samplerAnisotropy = m_physicalDeviceFeatures.samplerAnisotropy;
		//pdf.geometryShader = true;
	}
#ifdef _DEBUG
	const std::vector<const char*> validationLayers = supportedValidationLayers();
#endif
	auto deviceCreateInfo = vk::DeviceCreateInfo();
	{
		deviceCreateInfo.flags = {};
		deviceCreateInfo.queueCreateInfoCount = (unsigned int)queueCreateInfos.size();
		deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
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
    m_dynamicLoader = vk::DispatchLoaderDynamic(m_instance, m_device);
}
vk::PresentModeKHR Context::selectPresentMode()
{//https://vulkan.lunarg.com/doc/view/1.0.26.0/linux/vkspec.chunked/ch29s05.html#VkPresentModeKHR
	std::vector<vk::PresentModeKHR> pm = m_physicalDevice.getSurfacePresentModesKHR(m_surface);
	vk::PresentModeKHR bestMode = vk::PresentModeKHR::eFifo;//Vsync
	for (const auto& availablePresentMode : pm) {
		if (availablePresentMode == vk::PresentModeKHR::eMailbox) {//Vsync where fast frames cause frames to be skipped if 2nd is delivered before vsync barrier
			return availablePresentMode;
		}
		//else if (availablePresentMode == vk::PresentModeKHR::eImmediate) {//Immediate mode, not support on NV?
		//	bestMode = availablePresentMode;
		//}
	}
	return bestMode;
}
void Context::createDescriptorPool()
{
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
	m_descriptorPool = m_device.createDescriptorPool(poolCreateInfo);
	assert(m_gfxPipeline->Layout().descriptorSetLayoutsSize() == 1);
	vk::DescriptorSetAllocateInfo descSetAllocInfo;
	{
		descSetAllocInfo.descriptorPool = m_descriptorPool;
		descSetAllocInfo.descriptorSetCount = m_gfxPipeline->Layout().descriptorSetLayoutsSize();
		descSetAllocInfo.pSetLayouts = m_gfxPipeline->Layout().descriptorSetLayouts();
	}
	m_descriptorSet = m_device.allocateDescriptorSets(descSetAllocInfo)[0];
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

	vk::SwapchainCreateInfoKHR swapChainCreateInfo;
	{
		swapChainCreateInfo.flags = {};
		swapChainCreateInfo.surface = m_surface;
		swapChainCreateInfo.minImageCount = swapchainDesiredImageCount;
		swapChainCreateInfo.imageFormat = m_surfaceFormat.format;
		swapChainCreateInfo.imageColorSpace = m_surfaceFormat.colorSpace;
		swapChainCreateInfo.imageExtent = m_swapchainDims;
		swapChainCreateInfo.imageArrayLayers = 1; //Always 1 unless stereo rendering
		swapChainCreateInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
		swapChainCreateInfo.imageSharingMode = vk::SharingMode::eExclusive,
		swapChainCreateInfo.queueFamilyIndexCount = 0;//Unnecessary for eExclusive
		swapChainCreateInfo.pQueueFamilyIndices = nullptr;
		swapChainCreateInfo.preTransform = surfaceCap.currentTransform;
		swapChainCreateInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
		swapChainCreateInfo.presentMode = selectPresentMode();
		swapChainCreateInfo.clipped = true;
		swapChainCreateInfo.oldSwapchain = nullptr;
	}
	m_swapchain = m_device.createSwapchainKHR(swapChainCreateInfo);
}
void Context::createSwapchainImages()
{
	m_scImages = m_device.getSwapchainImagesKHR(m_swapchain);
	m_scImageViews.resize(m_scImages.size());
	for (size_t i = 0; i < m_scImageViews.size(); i++)
	{
		m_scImageViews[i] = createImageView(m_scImages[i], m_surfaceFormat.format, vk::ImageAspectFlagBits::eColor);
	}
}
void Context::createFramebuffers()
{
	m_scFramebuffers.resize(m_scImageViews.size());
	for (size_t i = 0; i < m_scImageViews.size(); i++) {
		//Can reuse depth as only one subpass operates at a time, due to use of semaphores
		std::array<vk::ImageView, 2> attachments = { m_scImageViews[i], m_depthImageView };
		vk::FramebufferCreateInfo fbCreate;
		{
			fbCreate.renderPass = m_gfxPipeline->RenderPass();
			fbCreate.attachmentCount = (unsigned int)attachments.size();
			fbCreate.pAttachments = attachments.data();
			fbCreate.width = m_swapchainDims.width;
			fbCreate.height = m_swapchainDims.height;
			fbCreate.layers = 1;
		}
		m_scFramebuffers[i] = m_device.createFramebuffer(fbCreate);
	}
}
void Context::setupPipelineCache()
{
	//Generate pipeline cache filename for current device
	std::string cachepath = pipelineCacheFilepath();

	//Check if it exists/is accessible
	std::ifstream f(cachepath, std::ios::binary | std::ios::ate);

	vk::PipelineCacheCreateInfo pipelineCacheCreate;
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
	vk::CommandPoolCreateInfo commandPoolCreateInfo;
	{
		commandPoolCreateInfo.flags = {};// vk::CommandPoolCreateFlagBits::eResetCommandBuffer | vk::CommandPoolCreateFlagBits::eTransient;
		commandPoolCreateInfo.queueFamilyIndex = graphicsQIndex;
	}
	m_commandPool = m_device.createCommandPool(commandPoolCreateInfo);
	//createCommandBuffers();
}
void Context::createDepthResources()
{
	vk::Format depthFormat = findDepthFormat();
	createImage(m_swapchainDims.width, m_swapchainDims.height, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal, m_depthImage, m_depthImageMemory);
	m_depthImageView = createImageView(m_depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth);
	transitionImageLayout(m_depthImage, depthFormat, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal);
}
void Context::createCommandBuffers()
{
	vk::CommandBufferAllocateInfo commandBufferAllocInfo;
	{
		commandBufferAllocInfo.commandPool = m_commandPool;
		commandBufferAllocInfo.level = vk::CommandBufferLevel::ePrimary;
		commandBufferAllocInfo.commandBufferCount = (unsigned int)m_scFramebuffers.size();
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
void Context::fillCommandBuffers()
{
	for (unsigned int i = 0; i<m_commandBuffers.size(); ++i)
	{
		vk::CommandBufferBeginInfo cbBegin;
		{
			cbBegin.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;//Command buffer can be resubmitted whilst awaiting execution
			cbBegin.pInheritanceInfo = nullptr;
		}
		m_commandBuffers[i].begin(cbBegin);
		vk::RenderPassBeginInfo rpBegin;
		std::array<vk::ClearValue, 2> clearValues = {};
		clearValues[0].color = vk::ClearColorValue(std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f });
		clearValues[1].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);
		{
			rpBegin.renderPass = m_gfxPipeline->RenderPass();
			rpBegin.framebuffer = m_scFramebuffers[i];
			rpBegin.renderArea.offset = vk::Offset2D({ 0, 0 });
			rpBegin.renderArea.extent = m_swapchainDims;
			rpBegin.clearValueCount = (unsigned int)clearValues.size();
			rpBegin.pClearValues = clearValues.data();
		}
		m_commandBuffers[i].beginRenderPass(rpBegin, vk::SubpassContents::eInline);
		m_commandBuffers[i].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_gfxPipeline->Layout().get(), 0, { m_descriptorSet }, {});
		//m_commandBuffers[i].bindPipeline(vk::PipelineBindPoint::eGraphics, m_gfxPipeline->Pipeline());
		//VkDeviceSize offsets[] = { 0 };
		//m_commandBuffers[i].bindVertexBuffers(0, 1, &m_vertexBuffer, offsets);
		//m_commandBuffers[i].bindIndexBuffer(m_indexBuffer, 0, vk::IndexType::eUint16);
		////m_commandBuffers[i].draw((unsigned int)tempVertices.size(), 1, 0, 0);//Drawing triangles without index
		//m_commandBuffers[i].drawIndexed((unsigned int)tempIndices.size(), 1, 0, 0, 0);
		m_assimpModel->render(m_commandBuffers[i], *m_gfxPipeline);
		m_commandBuffers[i].endRenderPass();
		m_commandBuffers[i].end();
	}
}
void Context::createTextureImage()
{
	int texWidth, texHeight, texChannels;
	stbi_uc* pixels = stbi_load("../shaders/test.png", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	VkDeviceSize imageSize = texWidth * texHeight * 4;

	if (!pixels) {
		throw std::runtime_error("failed to load texture image!");
	}

	vk::Buffer stagingBuffer;
	vk::DeviceMemory stagingBufferMemory;
	createBuffer(
		imageSize,
		vk::BufferUsageFlagBits::eTransferSrc,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
		stagingBuffer,
		stagingBufferMemory
	); 
	void* data;
	m_device.mapMemory(stagingBufferMemory, 0, imageSize, {}, &data);
	memcpy(data, pixels, imageSize);
	m_device.unmapMemory(stagingBufferMemory); 
	stbi_image_free(pixels);
	createImage(
		(uint32_t)texWidth,
		(uint32_t)texHeight,
		vk::Format::eR8G8B8A8Unorm,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
		vk::MemoryPropertyFlagBits::eDeviceLocal,
		m_textureImage,
		m_textureImageMemory
	);
	transitionImageLayout(
		m_textureImage,
		vk::Format::eR8G8B8A8Unorm,
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::eTransferDstOptimal
	);
	copyBufferToImage(
		stagingBuffer,
		m_textureImage,
		(uint32_t)texWidth,
		(uint32_t)texHeight
	);
	transitionImageLayout(
		m_textureImage,
		vk::Format::eR8G8B8A8Unorm,
		vk::ImageLayout::eTransferDstOptimal,
		vk::ImageLayout::eShaderReadOnlyOptimal
	);
	m_device.destroyBuffer(stagingBuffer);
	m_device.freeMemory(stagingBufferMemory);
}
void Context::createTextureImageView()
{
	m_textureImageView = createImageView(m_textureImage, vk::Format::eR8G8B8A8Unorm, vk::ImageAspectFlagBits::eColor);
}
void Context::createTextureSampler()
{
	vk::SamplerCreateInfo samplerInfo;
	{
		samplerInfo.magFilter = vk::Filter::eLinear;
		samplerInfo.minFilter = vk::Filter::eLinear;
		samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
		samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
		samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
		samplerInfo.anisotropyEnable = m_physicalDeviceFeatures.samplerAnisotropy;
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
	m_textureSampler = m_device.createSampler(samplerInfo);
}
void Context::createVertexBuffer()
{
	size_t buffSize = sizeof(tempVertices[0])*tempVertices.size();
	//Transfer queue data transfer
	vk::Buffer stagingBuffer;
	vk::DeviceMemory stagingBufferMemory;
	createBuffer(
		buffSize,
		vk::BufferUsageFlagBits::eTransferSrc,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
		stagingBuffer,
		stagingBufferMemory
	);
	void* data;
	m_device.mapMemory(stagingBufferMemory, 0, buffSize, {}, &data);
	memcpy(data, tempVertices.data(), buffSize);
	m_device.unmapMemory(stagingBufferMemory);
	createBuffer(
		buffSize,
		vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
		vk::MemoryPropertyFlagBits::eDeviceLocal,
		m_vertexBuffer,
		m_vertexBufferMemory
	);
	copyBuffer(stagingBuffer, m_vertexBuffer, buffSize);
	m_device.destroyBuffer(stagingBuffer);
	m_device.freeMemory(stagingBufferMemory);
	//Mapped buffer data transfer
	/*
	createBuffer(
		buffSize,
		vk::BufferUsageFlagBits::eVertexBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
		m_vertexBuffer,
		m_vertexBufferMemory
	);
	//Map Device Buffer to Host
	void* data;
	m_device.mapMemory(m_vertexBufferMemory, 0, buffSize, {}, &data);
		memcpy(data, tempVertices.data(), buffSize);
	m_device.unmapMemory(m_vertexBufferMemory);
    */
}
void Context::createIndexBuffer()
{
	size_t buffSize = sizeof(tempIndices[0])*tempIndices.size();
	//Transfer queue data transfer
	vk::Buffer stagingBuffer;
	vk::DeviceMemory stagingBufferMemory;
	createBuffer(
		buffSize,
		vk::BufferUsageFlagBits::eTransferSrc,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
		stagingBuffer,
		stagingBufferMemory
	);
	void* data;
	m_device.mapMemory(stagingBufferMemory, 0, buffSize, {}, &data);
	memcpy(data, tempIndices.data(), buffSize);
	m_device.unmapMemory(stagingBufferMemory);
	createBuffer(
		buffSize,
		vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
		vk::MemoryPropertyFlagBits::eDeviceLocal,
		m_indexBuffer,
		m_indexBufferMemory
	);
	copyBuffer(stagingBuffer, m_indexBuffer, buffSize);
	m_device.destroyBuffer(stagingBuffer);
	m_device.freeMemory(stagingBufferMemory);
}
void Context::createUniformBuffer()
{
	size_t buffSize = sizeof(UniformBufferObject);
	//Transfer queue data transfer
	createBuffer(
		buffSize,
		vk::BufferUsageFlagBits::eUniformBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
		m_uniformBuffer,
		m_uniformBufferMemory
	);
}
void Context::updateDescriptorSet()
{
	vk::DescriptorBufferInfo bufferInfo;
	{
		bufferInfo.buffer = m_uniformBuffer;
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UniformBufferObject);
	}
	vk::DescriptorImageInfo imageInfo;
	{
		imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		imageInfo.imageView = m_textureImageView;
		imageInfo.sampler = m_textureSampler;
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
	m_device.updateDescriptorSets((unsigned int)descWrites.size(), descWrites.data(), 0, nullptr);
}
/**
 * Destruction utility fns
 */
void Context::destroyVertexBuffer()
{
	m_device.destroyBuffer(m_vertexBuffer);
	m_vertexBuffer = nullptr;
	m_device.freeMemory(m_vertexBufferMemory);
	m_vertexBufferMemory = nullptr;
}
void Context::destroyIndexBuffer()
{
	m_device.destroyBuffer(m_indexBuffer);
	m_indexBuffer = nullptr;
	m_device.freeMemory(m_indexBufferMemory);
	m_indexBufferMemory = nullptr;
}
void Context::destroyUniformBuffer()
{
	m_device.destroyBuffer(m_uniformBuffer);
	m_uniformBuffer = nullptr;
	m_device.freeMemory(m_uniformBufferMemory);
	m_uniformBufferMemory = nullptr;
}
void Context::destroyTextureSampler()
{
	m_device.destroySampler(m_textureSampler);
	m_textureSampler = nullptr;
}
void Context::destroyTextureImageView()
{
	m_device.destroyImageView(m_textureImageView);
	m_textureImageView = nullptr;
}
void Context::destroyTextureImage()
{
	m_device.destroyImage(m_textureImage);
	m_textureImage = nullptr;
	m_device.freeMemory(m_textureImageMemory);
	m_textureImageMemory = nullptr;
}
void Context::destroyFences()
{
	for (auto &fence : m_fences)
	{
		m_device.destroyFence(fence);
	}
	m_fences.clear();
}
void Context::destroyDepthResources()
{
	m_device.destroyImageView(m_depthImageView);
	m_device.destroyImage(m_depthImage);
	m_device.freeMemory(m_depthImageMemory);
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
void Context::destroyFramebuffers()
{
	for(auto &fb : m_scFramebuffers)
	{
		m_device.destroyFramebuffer(fb);
	}
	m_scFramebuffers.clear();
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
void Context::destroyDescriptorPool()
{
	m_device.destroyDescriptorPool(m_descriptorPool);
	m_descriptorPool = nullptr;
	m_descriptorSet = nullptr;
}
void Context::destroyLogicalDevice()
{
	if(m_device)
	{
		m_device.waitIdle();
		m_dynamicLoader = vk::DispatchLoaderDynamic(m_instance);
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
		m_dynamicLoader = vk::DispatchLoaderDynamic();
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
	SDL_Quit();
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
    if(m_dynamicLoader.vkCreateDebugReportCallbackEXT)
    {
        m_debugCallback = m_instance.createDebugReportCallbackEXT(debugCallbackCreateInfo, nullptr, m_dynamicLoader);
    }
    else
        fprintf(stderr, "Could not load extension for vkCreateDebugReportCallbackEXT!\n");
}
void Context::destroyDebugCallbacks()
{
	//Must dynamically load extension fn
    if (m_dynamicLoader.vkDestroyDebugReportCallbackEXT)
	    m_instance.destroyDebugReportCallbackEXT(m_debugCallback, nullptr, m_dynamicLoader);
	m_debugCallback = nullptr;
}
#endif

void Context::createGraphicsPipeline()
{
	m_gfxPipeline = new GraphicsPipeline(*this,"../shaders/vert.spv","../shaders/frag.spv");
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
void Context::updateUniformBuffer()
{
	static auto startTime = std::chrono::high_resolution_clock::now();

	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	//Spin around z axis
	UniformBufferObject ubo = {};
	ubo.model = glm::mat4(1);// glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.view = e_viewMat ? *e_viewMat : glm::mat4();
	ubo.proj = glm::perspective(glm::radians(45.0f), m_swapchainDims.width / (float)m_swapchainDims.height, 0.1f, 10.0f);
	ubo.proj[1][1] *= -1;
	//Copy to uniform buffer
	void* data = m_device.mapMemory(m_uniformBufferMemory, 0, sizeof(ubo), {});
	memcpy(data, &ubo, sizeof(ubo));
	m_device.unmapMemory(m_uniformBufferMemory);
}
void Context::getNextImage()
{
	try
	{
		vk::ResultValue<uint32_t> imageIndex = m_device.acquireNextImageKHR(m_swapchain, std::numeric_limits<uint64_t>::max(), m_imageAvailableSemaphore, nullptr);
		if (imageIndex.result == vk::Result::eSuccess)
		{
			uint32_t i = imageIndex.value;
			//Success
			//Submit command buffer and setup semaphores to flag ready
			vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
			auto submitInfo = vk::SubmitInfo();
			{
				submitInfo.waitSemaphoreCount = 1;
				submitInfo.pWaitSemaphores = &m_imageAvailableSemaphore;
				submitInfo.pWaitDstStageMask = &waitStage;
				submitInfo.commandBufferCount = 1;
				submitInfo.pCommandBuffers = &m_commandBuffers[i];
				submitInfo.signalSemaphoreCount = 1;
				submitInfo.pSignalSemaphores = &m_renderingFinishedSemaphore;
			}
			vk::Result a = m_graphicsQueue.submit(1, &submitInfo, nullptr);
			auto presentInfo = vk::PresentInfoKHR();
			{
				presentInfo.waitSemaphoreCount = 1;
				presentInfo.pWaitSemaphores = &m_renderingFinishedSemaphore;
				presentInfo.swapchainCount = 1;
				presentInfo.pSwapchains = &m_swapchain;
				presentInfo.pImageIndices = &i;
				presentInfo.pResults = nullptr;
			}
			vk::Result b = m_presentQueue.presentKHR(&presentInfo);
			//if (a != vk::Result::eSuccess)
			//	fprintf(stderr, "m_graphicsQueue.submit(): %s\n", getVulkanResultString(a));
			//if (b != vk::Result::eSuccess)
			//	fprintf(stderr, "m_presentQueue.presentKHR(): %s\n", getVulkanResultString(b));
#ifdef _DEBUG
			m_presentQueue.waitIdle();
#endif
		}
		else
		{
			vk::throwResultException(imageIndex.result, getVulkanResultString(imageIndex.result));
		}
	}
	catch(vk::IncompatibleDisplayKHRError&)
	{
		rebuildSwapChain();
	}
	catch (vk::OutOfDateKHRError&)
	{
		rebuildSwapChain();
	}
	catch(std::system_error &err)
	{
		fprintf(stderr, "%s\n", err.what());
		getchar();
	}
}
void Context::toggleFullScreen()
{
	if (this->isFullscreen()) {
		// Update the window using the stored windowBounds
		SDL_SetWindowBordered(m_window, SDL_TRUE);
		SDL_SetWindowResizable(m_window, SDL_TRUE);
		SDL_SetWindowSize(m_window, m_windowedBounds.w, m_windowedBounds.h);
		SDL_SetWindowPosition(m_window, m_windowedBounds.x, m_windowedBounds.y);
	}
	else {
		// Store the windowedBounds for later
		SDL_GetWindowPosition(m_window, &m_windowedBounds.x, &m_windowedBounds.y);
		SDL_GetWindowSize(m_window, &m_windowedBounds.w, &m_windowedBounds.h);
		// Get the window bounds for the current screen
		int displayIndex = SDL_GetWindowDisplayIndex(m_window);
		SDL_Rect displayBounds;
		SDL_GetDisplayBounds(displayIndex, &displayBounds);
		// Update the window
		SDL_SetWindowBordered(m_window, SDL_FALSE);
		SDL_SetWindowResizable(m_window, SDL_FALSE);
		SDL_SetWindowPosition(m_window, displayBounds.x, displayBounds.y);
		SDL_SetWindowSize(m_window, displayBounds.w, displayBounds.h);
	}
	rebuildSwapChain();
}
bool Context::isFullscreen()
{
	// Use window borders as a proxy to detect fullscreen.
	return (SDL_GetWindowFlags(m_window) & SDL_WINDOW_BORDERLESS) == SDL_WINDOW_BORDERLESS;
}
unsigned int Context::findMemoryType(const unsigned int &typeFilter, const vk::MemoryPropertyFlags& properties) const
{
	vk::PhysicalDeviceMemoryProperties memoryProps = m_physicalDevice.getMemoryProperties();
	for (unsigned int i = 0; i < memoryProps.memoryTypeCount; ++i)
	{
		if (typeFilter & (1 << i))
		{//Correct type available
			if ((memoryProps.memoryTypes[i].propertyFlags & properties) == properties)
			{//And it has the right properties (e.g. host_coherent, can be mapped to host memory)
				return i;
			}
		}
	}
	throw std::runtime_error("failed to find suitable memory type!");
}
vk::Format Context::findSupportedFormat(const std::vector<vk::Format>& candidates, const vk::ImageTiling &tiling, vk::FormatFeatureFlags features)
{
	for (vk::Format format : candidates) 
	{
		vk::FormatProperties props = m_physicalDevice.getFormatProperties(format);
		if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features)
		{
			return format;
		}
		else if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features)
		{
			return format;
		}
	}
	throw std::runtime_error("Failed to find supported format! [findSupportedFormat()]");
}
vk::Format Context::findDepthFormat() 
{
	return findSupportedFormat(
	{ vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint },
		vk::ImageTiling::eOptimal,
		vk::FormatFeatureFlagBits::eDepthStencilAttachment
	);
}
bool Context::hasStencilComponent(const vk::Format &format) 
{
	return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
}
void Context::createBuffer(const vk::DeviceSize &size, const vk::BufferUsageFlags &usage, const vk::MemoryPropertyFlags &properties, vk::Buffer& buffer, vk::DeviceMemory& bufferMemory) const
{
	//Define buffer
	vk::BufferCreateInfo bufferInfo;
	{
		bufferInfo.flags = {};
		bufferInfo.size = size;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = vk::SharingMode::eExclusive;
	}
	buffer = m_device.createBuffer(bufferInfo);
	//Allocate memory
	const vk::MemoryRequirements memReq = m_device.getBufferMemoryRequirements(buffer);
	vk::MemoryAllocateInfo allocInfo;
	{
		allocInfo.allocationSize = memReq.size;
		allocInfo.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, properties);
	}
	bufferMemory = m_device.allocateMemory(allocInfo);
	m_device.bindBufferMemory(buffer, bufferMemory, 0);//Offset must be divisble by memReq.alignment
}

void Context::copyBuffer(const vk::Buffer &src, const vk::Buffer &dest, const vk::DeviceSize &size, const vk::DeviceSize &srcOffset, const vk::DeviceSize &dstOffset) const
{
	vk::CommandBuffer cb = beginSingleTimeCommands();
	vk::BufferCopy copyRegion;
	{
		copyRegion.srcOffset = srcOffset;
		copyRegion.dstOffset = dstOffset;
		copyRegion.size = size;
	}
	cb.copyBuffer(src, dest, 1, &copyRegion);
	endSingleTimeCommands(cb);
}
void Context::createImage(const uint32_t &width, const uint32_t &height, const vk::Format &format, const vk::ImageTiling &tiling, const vk::ImageUsageFlags &usage, const vk::MemoryPropertyFlags &properties, vk::Image& image, vk::DeviceMemory& imageMemory) const
{
	vk::ImageCreateInfo imgCreate;
	{
		imgCreate.imageType = vk::ImageType::e2D;
		imgCreate.extent.width = width;
		imgCreate.extent.height = height;
		imgCreate.extent.depth = 1;
		imgCreate.mipLevels = 1;
		imgCreate.arrayLayers = 1;
		imgCreate.format = format;
		imgCreate.tiling = tiling;
		imgCreate.initialLayout = vk::ImageLayout::eUndefined;
		imgCreate.usage = usage;
		imgCreate.sharingMode = vk::SharingMode::eExclusive;
		imgCreate.samples = vk::SampleCountFlagBits::e1;
		imgCreate.flags = {}; // Optional
	}
	image = m_device.createImage(imgCreate);
	vk::MemoryRequirements memReqs = m_device.getImageMemoryRequirements(image);
	vk::MemoryAllocateInfo memAllocInfo;
	{
		memAllocInfo.allocationSize = memReqs.size;
		memAllocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, properties);
	}
	imageMemory = m_device.allocateMemory(memAllocInfo);
	m_device.bindImageMemory(image, imageMemory, 0);
}
vk::CommandBuffer Context::beginSingleTimeCommands() const
{
	vk::CommandBufferAllocateInfo cbAllocInfo;
	{
		cbAllocInfo.level = vk::CommandBufferLevel::ePrimary;
		cbAllocInfo.commandPool = m_commandPool;
		cbAllocInfo.commandBufferCount = 1;
	}
	vk::CommandBuffer cb = m_device.allocateCommandBuffers(cbAllocInfo)[0];
	vk::CommandBufferBeginInfo cbBeginInfo;
	{
		cbBeginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
	}
	cb.begin(cbBeginInfo);
	return cb;
}
void Context::endSingleTimeCommands(vk::CommandBuffer &cb) const
{
	cb.end();
	vk::SubmitInfo submitInfo;
	{
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cb;
	}
	m_graphicsQueue.submit(1, &submitInfo, nullptr);
	m_graphicsQueue.waitIdle();
	m_device.freeCommandBuffers(m_commandPool, 1, &cb);
}
void Context::transitionImageLayout(vk::Image &image, const vk::Format &format, const vk::ImageLayout &oldLayout, const vk::ImageLayout &newLayout) const
{
	vk::CommandBuffer cb = beginSingleTimeCommands();
	vk::PipelineStageFlags srcStage, dstStage;
	vk::ImageMemoryBarrier barrier;
	{
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;
		barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		//Configure stuff based on layout transition
		if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal)
		{
			barrier.srcAccessMask = {};
			barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
			srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
			dstStage = vk::PipelineStageFlagBits::eTransfer;
		}
		else if(oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
		{
			barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
			barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
			srcStage = vk::PipelineStageFlagBits::eTransfer;
			dstStage = vk::PipelineStageFlagBits::eFragmentShader;
		}
		else if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
		{
			barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
			if (hasStencilComponent(format)) {
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
	endSingleTimeCommands(cb);
}
void Context::copyBufferToImage(const vk::Buffer &buffer, vk::Image &image, const uint32_t &width, const uint32_t &height) const
{
	vk::CommandBuffer cb = beginSingleTimeCommands();
	vk::BufferImageCopy region;
	{
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;

		region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = vk::Offset3D(0,0,0);
		region.imageExtent = vk::Extent3D(width, height, 1);
	}
	cb.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, 1, &region);
	endSingleTimeCommands(cb);
}
vk::ImageView Context::createImageView(const vk::Image &image, const vk::Format &format, const vk::ImageAspectFlags aspectFlags) const
{
	vk::ImageViewCreateInfo viewInfo;
	{
		viewInfo.image = image;
		viewInfo.viewType = vk::ImageViewType::e2D;
		viewInfo.format = format;
		viewInfo.subresourceRange.aspectMask = aspectFlags;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;
		viewInfo.components = vk::ComponentMapping();//eIdentity
	}
	return m_device.createImageView(viewInfo);
}