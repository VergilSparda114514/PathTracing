#include "Application.h"
#include "Singleton.h"

static PFN_vkCmdBeginRenderingKHR CmdBeginRenderingKHR = nullptr;
static PFN_vkCmdEndRenderingKHR CmdEndRenderingKHR = nullptr;

Application::~Application()
{
	FreeVulkan();
}

void Application::LoadDynamicRenderingExtensionFunctions()
{
	CmdBeginRenderingKHR = (PFN_vkCmdBeginRenderingKHR)vkGetDeviceProcAddr(m_Device, "vkCmdBeginRenderingKHR");
	CmdEndRenderingKHR = (PFN_vkCmdEndRenderingKHR)vkGetDeviceProcAddr(m_Device, "vkCmdEndRenderingKHR");
}

void Application::Run()
{
	if (Initialize())
	{
		Loop();
		Shutdown();
		FreeResources();
	}
}

bool Application::Initialize()
{
	if (!glfwInit())
	{
		return false;
	}

	if (!glfwVulkanSupported())
	{
		return false;
	}

	InitSettings();

	GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode* videoMode = glfwGetVideoMode(monitor);

	int monitorX, monitorY;
	glfwGetMonitorPos(monitor, &monitorX, &monitorY);

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	GLFWwindow* window = glfwCreateWindow(m_Settings.resolutionX, m_Settings.resolutionY, m_Settings.name.c_str(), m_Settings.fullscreen ? monitor : nullptr, nullptr);
	if (!window)
	{
		return false;
	}

	Singleton<GLFWwindow*>::Get() = window;

	glfwSetWindowPos(window,
		monitorX + (videoMode->width - m_Settings.resolutionX) / 2,
		monitorY + (videoMode->height - m_Settings.resolutionY) / 2);

	glfwSetWindowUserPointer(window, this);

	glfwSetWindowSizeCallback(window, [](GLFWwindow* wnd, int width, int height)
		{
			Application* app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(wnd));
			app->mFramebufferResized = true;
		});

	m_Window = window;

	if (!InitializeVulkan())
	{
		return false;
	}

	if (!InitializeDevicesAndQueues())
	{
		return false;
	}
	if (!InitializeSurface())
	{
		return false;
	}
	if (!InitializeSwapchain())
	{
		return false;
	}
	if (!InitializeFencesAndCommandPool())
	{
		return false;
	}

	LoadDynamicRenderingExtensionFunctions();

	VulkanHelpers::Initialize(m_PhysicalDevice, m_Device, m_CommandPool, m_GraphicsQueue);

	if (!InitializeOffscreenImage())
	{
		return false;
	}
	if (!InitializeCommandBuffers())
	{
		return false;
	}
	if (!InitializeSynchronization())
	{
		return false;
	}

	InitializeImgui();

	InitApp();
	FillCommandBuffers();

	return true;
}

void Application::Loop()
{
	float curTime = 0.0f, prevTime = 0.0f, deltaTime = 0.0f;

	ImGuiIO& io = ImGui::GetIO();

	while (!glfwWindowShouldClose(m_Window))
	{
		glfwPollEvents();

		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		curTime = glfwGetTime();
		deltaTime = curTime - prevTime;
		prevTime = curTime;

		if (m_Settings.supportDocking)
		{
			static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

			ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;

			const ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->WorkPos);
			ImGui::SetNextWindowSize(viewport->WorkSize);
			ImGui::SetNextWindowViewport(viewport->ID);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
			window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

			if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
				window_flags |= ImGuiWindowFlags_NoBackground;

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
			ImGui::Begin("Dockspace Demo", nullptr, window_flags);
			ImGui::PopStyleVar();

			ImGui::PopStyleVar(2);

			if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
			{
				ImGuiID dockspace_id = ImGui::GetID("VulkanAppDockspace");
				ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
			}

			OnUIRender(deltaTime);

			ImGui::End();
		}

		else
		{
			OnUIRender(deltaTime);
		}

		ImGui::Render();
		ProcessFrame(deltaTime, ImGui::GetDrawData());

		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}
	}
}

void Application::Shutdown() const
{
	vkDeviceWaitIdle(m_Device);
	glfwTerminate();
}

