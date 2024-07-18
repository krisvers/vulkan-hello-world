#include <vulkan/vulkan.h>
#include <types.hpp>
#include <window.inl>

#include <limits>
#include <vector>
#include <tuple>
#include <iostream>

struct Scope {
	struct IMess {
		IMess() = default;
		virtual ~IMess() = default;

		virtual void cleanup() = 0;
	};

	std::vector<IMess*> messes;

	~Scope() {
		cleanup();
	}

	void cleanup() {
		for (usize i = messes.size(); i > 0; --i) {
			messes[i - 1]->cleanup();
			delete messes[i - 1];
		}

		messes.clear();
	}

	template<typename Destructor, typename ...Ts>
	void addMess(Destructor destructor, Ts... data) {
		Mess<Destructor, Ts...>* mess = new Mess<Destructor, Ts...>(destructor, data...);
		messes.push_back(reinterpret_cast<IMess*>(mess));
	}

	template<typename Destructor, typename ...Ts>
	struct Mess : IMess {
		Mess(Destructor destructor, Ts... d) : destructor(destructor), data(d...) {}

		void cleanup() override {
			destructor(std::get<Ts>(data)...);
		}

		Destructor destructor;
		std::tuple<Ts...> data;
	};
};

struct Globals {
	bool VALIDATION = false;
	bool DEBUG_MESSENGER = false;
	bool FRAMES_IN_FLIGHT = 1;

	VkDebugUtilsMessengerEXT debugMessenger;

	Scope scope;

	static VkBool32 vkDebugMessengerCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT severity,
		VkDebugUtilsMessageTypeFlagsEXT type,
		const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
		void* userData)
	{
		std::cout << callbackData->pMessage << std::endl;
		return true;
	}
};

