#include "Context.h"
#include <sstream>
#include <fstream>
#include "GraphicsPipeline.h"
#include <SDL/SDL_vulkan.h>
#include "vk.h"
#include <set>
#include <chrono>
#include <glm/gtc/matrix_transform.hpp>

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
		createDescriptorPool();
		//Create Swapchain and dependencies
		createSwapchainStuff();
		//Create semaphores for queue sync
		m_imageAvailableSemaphore = m_device.createSemaphore({});
		m_renderingFinishedSemaphore = m_device.createSemaphore({});
		//Create memory fences for swapchain images
		createFences();
		//Build a Vertex Buf with model data
		createVertexBuffer();
		createIndexBuffer();
		createUniformBuffer();
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
	destroyDescriptorPool();
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
	//Create Framebuffer
	createFramebuffers();
	//Create the command pool and buffers, begin Renderpasses (inside each command buff)
	createCommandPool(m_graphicsQueueId);
}
void Context::destroySwapchainStuff()
{
	destroyCommandPool();
	delete m_gfxPipeline;
	m_gfxPipeline = nullptr;
	destroyFramebuffers();
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
{
	std::vector<vk::PresentModeKHR> pm = m_physicalDevice.getSurfacePresentModesKHR(m_surface);
	vk::PresentModeKHR bestMode = vk::PresentModeKHR::eFifo;
	for (const auto& availablePresentMode : pm) {
		if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
			return availablePresentMode;
		}
		else if (availablePresentMode == vk::PresentModeKHR::eImmediate) {
			bestMode = availablePresentMode;
		}
	}
	return bestMode;
}
void Context::createDescriptorPool()
{
	vk::DescriptorSetLayoutBinding dslb;
	{
		dslb.binding = 0;
		dslb.descriptorType = vk::DescriptorType::eUniformBuffer;
		dslb.descriptorCount = 1;
		dslb.stageFlags = vk::ShaderStageFlagBits::eVertex;
		dslb.pImmutableSamplers = nullptr;
	}
	vk::DescriptorSetLayoutCreateInfo descSetCreateInfo;
	{
		descSetCreateInfo.bindingCount = 1;
		descSetCreateInfo.pBindings = &dslb;
	}
	m_descriptorSetLayout = m_device.createDescriptorSetLayout(descSetCreateInfo);
	vk::DescriptorPoolSize poolSize;
	{
		poolSize.type = vk::DescriptorType::eUniformBuffer;
		poolSize.descriptorCount = 1;
	}
	vk::DescriptorPoolCreateInfo poolCreateInfo;
	{
		poolCreateInfo.poolSizeCount = 1;
		poolCreateInfo.pPoolSizes = &poolSize;
		poolCreateInfo.maxSets = 1;
		poolCreateInfo.flags = {};
	}
	m_descriptorPool = m_device.createDescriptorPool(poolCreateInfo);
	vk::DescriptorSetLayout layouts[] = { m_descriptorSetLayout };
	vk::DescriptorSetAllocateInfo descSetAllocInfo;
	{
		descSetAllocInfo.descriptorPool = m_descriptorPool;
		descSetAllocInfo.descriptorSetCount = 1;
		descSetAllocInfo.pSetLayouts = layouts;
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
	vk::ComponentMapping swizzleIdent;
	{
		swizzleIdent.r = vk::ComponentSwizzle::eIdentity;
		swizzleIdent.g = vk::ComponentSwizzle::eIdentity;
		swizzleIdent.b = vk::ComponentSwizzle::eIdentity;
		swizzleIdent.a = vk::ComponentSwizzle::eIdentity;
	}
	vk::ImageSubresourceRange subresourceTarget;
	{
		subresourceTarget.aspectMask = vk::ImageAspectFlagBits::eColor;
		subresourceTarget.baseMipLevel = 0;
		subresourceTarget.levelCount = 1;
		subresourceTarget.baseArrayLayer = 0;
		subresourceTarget.layerCount = 1;
	}
	vk::ImageViewCreateInfo imgCreate;
	{
		imgCreate.flags = {};
		imgCreate.image = nullptr;
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
void Context::createFramebuffers()
{
	m_scFramebuffers.resize(m_scImageViews.size());
	for (size_t i = 0; i < m_scImageViews.size(); i++) {
		vk::ImageView attachments[] = { m_scImageViews[i] };
		vk::FramebufferCreateInfo fbCreate;
		{
			fbCreate.renderPass = m_gfxPipeline->RenderPass();
			fbCreate.attachmentCount = 1;
			fbCreate.pAttachments = attachments;
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
		vk::ClearValue clearColor;
		{
			rpBegin.renderPass = m_gfxPipeline->RenderPass();
			rpBegin.framebuffer = m_scFramebuffers[i];
			rpBegin.renderArea.offset = vk::Offset2D({ 0, 0 });
			rpBegin.renderArea.extent = m_swapchainDims;
			rpBegin.clearValueCount = 1;
			clearColor.color = vk::ClearColorValue(std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f });
			rpBegin.pClearValues = &clearColor;
		}
		m_commandBuffers[i].beginRenderPass(rpBegin, vk::SubpassContents::eInline);
		m_commandBuffers[i].bindPipeline(vk::PipelineBindPoint::eGraphics, m_gfxPipeline->Pipeline());
		m_commandBuffers[i].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_gfxPipeline->PipelineLayout(), 0, { m_descriptorSet }, {});
		VkDeviceSize offsets[] = { 0 };
		m_commandBuffers[i].bindVertexBuffers(0, 1, &m_vertexBuffer, offsets);
		m_commandBuffers[i].bindIndexBuffer(m_indexBuffer, 0, vk::IndexType::eUint16);
		//m_commandBuffers[i].draw((unsigned int)tempVertices.size(), 1, 0, 0);//Drawing triangles without index
		m_commandBuffers[i].drawIndexed((unsigned int)tempIndices.size(), 1, 0, 0, 0);
		m_commandBuffers[i].endRenderPass();
		m_commandBuffers[i].end();
	}
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
	//Bind to Pipeline
	vk::DescriptorBufferInfo bufferInfo;
	{
		bufferInfo.buffer = m_uniformBuffer;
		bufferInfo.offset = 0;
		bufferInfo.range = buffSize;
	}
	vk::WriteDescriptorSet descWrite;
	{
		descWrite.dstSet = m_descriptorSet;
		descWrite.dstBinding = 0;
		descWrite.dstArrayElement = 0;
		descWrite.descriptorType = vk::DescriptorType::eUniformBuffer;
		descWrite.descriptorCount = 1;
		descWrite.pBufferInfo = &bufferInfo;
		descWrite.pImageInfo = nullptr; // Optional
		descWrite.pTexelBufferView = nullptr; // Optional
	}
	m_device.updateDescriptorSets(1, &descWrite, 0, nullptr);
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
	m_device.destroyDescriptorSetLayout(m_descriptorSetLayout);
	m_descriptorSetLayout = nullptr;
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
	ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
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
	vk::BufferCopy copyRegion;
	{
		copyRegion.srcOffset = srcOffset;
		copyRegion.dstOffset = dstOffset;
		copyRegion.size = size;
	}
	cb.copyBuffer(src, dest, 1, &copyRegion);
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