bool Application::InitializeVulkan()
{
	VkApplicationInfo appInfo;
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pNext = nullptr;
	appInfo.pApplicationName = m_Settings.name.c_str();
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "VulkanApp";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_2;

	uint32_t requiredExtensionsCount = 0;
	const char** requiredExtensions = glfwGetRequiredInstanceExtensions(&requiredExtensionsCount);

	std::vector<const char*> extensions;
	std::vector<const char*> layers;

	extensions.insert(extensions.begin(), requiredExtensions, requiredExtensions + requiredExtensionsCount);

	if (m_Settings.enableValidation)
	{
		extensions.emplace_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		layers.emplace_back("VK_LAYER_KHRONOS_validation");
	}

	VkInstanceCreateInfo instInfo;
	instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instInfo.pNext = nullptr;
	instInfo.flags = 0;
	instInfo.pApplicationInfo = &appInfo;
	instInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	instInfo.ppEnabledExtensionNames = extensions.data();
	instInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
	instInfo.ppEnabledLayerNames = layers.data();

	VkResult error = vkCreateInstance(&instInfo, nullptr, &m_Instance);
	if (VK_SUCCESS != error)
	{
		CHECK_VK_ERROR(error, "vkCreateInstance");
		return false;
	}

	return true;
}

void Application::InitializeImgui()
{
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
	// io.ConfigViewportsNoAutoMerge = true;
	// io.ConfigViewportsNoTaskBarIcon = true;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsLight();

	float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor()); // Valid on GLFW 3.3+ only

	// Setup scaling
	ImGuiStyle& style = ImGui::GetStyle();
	style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
	style.FontScaleDpi = main_scale;        // Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary. We leave both here for documentation purpose)
	io.ConfigDpiScaleFonts = true;          // [Experimental] Automatically overwrite style.FontScaleDpi in Begin() when Monitor DPI changes. This will scale fonts but _NOT_ scale sizes/padding for now.
	io.ConfigDpiScaleViewports = true;      // [Experimental] Scale Dear ImGui and Platform Windows when Monitor DPI changes.

	// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	VkPipelineRenderingCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.viewMask = 0;
	createInfo.colorAttachmentCount = 1;
	createInfo.pColorAttachmentFormats = &m_SurfaceFormat.format;
	createInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
	createInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

	// Setup Platform/Renderer backends
	ImGui_ImplGlfw_InitForVulkan(m_Window, true);
	ImGui_ImplVulkan_InitInfo init_info{};
	init_info.ApiVersion = VK_API_VERSION_1_4;
	init_info.Instance = m_Instance;
	init_info.PhysicalDevice = m_PhysicalDevice;
	init_info.Device = m_Device;
	init_info.QueueFamily = m_GraphicsQueueFamilyIndex;
	init_info.Queue = m_GraphicsQueue;
	init_info.DescriptorPoolSize = 1000;
	init_info.Subpass = 0;
	init_info.UseDynamicRendering = true;
	init_info.PipelineRenderingCreateInfo = createInfo;
	init_info.MinImageCount = gMaxFramesInFlight;
	init_info.ImageCount = static_cast<uint32_t>(m_SwapchainImages.size());
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	ImGui_ImplVulkan_Init(&init_info);
}