int main(int argc, char** argv) {
	Globals globals = {};
	globals.VALIDATION = true;
	globals.DEBUG_MESSENGER = true;
	globals.FRAMES_IN_FLIGHT = 3;

	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Hello, World!";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo instanceCreateInfo = {};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pApplicationInfo = &appInfo;

	std::vector<const char*> extensions;
	if (globals.DEBUG_MESSENGER) {
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}
	appendWindowExtensions(extensions);

	instanceCreateInfo.enabledExtensionCount = static_cast<u32>(extensions.size());
	instanceCreateInfo.ppEnabledExtensionNames = extensions.data();

	std::vector<const char*> layers;
	if (globals.VALIDATION) {
		layers.push_back("VK_LAYER_KHRONOS_validation");
	}

	instanceCreateInfo.enabledLayerCount = static_cast<u32>(layers.size());
	instanceCreateInfo.ppEnabledLayerNames = layers.data();

	VkInstance instance;
	VkResult result = vkCreateInstance(&instanceCreateInfo, nullptr, &instance);
	if (result != VK_SUCCESS) {
		std::cout << static_cast<s32>(result) << std::endl;
		return 1;
	}
	globals.scope.addMess(vkDestroyInstance, instance, nullptr);

	if (globals.DEBUG_MESSENGER) {
		VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo = {};
		debugMessengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		debugMessengerCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
		debugMessengerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		debugMessengerCreateInfo.pfnUserCallback = globals.vkDebugMessengerCallback;

		PFN_vkCreateDebugUtilsMessengerEXT func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
		if (func != nullptr) {
			if (func(instance, &debugMessengerCreateInfo, nullptr, &globals.debugMessenger) == VK_SUCCESS) {
				PFN_vkDestroyDebugUtilsMessengerEXT destructor = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
				globals.scope.addMess(destructor, instance, globals.debugMessenger, nullptr);
			}
		}
	}

	window_t* window = createWindow("Hello, World!", 800, 600);
	if (window == nullptr) {
		return 1;
	}
	globals.scope.addMess(destroyWindow, window);

	VkSurfaceKHR surface = createSurface(instance, window);
	if (surface == nullptr) {
		return 1;
	}
	globals.scope.addMess(vkDestroySurfaceKHR, instance, surface, nullptr);

	u32 physicalDeviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);

	std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
	vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data());

	VkPhysicalDevice physicalDevice;
	{
		usize bestIndex = std::numeric_limits<usize>::max();
		usize bestScore = 0;
		for (usize i = 0; i < physicalDevices.size(); ++i) {
			VkPhysicalDevice ph = physicalDevices[i];

			VkPhysicalDeviceFeatures feats;
			vkGetPhysicalDeviceFeatures(ph, &feats);

			VkPhysicalDeviceProperties props;
			vkGetPhysicalDeviceProperties(ph, &props);

			if (props.apiVersion < appInfo.apiVersion) {
				continue;
			}

			usize score = (props.limits.maxImageDimension1D * props.limits.maxImageDimension2D) / 1024 + (props.limits.maxFramebufferWidth * props.limits.maxFramebufferHeight) / 1024;
			switch (props.vendorID) {
				case 0x10DE:
					score += 16777216;
					break;
				case 0x1002:
					score += 4194304;
					break;
			}

			switch (props.deviceType) {
				case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
					score *= 1000;
					break;
				case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
					score *= 10;
					break;
				case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
					score /= 10;
					break;
				case VK_PHYSICAL_DEVICE_TYPE_CPU:
				case VK_PHYSICAL_DEVICE_TYPE_OTHER:
					score /= 1000;
					break;
			}

			if (score > bestScore) {
				bestIndex = i;
				bestScore = score;
			}
		}

		if (bestIndex == std::numeric_limits<usize>::max()) {
			return 1;
		}

		physicalDevice = physicalDevices[bestIndex];
	}

	std::vector<VkDeviceQueueCreateInfo> deviceQueueCreateInfos;
	std::vector<u32> queueFamilyIndices;

	VkDevice device;
	VkQueue graphicsQueue;
	VkQueue computeQueue;
	VkQueue transferQueue;
	VkQueue presentQueue;
	u32 graphicsFamilyIndex;
	u32 computeFamilyIndex;
	u32 transferFamilyIndex;
	u32 presentFamilyIndex;
	{
		u32 count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, nullptr);

		std::vector<VkQueueFamilyProperties> families(count);
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, families.data());

		u32 graphics = 0;
		u32 compute = 0;
		u32 transfer = 0;
		u32 present = 0;

		s32 graphicsScore = -3;
		s32 computeScore = -3;
		s32 transferScore = -3;
		s32 presentScore = -3;

		for (usize i = 0; i < families.size(); ++i) {
			VkQueueFlags flags = families[i].queueFlags;

			s32 gScore = 0;
			s32 cScore = 0;
			s32 tScore = 0;
			s32 pScore = 0;

			if (flags & VK_QUEUE_GRAPHICS_BIT) {
				++gScore;
				--cScore;
				--tScore;
				--pScore;
			}

			if (flags & VK_QUEUE_COMPUTE_BIT) {
				--gScore;
				++cScore;
				--tScore;
				--pScore;
			}
			
			if (flags & VK_QUEUE_TRANSFER_BIT) {
				--gScore;
				--cScore;
				++tScore;
				--pScore;
			}
			
			VkBool32 presentSupported = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupported);
			if (presentSupported) {
				pScore += 3;
			}

			if (gScore > graphicsScore) {
				graphicsScore = gScore;
				graphics = i;
			}

			if (cScore > computeScore) {
				computeScore = cScore;
				compute = i;
			}

			if (tScore > transferScore) {
				transferScore = tScore;
				transfer = i;
			}

			if (pScore > presentScore) {
				presentScore = pScore;
				present = i;
			}
		}

		f32 queuePriority = 1.0f;

		VkDeviceQueueCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		info.queueFamilyIndex = graphics;
		info.queueCount = 1;
		info.pQueuePriorities = &queuePriority;

		deviceQueueCreateInfos.push_back(info);

		info.queueFamilyIndex = compute;
		if (compute != graphics) {
			deviceQueueCreateInfos.push_back(info);
		}

		info.queueFamilyIndex = transfer;
		if (transfer != graphics && transfer != compute) {
			deviceQueueCreateInfos.push_back(info);
		}

		info.queueFamilyIndex = present;
		if (present != graphics && present != compute && present != transfer) {
			deviceQueueCreateInfos.push_back(info);
		}

		std::vector<const char*> deviceLayers;
		std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);

		VkDeviceCreateInfo deviceCreateInfo = {};
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceCreateInfo.queueCreateInfoCount = deviceQueueCreateInfos.size();
		deviceCreateInfo.pQueueCreateInfos = deviceQueueCreateInfos.data();
		deviceCreateInfo.enabledLayerCount = deviceLayers.size();
		deviceCreateInfo.ppEnabledLayerNames = deviceLayers.data();
		deviceCreateInfo.enabledExtensionCount = deviceExtensions.size();
		deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
		deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

		if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS) {
			return 1;
		}
		globals.scope.addMess(vkDestroyDevice, device, nullptr);

		vkGetDeviceQueue(device, graphics, 0, &graphicsQueue);
		if (compute == graphics) {
			computeQueue = graphicsQueue;
		} else {
			vkGetDeviceQueue(device, compute, 0, &computeQueue);
		}

		if (transfer == graphics) {
			transferQueue = graphicsQueue;
		} else if (transfer == compute) {
			transferQueue = computeQueue;
		} else {
			vkGetDeviceQueue(device, transfer, 0, &transferQueue);
		}

		if (present == graphics) {
			presentQueue = graphicsQueue;
		} else if (present == compute) {
			presentQueue = computeQueue;
		} else if (present == transfer) {
			presentQueue = transferQueue;
		} else {
			vkGetDeviceQueue(device, present, 0, &presentQueue);
			queueFamilyIndices.push_back(present);
			queueFamilyIndices.push_back(graphics);
		}

		graphicsFamilyIndex = graphics;
		computeFamilyIndex = compute;
		transferFamilyIndex = transfer;
		presentFamilyIndex = present;
	}

	VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
	swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCreateInfo.surface = surface;
	swapchainCreateInfo.minImageCount = 0;
	swapchainCreateInfo.imageFormat = VK_FORMAT_UNDEFINED;
	swapchainCreateInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	swapchainCreateInfo.imageExtent = {};
	swapchainCreateInfo.imageExtent.width = 800;
	swapchainCreateInfo.imageExtent.height = 600;
	swapchainCreateInfo.imageArrayLayers = 1;
	swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchainCreateInfo.imageSharingMode = (queueFamilyIndices.size() != 0) ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
	swapchainCreateInfo.queueFamilyIndexCount = queueFamilyIndices.size();
	swapchainCreateInfo.pQueueFamilyIndices = queueFamilyIndices.data();
	swapchainCreateInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
	swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainCreateInfo.clipped = true;

	u32 swapchainFormatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &swapchainFormatCount, nullptr);

	std::vector<VkSurfaceFormatKHR> swapchainFormats(swapchainFormatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &swapchainFormatCount, swapchainFormats.data());

	swapchainCreateInfo.imageFormat = swapchainFormats[0].format;
	for (VkSurfaceFormatKHR format : swapchainFormats) {
		if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			swapchainCreateInfo.imageFormat = format.format;
			swapchainCreateInfo.imageColorSpace = format.colorSpace;
			break;
		}
	}

	u32 surfacePresentModeCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &surfacePresentModeCount, nullptr);

	std::vector<VkPresentModeKHR> surfacePresentModes(surfacePresentModeCount);
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &surfacePresentModeCount, surfacePresentModes.data());

	for (VkPresentModeKHR pMode : surfacePresentModes) {
		if (pMode == VK_PRESENT_MODE_MAILBOX_KHR) {
			swapchainCreateInfo.presentMode = pMode;
			break;
		}
	}

	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);

	if (surfaceCapabilities.currentExtent.width != std::numeric_limits<u32>::max()) {
		swapchainCreateInfo.imageExtent = surfaceCapabilities.currentExtent;
	} else {
		swapchainCreateInfo.imageExtent.width = std::max(surfaceCapabilities.minImageExtent.width, std::min(window->width, surfaceCapabilities.maxImageExtent.width));
		swapchainCreateInfo.imageExtent.height = std::max(surfaceCapabilities.minImageExtent.height, std::min(window->height, surfaceCapabilities.maxImageExtent.height));
	}

	swapchainCreateInfo.minImageCount = std::max(surfaceCapabilities.minImageCount, std::min(static_cast<u32>(3), surfaceCapabilities.maxImageCount));

	VkSwapchainKHR swapchain;
	if (vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapchain) != VK_SUCCESS) {
		return 1;
	}
	globals.scope.addMess(vkDestroySwapchainKHR, device, swapchain, nullptr);

	u32 swapchainImageCount = 0;
	vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr);

	std::vector<VkImage> swapchainImages(swapchainImageCount);
	vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data());

	std::vector<VkImageView> swapchainImageViews(swapchainImageCount);
	for (usize i = 0; i < swapchainImages.size(); ++i) {
		VkImageViewCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		info.image = swapchainImages[i];
		info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		info.format =  swapchainCreateInfo.imageFormat;
		info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		info.subresourceRange.baseMipLevel = 0;
		info.subresourceRange.levelCount = 1;
		info.subresourceRange.baseArrayLayer = 0;
		info.subresourceRange.layerCount = 1;

		if (vkCreateImageView(device, &info, nullptr, &swapchainImageViews[i]) != VK_SUCCESS) {
			return 1;
		}
		globals.scope.addMess(vkDestroyImageView, device, swapchainImageViews[i], nullptr);
	}

	const uint32_t vertexShaderCode[] = {
		0x07230203,0x00010000,0x0008000b,0x00000035,0x00000000,0x00020011,0x00000001,0x0006000b,
		0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
		0x0008000f,0x00000000,0x00000004,0x6e69616d,0x00000000,0x00000021,0x00000025,0x00000030,
		0x00030003,0x00000002,0x000001c2,0x00040005,0x00000004,0x6e69616d,0x00000000,0x00050005,
		0x0000000c,0x74726576,0x73656369,0x00000000,0x00040005,0x00000017,0x6f6c6f63,0x00007372,
		0x00060005,0x0000001f,0x505f6c67,0x65567265,0x78657472,0x00000000,0x00060006,0x0000001f,
		0x00000000,0x505f6c67,0x7469736f,0x006e6f69,0x00070006,0x0000001f,0x00000001,0x505f6c67,
		0x746e696f,0x657a6953,0x00000000,0x00070006,0x0000001f,0x00000002,0x435f6c67,0x4470696c,
		0x61747369,0x0065636e,0x00070006,0x0000001f,0x00000003,0x435f6c67,0x446c6c75,0x61747369,
		0x0065636e,0x00030005,0x00000021,0x00000000,0x00060005,0x00000025,0x565f6c67,0x65747265,
		0x646e4978,0x00007865,0x00040005,0x00000030,0x6c6f4376,0x0000726f,0x00050048,0x0000001f,
		0x00000000,0x0000000b,0x00000000,0x00050048,0x0000001f,0x00000001,0x0000000b,0x00000001,
		0x00050048,0x0000001f,0x00000002,0x0000000b,0x00000003,0x00050048,0x0000001f,0x00000003,
		0x0000000b,0x00000004,0x00030047,0x0000001f,0x00000002,0x00040047,0x00000025,0x0000000b,
		0x0000002a,0x00040047,0x00000030,0x0000001e,0x00000000,0x00020013,0x00000002,0x00030021,
		0x00000003,0x00000002,0x00030016,0x00000006,0x00000020,0x00040017,0x00000007,0x00000006,
		0x00000002,0x00040015,0x00000008,0x00000020,0x00000000,0x0004002b,0x00000008,0x00000009,
		0x00000003,0x0004001c,0x0000000a,0x00000007,0x00000009,0x00040020,0x0000000b,0x00000006,
		0x0000000a,0x0004003b,0x0000000b,0x0000000c,0x00000006,0x0004002b,0x00000006,0x0000000d,
		0xbf800000,0x0005002c,0x00000007,0x0000000e,0x0000000d,0x0000000d,0x0004002b,0x00000006,
		0x0000000f,0x3f800000,0x0005002c,0x00000007,0x00000010,0x0000000f,0x0000000d,0x0004002b,
		0x00000006,0x00000011,0x00000000,0x0005002c,0x00000007,0x00000012,0x00000011,0x0000000f,
		0x0006002c,0x0000000a,0x00000013,0x0000000e,0x00000010,0x00000012,0x00040017,0x00000014,
		0x00000006,0x00000003,0x0004001c,0x00000015,0x00000014,0x00000009,0x00040020,0x00000016,
		0x00000006,0x00000015,0x0004003b,0x00000016,0x00000017,0x00000006,0x0006002c,0x00000014,
		0x00000018,0x0000000f,0x00000011,0x0000000f,0x0006002c,0x00000014,0x00000019,0x00000011,
		0x0000000f,0x0000000f,0x0006002c,0x00000014,0x0000001a,0x0000000f,0x0000000f,0x00000011,
		0x0006002c,0x00000015,0x0000001b,0x00000018,0x00000019,0x0000001a,0x00040017,0x0000001c,
		0x00000006,0x00000004,0x0004002b,0x00000008,0x0000001d,0x00000001,0x0004001c,0x0000001e,
		0x00000006,0x0000001d,0x0006001e,0x0000001f,0x0000001c,0x00000006,0x0000001e,0x0000001e,
		0x00040020,0x00000020,0x00000003,0x0000001f,0x0004003b,0x00000020,0x00000021,0x00000003,
		0x00040015,0x00000022,0x00000020,0x00000001,0x0004002b,0x00000022,0x00000023,0x00000000,
		0x00040020,0x00000024,0x00000001,0x00000022,0x0004003b,0x00000024,0x00000025,0x00000001,
		0x00040020,0x00000027,0x00000006,0x00000007,0x00040020,0x0000002d,0x00000003,0x0000001c,
		0x00040020,0x0000002f,0x00000003,0x00000014,0x0004003b,0x0000002f,0x00000030,0x00000003,
		0x00040020,0x00000032,0x00000006,0x00000014,0x00050036,0x00000002,0x00000004,0x00000000,
		0x00000003,0x000200f8,0x00000005,0x0003003e,0x0000000c,0x00000013,0x0003003e,0x00000017,
		0x0000001b,0x0004003d,0x00000022,0x00000026,0x00000025,0x00050041,0x00000027,0x00000028,
		0x0000000c,0x00000026,0x0004003d,0x00000007,0x00000029,0x00000028,0x00050051,0x00000006,
		0x0000002a,0x00000029,0x00000000,0x00050051,0x00000006,0x0000002b,0x00000029,0x00000001,
		0x00070050,0x0000001c,0x0000002c,0x0000002a,0x0000002b,0x00000011,0x0000000f,0x00050041,
		0x0000002d,0x0000002e,0x00000021,0x00000023,0x0003003e,0x0000002e,0x0000002c,0x0004003d,
		0x00000022,0x00000031,0x00000025,0x00050041,0x00000032,0x00000033,0x00000017,0x00000031,
		0x0004003d,0x00000014,0x00000034,0x00000033,0x0003003e,0x00000030,0x00000034,0x000100fd,
		0x00010038
	};

	VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
	shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCreateInfo.codeSize = sizeof(vertexShaderCode);
	shaderModuleCreateInfo.pCode = vertexShaderCode;

	VkShaderModule vertexModule;
	if (vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &vertexModule) != VK_SUCCESS) {
		return 1;
	}

	const uint32_t fragmentShaderCode[] = {
		0x07230203,0x00010000,0x0008000b,0x00000013,0x00000000,0x00020011,0x00000001,0x0006000b,
		0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
		0x0007000f,0x00000004,0x00000004,0x6e69616d,0x00000000,0x00000009,0x0000000c,0x00030010,
		0x00000004,0x00000007,0x00030003,0x00000002,0x000001c2,0x00040005,0x00000004,0x6e69616d,
		0x00000000,0x00040005,0x00000009,0x6c6f436f,0x0000726f,0x00040005,0x0000000c,0x6c6f4376,
		0x0000726f,0x00040047,0x00000009,0x0000001e,0x00000000,0x00040047,0x0000000c,0x0000001e,
		0x00000000,0x00020013,0x00000002,0x00030021,0x00000003,0x00000002,0x00030016,0x00000006,
		0x00000020,0x00040017,0x00000007,0x00000006,0x00000004,0x00040020,0x00000008,0x00000003,
		0x00000007,0x0004003b,0x00000008,0x00000009,0x00000003,0x00040017,0x0000000a,0x00000006,
		0x00000003,0x00040020,0x0000000b,0x00000001,0x0000000a,0x0004003b,0x0000000b,0x0000000c,
		0x00000001,0x0004002b,0x00000006,0x0000000e,0x3f800000,0x00050036,0x00000002,0x00000004,
		0x00000000,0x00000003,0x000200f8,0x00000005,0x0004003d,0x0000000a,0x0000000d,0x0000000c,
		0x00050051,0x00000006,0x0000000f,0x0000000d,0x00000000,0x00050051,0x00000006,0x00000010,
		0x0000000d,0x00000001,0x00050051,0x00000006,0x00000011,0x0000000d,0x00000002,0x00070050,
		0x00000007,0x00000012,0x0000000f,0x00000010,0x00000011,0x0000000e,0x0003003e,0x00000009,
		0x00000012,0x000100fd,0x00010038
	};

	shaderModuleCreateInfo.codeSize = sizeof(fragmentShaderCode);
	shaderModuleCreateInfo.pCode = fragmentShaderCode;
	
	VkShaderModule fragmentModule;
	if (vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &fragmentModule) != VK_SUCCESS) {
		return 1;
	}

	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
	const char* stageName = "main";
	{
		VkPipelineShaderStageCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		info.stage = VK_SHADER_STAGE_VERTEX_BIT;
		info.module = vertexModule;
		info.pName = stageName;
	
		shaderStages.push_back(info);

		info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		info.module = fragmentModule;
	
		shaderStages.push_back(info);
	}

	VkPipelineVertexInputStateCreateInfo vertexInputState = {};
	vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
	inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyState.primitiveRestartEnable = false;

	VkViewport viewport = {};
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = swapchainCreateInfo.imageExtent.width;
	viewport.height = swapchainCreateInfo.imageExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent = swapchainCreateInfo.imageExtent;

	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizationState = {};
	rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationState.depthClampEnable = false;
	rasterizationState.rasterizerDiscardEnable = false;
	rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationState.cullMode = VK_CULL_MODE_NONE;
	rasterizationState.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizationState.depthBiasEnable = false;
	rasterizationState.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo multisampleState = {};
	multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleState.sampleShadingEnable = false;
	multisampleState.minSampleShading = 1.0f;
	multisampleState.pSampleMask = nullptr;
	multisampleState.alphaToCoverageEnable = false;
	multisampleState.alphaToOneEnable = false;

	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = true;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlendState = {};
	colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendState.logicOpEnable = true;
	colorBlendState.logicOp = VK_LOGIC_OP_COPY;
	colorBlendState.attachmentCount = 1;
	colorBlendState.pAttachments = &colorBlendAttachment;
	colorBlendState.blendConstants[0] = 0.0f;
	colorBlendState.blendConstants[1] = 0.0f;
	colorBlendState.blendConstants[2] = 0.0f;
	colorBlendState.blendConstants[3] = 0.0f;

	VkDynamicState dynamicStates[2] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamicState = {};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = 2;
	dynamicState.pDynamicStates = dynamicStates;

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = 0;
	pipelineLayoutCreateInfo.pSetLayouts = nullptr;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

	VkPipelineLayout pipelineLayout;
	if (vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
		return 1;
	}
	globals.scope.addMess(vkDestroyPipelineLayout, device, pipelineLayout, nullptr);

	VkAttachmentDescription attachmentDescription = {};
	attachmentDescription.format = swapchainCreateInfo.imageFormat;
	attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference attachmentReference = {};
	attachmentReference.attachment = 0;
	attachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpassDescription = {};
	subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDescription.colorAttachmentCount = 1;
	subpassDescription.pColorAttachments = &attachmentReference;

	VkSubpassDependency subpassDependency = {};
	subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	subpassDependency.dstSubpass = 0;
	subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependency.srcAccessMask = 0;
	subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo renderPassCreateInfo = {};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = 1;
	renderPassCreateInfo.pAttachments = &attachmentDescription;
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpassDescription;
	renderPassCreateInfo.dependencyCount = 1;
	renderPassCreateInfo.pDependencies = &subpassDependency;

	VkRenderPass renderPass;
	if (vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPass) != VK_SUCCESS) {
		return 1;
	}
	globals.scope.addMess(vkDestroyRenderPass, device, renderPass, nullptr);

	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.stageCount = shaderStages.size();
	pipelineCreateInfo.pStages = shaderStages.data();
	pipelineCreateInfo.pVertexInputState = &vertexInputState;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
	pipelineCreateInfo.pTessellationState = nullptr;
	pipelineCreateInfo.pViewportState = &viewportState;
	pipelineCreateInfo.pRasterizationState = &rasterizationState;
	pipelineCreateInfo.pMultisampleState = &multisampleState;
	pipelineCreateInfo.pDepthStencilState = nullptr;
	pipelineCreateInfo.pColorBlendState = &colorBlendState;
	pipelineCreateInfo.pDynamicState = &dynamicState;
	pipelineCreateInfo.pDynamicState = nullptr;
	pipelineCreateInfo.layout = pipelineLayout;
	pipelineCreateInfo.renderPass = renderPass;

	VkPipeline pipeline;
	result = vkCreateGraphicsPipelines(device, nullptr, 1, &pipelineCreateInfo, nullptr, &pipeline);
	vkDestroyShaderModule(device, fragmentModule, nullptr);
	vkDestroyShaderModule(device, vertexModule, nullptr);

	if (result != VK_SUCCESS) {
		return 1;
	}
	globals.scope.addMess(vkDestroyPipeline, device, pipeline, nullptr);

	std::vector<VkFramebuffer> framebuffers(swapchainImageCount);
	for (usize i = 0; i < swapchainImageViews.size(); ++i) {
		VkFramebufferCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		info.renderPass = renderPass;
		info.attachmentCount = 1;
		info.pAttachments = &swapchainImageViews[i];
		info.width = swapchainCreateInfo.imageExtent.width;
		info.height = swapchainCreateInfo.imageExtent.height;
		info.layers = 1;

		if (vkCreateFramebuffer(device, &info, nullptr, &framebuffers[i])) {
			return 1;
		}
		globals.scope.addMess(vkDestroyFramebuffer, device, framebuffers[i], nullptr);
	}

	VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo = {};
	graphicsCommandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	graphicsCommandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	graphicsCommandPoolCreateInfo.queueFamilyIndex = graphicsFamilyIndex;

	VkCommandPool graphicsCommandPool;
	if (vkCreateCommandPool(device, &graphicsCommandPoolCreateInfo, nullptr, &graphicsCommandPool) != VK_SUCCESS) {
		return 1;
	}
	globals.scope.addMess(vkDestroyCommandPool, device, graphicsCommandPool, nullptr);

	VkCommandBufferAllocateInfo graphicsCommandBufferAllocateInfo = {};
	graphicsCommandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	graphicsCommandBufferAllocateInfo.commandPool = graphicsCommandPool;
	graphicsCommandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	graphicsCommandBufferAllocateInfo.commandBufferCount = globals.FRAMES_IN_FLIGHT;

	std::vector<VkCommandBuffer> graphicsCommandBuffers(globals.FRAMES_IN_FLIGHT);
	for (usize i = 0; i < graphicsCommandBuffers.size(); ++i) {
		if (vkAllocateCommandBuffers(device, &graphicsCommandBufferAllocateInfo, &graphicsCommandBuffers[i]) != VK_SUCCESS) {
			return 1;
		}
	}

	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	std::vector<VkSemaphore> imageAvailableSemaphores(globals.FRAMES_IN_FLIGHT);
	std::vector<VkSemaphore> renderFinishedSemaphores(globals.FRAMES_IN_FLIGHT);
	for (usize i = 0; i < globals.FRAMES_IN_FLIGHT; ++i) {
		if (vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS) {
			return 1;
		}
		globals.scope.addMess(vkDestroySemaphore, device, imageAvailableSemaphores[i], nullptr);

		if (vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS) {
			return 1;
		}
		globals.scope.addMess(vkDestroySemaphore, device, renderFinishedSemaphores[i], nullptr);
	}

	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	std::vector<VkFence> inFlightFences(globals.FRAMES_IN_FLIGHT);
	for (usize i = 0; i < globals.FRAMES_IN_FLIGHT; ++i) {
		if (vkCreateFence(device, &fenceCreateInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
			return 1;
		}
		globals.scope.addMess(vkDestroyFence, device, inFlightFences[i], nullptr);
	}

	usize currentFrameInFlight = 0;
	while (processEvents(window)) {
		vkWaitForFences(device, 1, &inFlightFences[currentFrameInFlight], VK_TRUE, std::numeric_limits<u64>::max());
		vkResetFences(device, 1, &inFlightFences[currentFrameInFlight]);

		u32 swapchainImageIndex = 0;
		vkAcquireNextImageKHR(device, swapchain, std::numeric_limits<u64>::max(), imageAvailableSemaphores[currentFrameInFlight], VK_NULL_HANDLE, &swapchainImageIndex);

		vkResetCommandBuffer(graphicsCommandBuffers[currentFrameInFlight], 0);

		VkCommandBufferBeginInfo commandBufferBeginInfo = {};
		commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		
		if (vkBeginCommandBuffer(graphicsCommandBuffers[currentFrameInFlight], &commandBufferBeginInfo) != VK_SUCCESS) {
			return 1;
		}

		VkClearValue clearColor = { { { 0.2f, 0.4f, 0.1f, 1.0f } } };
		
		VkRenderPassBeginInfo renderPassBeginInfo = {};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.framebuffer = framebuffers[swapchainImageIndex];
		renderPassBeginInfo.renderArea.offset = { 0, 0 };
		renderPassBeginInfo.renderArea.extent = swapchainCreateInfo.imageExtent;
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = &clearColor;

		vkCmdBeginRenderPass(graphicsCommandBuffers[currentFrameInFlight], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdSetViewport(graphicsCommandBuffers[currentFrameInFlight], 0, 1, &viewport);
		vkCmdSetScissor(graphicsCommandBuffers[currentFrameInFlight], 0, 1, &scissor);
		vkCmdBindPipeline(graphicsCommandBuffers[currentFrameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vkCmdDraw(graphicsCommandBuffers[currentFrameInFlight], 3, 1, 0, 0);
		
		vkCmdEndRenderPass(graphicsCommandBuffers[currentFrameInFlight]);
		if (vkEndCommandBuffer(graphicsCommandBuffers[currentFrameInFlight]) != VK_SUCCESS) {
			return 1;
		}

		VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &imageAvailableSemaphores[currentFrameInFlight];
		submitInfo.pWaitDstStageMask = &waitStage;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &graphicsCommandBuffers[currentFrameInFlight];
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &renderFinishedSemaphores[currentFrameInFlight];

		if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrameInFlight]) != VK_SUCCESS) {
			return 1;
		}

		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &renderFinishedSemaphores[currentFrameInFlight];
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &swapchain;
		presentInfo.pImageIndices = &swapchainImageIndex;
		
		vkQueuePresentKHR(presentQueue, &presentInfo);
		currentFrameInFlight = (currentFrameInFlight + 1) % globals.FRAMES_IN_FLIGHT;
	}

	vkDeviceWaitIdle(device);
	return 0;
}