bool Application::InitializeDevicesAndQueues()
{
	uint32_t numPhysDevices = 0;
	VkResult error = vkEnumeratePhysicalDevices(m_Instance, &numPhysDevices, nullptr);
	if (VK_SUCCESS != error || !numPhysDevices) {
		CHECK_VK_ERROR(error, "vkEnumeratePhysicalDevices");
		return false;
	}

	std::vector<VkPhysicalDevice> physDevices(numPhysDevices);
	vkEnumeratePhysicalDevices(m_Instance, &numPhysDevices, physDevices.data());
	m_PhysicalDevice = VK_NULL_HANDLE;

	for (VkPhysicalDevice dev : physDevices) {
		VkPhysicalDeviceProperties props{};
		vkGetPhysicalDeviceProperties(dev, &props);
		if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
			m_PhysicalDevice = dev;
			break;
		}
	}

	if (m_PhysicalDevice == VK_NULL_HANDLE)
		m_PhysicalDevice = physDevices[0];

	// find our queues
	const VkQueueFlagBits askingFlags[3] = { VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_COMPUTE_BIT, VK_QUEUE_TRANSFER_BIT };
	uint32_t queuesIndices[3] = { ~0u, ~0u, ~0u };

	uint32_t queueFamilyPropertyCount;
	vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyPropertyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyPropertyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyPropertyCount, queueFamilyProperties.data());

	for (size_t i = 0; i < 3; i++)
	{
		const VkQueueFlagBits flag = askingFlags[i];
		uint32_t& queueIdx = queuesIndices[i];

		if (flag == VK_QUEUE_COMPUTE_BIT)
		{
			for (uint32_t j = 0; j < queueFamilyPropertyCount; ++j)
			{
				if ((queueFamilyProperties[j].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
					!(queueFamilyProperties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT))
				{
					queueIdx = j;
					break;
				}
			}
		}

		else if (flag == VK_QUEUE_TRANSFER_BIT)
		{
			for (uint32_t j = 0; j < queueFamilyPropertyCount; ++j)
			{
				if ((queueFamilyProperties[j].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
					!(queueFamilyProperties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
					!(queueFamilyProperties[j].queueFlags & VK_QUEUE_COMPUTE_BIT))
				{
					queueIdx = j;
					break;
				}
			}
		}

		if (queueIdx == ~0u)
		{
			for (uint32_t j = 0; j < queueFamilyPropertyCount; ++j)
			{
				if (queueFamilyProperties[j].queueFlags & flag)
				{
					queueIdx = j;
					break;
				}
			}
		}
	}

	m_GraphicsQueueFamilyIndex = queuesIndices[0];
	m_ComputeQueueFamilyIndex = queuesIndices[1];
	m_TransferQueueFamilyIndex = queuesIndices[2];

	// create device
	std::vector<VkDeviceQueueCreateInfo> deviceQueueCreateInfos;
	const float priority = 0.0f;

	VkDeviceQueueCreateInfo deviceQueueCreateInfo;
	deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	deviceQueueCreateInfo.pNext = nullptr;
	deviceQueueCreateInfo.flags = 0;
	deviceQueueCreateInfo.queueFamilyIndex = m_GraphicsQueueFamilyIndex;
	deviceQueueCreateInfo.queueCount = 1;
	deviceQueueCreateInfo.pQueuePriorities = &priority;
	deviceQueueCreateInfos.push_back(deviceQueueCreateInfo);

	if (m_ComputeQueueFamilyIndex != m_GraphicsQueueFamilyIndex)
	{
		deviceQueueCreateInfo.queueFamilyIndex = m_ComputeQueueFamilyIndex;
		deviceQueueCreateInfos.push_back(deviceQueueCreateInfo);
	}

	if (m_TransferQueueFamilyIndex != m_GraphicsQueueFamilyIndex && m_TransferQueueFamilyIndex != m_ComputeQueueFamilyIndex)
	{
		deviceQueueCreateInfo.queueFamilyIndex = m_TransferQueueFamilyIndex;
		deviceQueueCreateInfos.push_back(deviceQueueCreateInfo);
	}

	VkPhysicalDeviceFeatures2 features2{};
	features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

	VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipeline{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };
	VkPhysicalDeviceAccelerationStructureFeaturesKHR rayTracingStructure{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
	VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddress{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES };
	VkPhysicalDeviceDynamicRenderingFeatures dynamicRendering{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES };

	std::vector<const char*> deviceExtensions({ VK_KHR_SWAPCHAIN_EXTENSION_NAME });

	if (m_Settings.supportRaytracing)
	{
		deviceExtensions.emplace_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
		deviceExtensions.emplace_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
		deviceExtensions.emplace_back(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
		deviceExtensions.emplace_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
		deviceExtensions.emplace_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
		deviceExtensions.emplace_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);

		// VK_KHR_ray_tracing requires VK_EXT_descriptor_indexing extension so we make sure it's enabled as well
		m_Settings.supportDescriptorIndexing = true;

		dynamicRendering.dynamicRendering = VK_TRUE;
		bufferDeviceAddress.pNext = &dynamicRendering;
		bufferDeviceAddress.bufferDeviceAddress = VK_TRUE;
		rayTracingPipeline.pNext = &bufferDeviceAddress;
		rayTracingPipeline.rayTracingPipeline = VK_TRUE;
		rayTracingStructure.pNext = &rayTracingPipeline;
		rayTracingStructure.accelerationStructure = VK_TRUE;
		features2.pNext = &rayTracingStructure;
	}

	VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexing{};
	descriptorIndexing.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;

	if (m_Settings.supportDescriptorIndexing)
	{
		deviceExtensions.emplace_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);

		if (features2.pNext)
		{
			descriptorIndexing.pNext = features2.pNext;
		}

		features2.pNext = &descriptorIndexing;
	}

	vkGetPhysicalDeviceFeatures2(m_PhysicalDevice, &features2); // enable all the features our GPU has

	VkDeviceCreateInfo deviceCreateInfo;
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pNext = &features2;
	deviceCreateInfo.flags = 0;
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(deviceQueueCreateInfos.size());
	deviceCreateInfo.pQueueCreateInfos = deviceQueueCreateInfos.data();
	deviceCreateInfo.enabledLayerCount = 0;
	deviceCreateInfo.ppEnabledLayerNames = nullptr;
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
	deviceCreateInfo.pEnabledFeatures = nullptr;

	error = vkCreateDevice(m_PhysicalDevice, &deviceCreateInfo, nullptr, &m_Device);

	if (VK_SUCCESS != error)
	{
		CHECK_VK_ERROR(error, "vkCreateDevice");
		return false;
	}

	// get our queues handles
	vkGetDeviceQueue(m_Device, m_GraphicsQueueFamilyIndex, 0, &m_GraphicsQueue);
	vkGetDeviceQueue(m_Device, m_ComputeQueueFamilyIndex, 0, &m_ComputeQueue);
	vkGetDeviceQueue(m_Device, m_TransferQueueFamilyIndex, 0, &m_TransferQueue);

	// if raytracing support requested - let's get raytracing properties to know shader header size and max recursion
	if (m_Settings.supportRaytracing)
	{
		m_RTProperties = {};
		m_RTProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

		VkPhysicalDeviceProperties2 devProps;
		devProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		devProps.pNext = &m_RTProperties;
		devProps.properties = {};

		vkGetPhysicalDeviceProperties2(m_PhysicalDevice, &devProps);
	}

	return true;
}

bool Application::InitializeSurface()
{
	VkResult error = glfwCreateWindowSurface(m_Instance, m_Window, nullptr, &m_Surface);
	if (VK_SUCCESS != error)
	{
		CHECK_VK_ERROR(error, "glfwCreateWindowSurface");
		return false;
	}

	VkBool32 supportPresent = VK_FALSE;
	error = vkGetPhysicalDeviceSurfaceSupportKHR(m_PhysicalDevice, m_GraphicsQueueFamilyIndex, m_Surface, &supportPresent);
	if (VK_SUCCESS != error || !supportPresent)
	{
		CHECK_VK_ERROR(error, "vkGetPhysicalDeviceSurfaceSupportKHR");
		return false;
	}

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &formatCount, nullptr);
	std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &formatCount, surfaceFormats.data());

	if (formatCount == 1 && surfaceFormats[0].format == VK_FORMAT_UNDEFINED)
	{
		m_SurfaceFormat.format = m_Settings.surfaceFormat;
		m_SurfaceFormat.colorSpace = surfaceFormats[0].colorSpace;
	}

	else
	{
		bool found = false;

		for (const VkSurfaceFormatKHR& surfaceFormat : surfaceFormats)
		{
			if (surfaceFormat.format == m_Settings.surfaceFormat)
			{
				m_SurfaceFormat = surfaceFormat;
				found = true;
				break;
			}
		}

		if (!found)
		{
			m_SurfaceFormat = surfaceFormats[0];
		}
	}

	return true;
}

bool Application::InitializeSwapchain()
{
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	VkResult error = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysicalDevice, m_Surface, &surfaceCapabilities);

	if (VK_SUCCESS != error)
	{
		return false;
	}

	// make sure we stay in our surface's limits
	m_Settings.resolutionX = Clamp(m_Settings.resolutionX, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.currentExtent.width);
	m_Settings.resolutionY = Clamp(m_Settings.resolutionY, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.currentExtent.height);

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, m_Surface, &presentModeCount, nullptr);
	std::vector<VkPresentModeKHR> presentModes(presentModeCount);
	vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, m_Surface, &presentModeCount, presentModes.data());

	// trying to find best present mode for us
	VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
	if (!m_Settings.enableVSync)
	{
		// if we don't want vsync - let's find best one
		for (const VkPresentModeKHR mode : presentModes)
		{
			if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				// this is the best one, so if we found it - just quit
				presentMode = mode;
				break;
			}

			else if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR)
			{
				// we'll use this one if no mailbox supported
				presentMode = mode;
			}
		}
	}

	VkSwapchainKHR prevSwapchain = m_Swapchain;

	VkSwapchainCreateInfoKHR swapchainCreateInfo;
	swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCreateInfo.pNext = nullptr;
	swapchainCreateInfo.flags = 0;
	swapchainCreateInfo.surface = m_Surface;
	swapchainCreateInfo.minImageCount = surfaceCapabilities.minImageCount + 1;
	swapchainCreateInfo.imageFormat = m_SurfaceFormat.format;
	swapchainCreateInfo.imageColorSpace = m_SurfaceFormat.colorSpace;
	swapchainCreateInfo.imageExtent = { m_Settings.resolutionX, m_Settings.resolutionY };
	swapchainCreateInfo.imageArrayLayers = 1;
	swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchainCreateInfo.queueFamilyIndexCount = 0;
	swapchainCreateInfo.pQueueFamilyIndices = nullptr;
	swapchainCreateInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainCreateInfo.presentMode = presentMode;
	swapchainCreateInfo.clipped = VK_TRUE;
	swapchainCreateInfo.oldSwapchain = prevSwapchain;

	error = vkCreateSwapchainKHR(m_Device, &swapchainCreateInfo, nullptr, &m_Swapchain);
	if (VK_SUCCESS != error)
	{
		return false;
	}

	if (prevSwapchain)
	{
		for (VkImageView& imageView : m_SwapchainImageViews)
		{
			vkDestroyImageView(m_Device, imageView, nullptr);
			imageView = VK_NULL_HANDLE;
		}

		vkDestroySwapchainKHR(m_Device, prevSwapchain, nullptr);
	}

	uint32_t imageCount = 0;
	vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, nullptr);
	m_SwapchainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, m_SwapchainImages.data());

	m_SwapchainImageViews.resize(imageCount);
	for (uint32_t i = 0; i < imageCount; i++)
	{
		VkImageViewCreateInfo imageViewCreateInfo;
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.pNext = nullptr;
		imageViewCreateInfo.format = m_SurfaceFormat.format;
		imageViewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.image = m_SwapchainImages[i];
		imageViewCreateInfo.flags = 0;
		imageViewCreateInfo.components = {};

		error = vkCreateImageView(m_Device, &imageViewCreateInfo, nullptr, &m_SwapchainImageViews[i]);

		if (VK_SUCCESS != error)
		{
			return false;
		}
	}

	return true;
}

bool Application::InitializeFencesAndCommandPool()
{
	VkFenceCreateInfo fenceCreateInfo;
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.pNext = nullptr;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	m_WaitForFrameFences.resize(m_SwapchainImages.size());

	for (VkFence& fence : m_WaitForFrameFences)
	{
		vkCreateFence(m_Device, &fenceCreateInfo, nullptr, &fence);
	}

	VkCommandPoolCreateInfo commandPoolCreateInfo;
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.pNext = nullptr;
	commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	commandPoolCreateInfo.queueFamilyIndex = m_GraphicsQueueFamilyIndex;

	const VkResult error = vkCreateCommandPool(m_Device, &commandPoolCreateInfo, nullptr, &m_CommandPool);
	return (VK_SUCCESS == error);
}

bool Application::InitializeOffscreenImage()
{
	const VkExtent3D extent = { m_Settings.resolutionX, m_Settings.resolutionY, 1 };
	VkResult error = m_OffscreenImage.Create(VK_IMAGE_TYPE_2D,
		m_SurfaceFormat.format,
		extent,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if (VK_SUCCESS != error)
	{
		return false;
	}

	VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	error = m_OffscreenImage.CreateImageView(VK_IMAGE_VIEW_TYPE_2D, m_SurfaceFormat.format, range);
	return error == VK_SUCCESS;
}

bool Application::InitializeCommandBuffers()
{
	m_CommandBuffers.resize(m_SwapchainImages.size());

	VkCommandBufferAllocateInfo commandBufferAllocateInfo;
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.pNext = nullptr;
	commandBufferAllocateInfo.commandPool = m_CommandPool;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandBufferCount = static_cast<uint32_t>(m_CommandBuffers.size());

	const VkResult error = vkAllocateCommandBuffers(m_Device, &commandBufferAllocateInfo, m_CommandBuffers.data());
	return (VK_SUCCESS == error);
}

bool Application::InitializeSynchronization()
{
	VkSemaphoreCreateInfo semaphoreCreatInfo;
	semaphoreCreatInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreCreatInfo.pNext = nullptr;
	semaphoreCreatInfo.flags = 0;

	VkResult error = vkCreateSemaphore(m_Device, &semaphoreCreatInfo, nullptr, &m_SemaphoreImageAcquired);
	
	if (VK_SUCCESS != error)
	{
		return false;
	}

	error = vkCreateSemaphore(m_Device, &semaphoreCreatInfo, nullptr, &m_SemaphoreRenderFinished);
	return (VK_SUCCESS == error);
}

void Application::FillCommandBuffers()
{
	VkCommandBufferBeginInfo commandBufferBeginInfo;
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.pNext = nullptr;
	commandBufferBeginInfo.flags = 0;
	commandBufferBeginInfo.pInheritanceInfo = nullptr;

	VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

	for (size_t i = 0; i < m_CommandBuffers.size(); i++)
	{
		const VkCommandBuffer commandBuffer = m_CommandBuffers[i];

		VkResult error = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
		CHECK_VK_ERROR(error, "vkBeginCommandBuffer");

		VulkanHelpers::ImageBarrier(commandBuffer,
			m_OffscreenImage.GetImage(),
			subresourceRange,
			0,
			VK_ACCESS_SHADER_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_GENERAL);

		FillCommandBuffer(commandBuffer, i); // user draw code

		VulkanHelpers::ImageBarrier(commandBuffer,
			m_SwapchainImages[i],
			subresourceRange,
			0,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		VulkanHelpers::ImageBarrier(commandBuffer,
			m_OffscreenImage.GetImage(),
			subresourceRange,
			VK_ACCESS_SHADER_WRITE_BIT,
			VK_ACCESS_TRANSFER_READ_BIT,
			VK_IMAGE_LAYOUT_GENERAL,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

		VkImageCopy copyRegion;
		copyRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		copyRegion.srcOffset = { 0, 0, 0 };
		copyRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		copyRegion.dstOffset = { 0, 0, 0 };
		copyRegion.extent = { m_Settings.resolutionX, m_Settings.resolutionY, 1 };
		vkCmdCopyImage(commandBuffer,
			m_OffscreenImage.GetImage(),
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			m_SwapchainImages[i],
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&copyRegion);

		VulkanHelpers::ImageBarrier(commandBuffer,
			m_SwapchainImages[i], subresourceRange,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			0,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

		error = vkEndCommandBuffer(commandBuffer);
		CHECK_VK_ERROR(error, "vkEndCommandBuffer");
	}
}

void Application::CleanupSwapchain()
{
	m_OffscreenImage.Destroy();

	for (auto imageView : m_SwapchainImageViews)
	{
		vkDestroyImageView(m_Device, imageView, nullptr);
	}

	m_SwapchainImageViews.clear();
	m_SwapchainImages.clear();

	if (m_Swapchain)
	{
		vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
		m_Swapchain = VK_NULL_HANDLE;
	}
}

void Application::RecreateSwapchain()
{
	vkDeviceWaitIdle(m_Device);

	CleanupSwapchain();

	int width = 0, height = 0;

	do
	{
		glfwGetFramebufferSize(m_Window, &width, &height);
		glfwWaitEvents();
	} while (width == 0 || height == 0);

	if (m_Settings.supportDocking)
	{
		m_Settings.resolutionX = m_ViewportWidth;
		m_Settings.resolutionY = m_ViewportHeight;
	}

	else
	{
		m_Settings.resolutionX = width;
		m_Settings.resolutionY = height;
	}

	InitializeSwapchain();
	InitializeOffscreenImage();
	OnResize();
	FillCommandBuffers();
}


//
void Application::ProcessFrame(const float dt, ImDrawData* drawData)
{
	if (mFramebufferResized)
	{
		mFramebufferResized = false;
		RecreateSwapchain();
		return;
	}

	vkWaitForFences(m_Device, 1, &m_WaitForFrameFences[m_CurrentFrame], VK_TRUE, UINT64_MAX);

	uint32_t imageIndex = 0;
	VkResult result = vkAcquireNextImageKHR(m_Device, m_Swapchain, UINT64_MAX, m_SemaphoreImageAcquired, VK_NULL_HANDLE, &imageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		RecreateSwapchain();
		return;
	}

	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		throw std::runtime_error("Failed to accuire image from swapchian!");
		return;
	}

	vkResetFences(m_Device, 1, &m_WaitForFrameFences[imageIndex]);

	OnUpdate(imageIndex, dt);

	const VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	std::array<VkCommandBuffer, 2> commandBuffers{ m_CommandBuffers[imageIndex], RecordCommandBuffer(imageIndex, drawData) };

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &m_SemaphoreImageAcquired;
	submitInfo.pWaitDstStageMask = &waitStageMask;
	submitInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
	submitInfo.pCommandBuffers = commandBuffers.data();
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &m_SemaphoreRenderFinished;

	vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, m_WaitForFrameFences[imageIndex]);

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &m_SemaphoreRenderFinished;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &m_Swapchain;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = nullptr;

	result = vkQueuePresentKHR(m_GraphicsQueue, &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
	{
		RecreateSwapchain();
	}

	else if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to accuire image from swapchian!");
		return;
	}

	m_CurrentFrame = (m_CurrentFrame + 1) % gMaxFramesInFlight;
}

VkCommandBuffer Application::RecordCommandBuffer(size_t imageIndex, ImDrawData* drawData)
{
	VkCommandBuffer commandBuffer = VulkanHelpers::BeginSingleTimeCommandBuffer();

	VkRenderingAttachmentInfo colorAttachment{};
	colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	colorAttachment.imageView = m_SwapchainImageViews[imageIndex];
	colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;   // IMPORTANT for ImGui
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.clearValue = {}; // ignored since LOAD

	VkRenderingInfo renderingInfo{};
	renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderingInfo.renderArea.offset = { 0, 0 };
	renderingInfo.renderArea.extent = { m_Settings.resolutionX, m_Settings.resolutionY };

	renderingInfo.layerCount = 1;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments = &colorAttachment;

	// No depth for ImGui
	renderingInfo.pDepthAttachment = nullptr;
	renderingInfo.pStencilAttachment = nullptr;

	CmdBeginRenderingKHR(commandBuffer, &renderingInfo);

	ImGui_ImplVulkan_RenderDrawData(drawData, commandBuffer);

	CmdEndRenderingKHR(commandBuffer);

	vkEndCommandBuffer(commandBuffer);

	return commandBuffer;
}

void Application::FreeVulkan()
{
	if (m_SemaphoreRenderFinished)
	{
		vkDestroySemaphore(m_Device, m_SemaphoreRenderFinished, nullptr);
		m_SemaphoreRenderFinished = VK_NULL_HANDLE;
	}

	if (m_SemaphoreImageAcquired)
	{
		vkDestroySemaphore(m_Device, m_SemaphoreImageAcquired, nullptr);
		m_SemaphoreImageAcquired = VK_NULL_HANDLE;
	}

	if (!m_CommandBuffers.empty())
	{
		vkFreeCommandBuffers(m_Device, m_CommandPool, static_cast<uint32_t>(m_CommandBuffers.size()), m_CommandBuffers.data());
		m_CommandBuffers.clear();
	}

	if (m_CommandPool)
	{
		vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
		m_CommandPool = VK_NULL_HANDLE;
	}

	for (VkFence& fence : m_WaitForFrameFences)
	{
		vkDestroyFence(m_Device, fence, nullptr);
	}
	m_WaitForFrameFences.clear();

	CleanupSwapchain();

	if (m_Surface) {
		vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
		m_Surface = VK_NULL_HANDLE;
	}

	if (m_Device) {
		vkDestroyDevice(m_Device, nullptr);
		m_Device = VK_NULL_HANDLE;
	}

	if (m_Instance) {
		vkDestroyInstance(m_Instance, nullptr);
		m_Instance = VK_NULL_HANDLE;
	}
}