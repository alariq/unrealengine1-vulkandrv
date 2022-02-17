#define WIN32_LEAN_AND_MEAN
#include <windows.h>

//#define VK_USE_PLATFORM_WIN32_KHR
//#include "glad/vulkan.h"
//#include "flext/flextVk.h"
#include <vulkan/vulkan.h>

#include "vulkan_rhi.h"
#include "vulkan_device.h"
#include "rhi.h"

#include "utils/logging.h"
#include "utils/macros.h"
#include "utils/vec.h"

#include <cassert>
#include <vector>
#include <algorithm>

struct v {
	// KHR_surface
	PFN_vkGetPhysicalDeviceSurfaceSupportKHR fpGetPhysicalDeviceSurfaceSupportKHR;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR fpGetPhysicalDeviceSurfaceCapabilitiesKHR;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR fpGetPhysicalDeviceSurfaceFormatsKHR;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR fpGetPhysicalDeviceSurfacePresentModesKHR;

	// KHR_win32_surface
	PFN_vkCreateWin32SurfaceKHR fpCreateWin32SurfaceKHR;

	// KHR_swapchain
    PFN_vkCreateSwapchainKHR fpCreateSwapchainKHR;
    PFN_vkDestroySwapchainKHR fpDestroySwapchainKHR;
    PFN_vkGetSwapchainImagesKHR fpGetSwapchainImagesKHR;
    PFN_vkAcquireNextImageKHR fpAcquireNextImageKHR;
    PFN_vkQueuePresentKHR fpQueuePresentKHR;

};

struct v V;

#define GET_INSTANCE_PROC_ADDR(inst, entrypoint)                                                              \
    {                                                                                                         \
        V.fp##entrypoint = (PFN_vk##entrypoint)vkGetInstanceProcAddr((inst), "vk" #entrypoint);             \
        if (V.fp##entrypoint == NULL) {                                                                   \
            log_error("vkGetInstanceProcAddr failed to find vk" #entrypoint, "vkGetInstanceProcAddr Failure"); \
        }                                                                                                     \
    }

static PFN_vkGetDeviceProcAddr g_gdpa = NULL;

#define GET_DEVICE_PROC_ADDR(inst, dev, entrypoint)                                                                    \
    {                                                                                                            \
        if (!g_gdpa) g_gdpa = (PFN_vkGetDeviceProcAddr)vkGetInstanceProcAddr((inst), "vkGetDeviceProcAddr"); \
        V.fp##entrypoint = (PFN_vk##entrypoint)g_gdpa(dev, "vk" #entrypoint);                                \
        if (V.fp##entrypoint == NULL) {                                                                      \
            log_error("vkGetDeviceProcAddr failed to find vk" #entrypoint, "vkGetDeviceProcAddr Failure");        \
        }                                                                                                        \
    }


static VulkanDevice vk_dev;

static const char* const s_validation_layers[] = {
	"VK_LAYER_KHRONOS_validation",
	"VK_LAYER_LUNARG_standard_validation"
};

std::vector<const char*> req_device_ext;

static const bool s_enable_validation_layers = !M_IS_DEFINED(NDEBUG);
//#define USE_NSIGHT
#define NO_VALIDATION 
std::vector<const char*> get_validation_layers() {
	uint32_t count;
	vkEnumerateInstanceLayerProperties(&count, nullptr);
	std::vector<VkLayerProperties> available_layers(count);
	vkEnumerateInstanceLayerProperties(&count, available_layers.data());

	std::vector<const char*> actual_layer_names2enable;
#if !defined(USE_NSIGHT) && !defined(NO_VALIDATION) // otherwise crashes
	for (int i = 0; i < countof(s_validation_layers); ++i) {
		const char* name = s_validation_layers[i];
		bool b_found = false;

		for (int j = 0; j < (int)available_layers.size(); ++j) {
			const VkLayerProperties& props = available_layers[j];
			if (strcmp(name, props.layerName) == 0) {
				b_found = true;
				actual_layer_names2enable.push_back(name);
				break;
			}
		}

		if (!b_found) {
			log_warning("Validation layer %s is not available, will not be enabled\n", name);
		}
	}
#endif
	return actual_layer_names2enable;
}

bool get_required_extensions(HWND /*rw_handle*/, std::vector<const char*>& ext) {

#if 0
	uint32_t count;
	if (!graphics::get_required_extensions(rw_handle, &count, nullptr))
		return false;

	ext.resize(count);
	if (!graphics::get_required_extensions(rw_handle, &count, ext.data()))
		return false;
#endif
	ext.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#if defined(VK_USE_PLATFORM_WIN32_KHR)
	ext.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#else
#error unsupported platform
#endif
	

	if (s_enable_validation_layers) {
		ext.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		ext.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	};
	return true;
}

bool check_device_extensions(VkPhysicalDevice phys_device, const std::vector<const char*> req_ext) {

	uint32_t count = 0;
	vkEnumerateDeviceExtensionProperties(phys_device, nullptr, &count, nullptr);

	std::vector<VkExtensionProperties> extensions(count);
	if (vkEnumerateDeviceExtensionProperties(phys_device, nullptr, &count, extensions.data()) != VK_SUCCESS) {
		return false;
	}
	// first print all extensions	
	log_info("Device extensions:\n");
	for (int i = 0; i < (int)extensions.size(); ++i) {
		const VkExtensionProperties& e = extensions[i];
		log_info("%s : %d\n", e.extensionName, e.specVersion);
	}

	for (int i = 0; i < (int)req_ext.size(); ++i) {
		const char* re = req_ext[i];
		if (extensions.end() ==
			std::find_if(extensions.begin(), extensions.end(),
				[re](VkExtensionProperties prop) { return 0 == strcmp(prop.extensionName, re); }))
			return false;
	}

	return true;
}


static VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT /*messageType*/,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* /*pUserData*/) {

	if (messageSeverity > VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		log_warning("validation layer: %s\n", pCallbackData->pMessage);
		return VK_TRUE;
	}
	else if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		log_warning("validation layer: %s\n", pCallbackData->pMessage);
}
	else {
		log_info("validation layer: %s\n", pCallbackData->pMessage);
	}
	return VK_TRUE;
}

VkDebugUtilsMessengerCreateInfoEXT setup_debug_messenger() {
	VkDebugUtilsMessengerCreateInfoEXT create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	create_info.pfnUserCallback = debugCallback;
	create_info.pUserData = nullptr; // Optional
	return create_info;
}

VkResult create_debug_utils_messenger_EXT(VkInstance instance,
	const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
	const VkAllocationCallbacks* pAllocator,
	VkDebugUtilsMessengerEXT* pDebugMessenger) {
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
		instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void destroy_debug_utils_messenger_EXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
	const VkAllocationCallbacks* pAllocator) {
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
		instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr) {
		func(instance, debugMessenger, pAllocator);
	}
}

bool create_instance(HWND rw_handle, VkAllocationCallbacks* pallocator,
	VkInstance* instance, VkDebugUtilsMessengerEXT* debug_messenger) {

	VkApplicationInfo application_info = {
		VK_STRUCTURE_TYPE_APPLICATION_INFO,	// VkStructureType            sType
		nullptr,					// const void                *pNext
		"UnrealTournament",				// const char *pApplicationName
		VK_MAKE_VERSION(1, 0, 0),	// uint32_t                   applicationVersion
		"ue1-vulkandrv",				// const char                *pEngineName
		VK_MAKE_VERSION(1, 0, 0),	// uint32_t                   engineVersion
		VK_API_VERSION_1_1			// uint32_t                   apiVersion
	};

	const auto layers = get_validation_layers();
	std::vector<const char*> ext;
	if (!get_required_extensions(rw_handle, ext))
		return false;

	VkInstanceCreateInfo instance_create_info = {
		VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, // VkStructureType            sType
		nullptr,								// const void*                pNext
		0,										// VkInstanceCreateFlags      flags
		&application_info,						// const VkApplicationInfo   *pApplicationInfo
		(uint32_t)layers.size(), layers.data(),
		(uint32_t)ext.size(), ext.data()
	};

	// in order to catch errors during instance creation / destruction
	// because debug messenger is not available yet
	auto msg_creation_info = setup_debug_messenger();
	if (s_enable_validation_layers) {
		instance_create_info.pNext = &msg_creation_info;
	}

	if (vkCreateInstance(&instance_create_info, pallocator, instance) != VK_SUCCESS) {
		log_error("Could not create Vulkan instance!\n");
		return false;
	}

	if (s_enable_validation_layers &&
		create_debug_utils_messenger_EXT(*instance, &msg_creation_info, pallocator,
			debug_messenger) != VK_SUCCESS) {
		log_warning("Failed to set up debug messenger!\n");
	}

#if defined(VK_USE_PLATFORM_WIN32_KHR)
    GET_INSTANCE_PROC_ADDR(*instance, CreateWin32SurfaceKHR);
#endif

	return true;
}

bool create_surface(HWND rw_handle, VkInstance instance, VkSurfaceKHR& surface) {

	VkWin32SurfaceCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	createInfo.hwnd = rw_handle;
	createInfo.hinstance = GetModuleHandle(nullptr);
	if (V.fpCreateWin32SurfaceKHR(instance, &createInfo, nullptr, &surface) != VK_SUCCESS) {
		log_error("vkCreateWin32SurfaceKHR: failed to create window surface!");
		return false;
	}
#if 0
	if (!SDL_Vulkan_CreateSurface(rw->window_, instance, &surface)) {
		log_error("SDL_Vulkan_CreateSurfac failed: %s", SDL_GetError());
		return false;
	}
#endif
	return true;
}

QueueFamilies find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface) {
	QueueFamilies indices;

	uint32_t count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
	uint32_t found_queue = 0;
	std::vector<VkQueueFamilyProperties> families(count);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());
	int idx = 0;
	for (int i = 0; i < (int)families.size(); ++i) {
		const VkQueueFamilyProperties& f = families[i];
		bool is_graphics = 0 != (f.queueFlags & VK_QUEUE_GRAPHICS_BIT);
		bool is_compute = 0 != (f.queueFlags & VK_QUEUE_COMPUTE_BIT);
		bool is_transfer = 0 != (f.queueFlags & VK_QUEUE_TRANSFER_BIT);
		log_info("Queue family: G:%d C:%d T:%d count: %d\n",
			is_graphics, is_compute, is_transfer, f.queueCount);

		if (is_graphics && !(found_queue & VK_QUEUE_GRAPHICS_BIT)) {
			indices.graphics_ = idx;
			found_queue |= VK_QUEUE_GRAPHICS_BIT;
			indices.family_bits_ |= VK_QUEUE_GRAPHICS_BIT;
		}
		if (is_compute && !(found_queue & VK_QUEUE_COMPUTE_BIT)) {
			indices.compute_ = idx;
			found_queue |= VK_QUEUE_COMPUTE_BIT;
			indices.family_bits_ |= VK_QUEUE_COMPUTE_BIT;
		}
		if (is_transfer && !(found_queue & VK_QUEUE_TRANSFER_BIT)) {
			indices.transfer_ = idx;
			found_queue |= VK_QUEUE_TRANSFER_BIT;
			indices.family_bits_ |= VK_QUEUE_TRANSFER_BIT;
		}

		VkBool32 present_support = false;
		//vkGetPhysicalDeviceSurfaceSupportKHR(device, idx, surface, &present_support);
		V.fpGetPhysicalDeviceSurfaceSupportKHR(device, idx, surface, &present_support);
		if (present_support) {
			indices.present_ = idx;
		}

		idx++;
	}

	return indices;
}

bool query_swapchain_data(VkPhysicalDevice phys_device, VkSurfaceKHR surface, SwapChainData& swap_chain) {

	if (VK_SUCCESS != V.fpGetPhysicalDeviceSurfaceCapabilitiesKHR(phys_device, surface, &swap_chain.capabilities_))
		return false;

	uint32_t format_count;
	if (VK_SUCCESS != V.fpGetPhysicalDeviceSurfaceFormatsKHR(phys_device, surface, &format_count, nullptr))
		return false;
	if (format_count != 0) {
		swap_chain.formats_.resize(format_count);
		V.fpGetPhysicalDeviceSurfaceFormatsKHR(phys_device, surface, &format_count, swap_chain.formats_.data());
	}

	uint32_t present_mode_count;
	if (VK_SUCCESS != V.fpGetPhysicalDeviceSurfacePresentModesKHR(phys_device, surface, &present_mode_count, nullptr))
		return false;
	if (present_mode_count != 0) {
		swap_chain.present_modes_.resize(present_mode_count);
		V.fpGetPhysicalDeviceSurfacePresentModesKHR(phys_device, surface, &present_mode_count, swap_chain.present_modes_.data());
	}

	return true;
}

bool is_device_suitable(VkPhysicalDevice device, VkPhysicalDeviceProperties props,
	VkPhysicalDeviceFeatures features, VkSurfaceKHR surface) {
	// some arbitrary checks
	auto qf = find_queue_families(device, surface);
	SwapChainData swap_chain;
	if (!query_swapchain_data(device, surface, swap_chain)) {
		return false;
	}
	bool swap_chain_ok = !swap_chain.formats_.empty() && !swap_chain.present_modes_.empty();
	return props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
		features.geometryShader && qf.has_graphics() &&
		check_device_extensions(device, req_device_ext) && swap_chain_ok;
}

bool pick_phys_device(VkInstance instance, VkSurfaceKHR surface, VkPhysicalDevice& phys_device, VkPhysicalDeviceProperties& phys_device_prop) {

    GET_INSTANCE_PROC_ADDR(instance, GetPhysicalDeviceSurfaceSupportKHR);
    GET_INSTANCE_PROC_ADDR(instance, GetPhysicalDeviceSurfaceCapabilitiesKHR);
    GET_INSTANCE_PROC_ADDR(instance, GetPhysicalDeviceSurfaceFormatsKHR);
    GET_INSTANCE_PROC_ADDR(instance, GetPhysicalDeviceSurfacePresentModesKHR);
    GET_INSTANCE_PROC_ADDR(instance, GetSwapchainImagesKHR);


	uint32_t count = 0;
	vkEnumeratePhysicalDevices(instance, &count, nullptr);
	if (0 == count) {
		log_error("vkEnumeratePhysicalDevices: returned 0 available devices\n");
		return false;
	}
	std::vector<VkPhysicalDevice> phys_devices;
	phys_devices.resize(count);
	vkEnumeratePhysicalDevices(instance, &count, phys_devices.data());
	std::string picked_name;
	for (int i = 0; i < (int)phys_devices.size(); ++i) {
		VkPhysicalDevice device = phys_devices[i];
		unsigned char arr[16] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
		VkPhysicalDeviceProperties2 props = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
			nullptr,
		};

		VkPhysicalDeviceFeatures2 features = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
			nullptr,
		};
		vkGetPhysicalDeviceFeatures2(device, &features);
		vkGetPhysicalDeviceProperties2(device, &props);
		if (is_device_suitable(device, props.properties, features.features, surface) && picked_name.empty()) {
			phys_device = device;
			phys_device_prop = props.properties;
			picked_name = props.properties.deviceName;
		}
		log_info("Phys device: %s\n", props.properties.deviceName);
	}

	if (!picked_name.empty()) {
		log_info("Picked phys device: %s\n", picked_name.c_str());
	}

	return !picked_name.empty();
}


bool create_logical_device(VkPhysicalDevice phys_device, QueueFamilies qf, VkAllocationCallbacks* pallocator, VkDevice* device) {

	VkDeviceQueueCreateInfo qci[2] = {}; // graphics + present
	uint32_t qf_indices[2] = { qf.graphics_, qf.present_ };
	const int qci_count = qf.graphics_ == qf.present_ ? 1 : 2;

	float prio = 1.0f;
	for (int i = 0; i < qci_count; ++i) {
		qci[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		qci[i].queueFamilyIndex = qf_indices[i];
		qci[i].queueCount = 1;
		qci[i].pQueuePriorities = &prio;
	}

	// fill as necessary later
	VkPhysicalDeviceFeatures features = {};

	VkDeviceCreateInfo device_create_info = {};
	device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	device_create_info.pQueueCreateInfos = &qci[0];
	device_create_info.queueCreateInfoCount = qci_count;
	device_create_info.pEnabledFeatures = &features;
	device_create_info.enabledExtensionCount = req_device_ext.size();
	device_create_info.ppEnabledExtensionNames = req_device_ext.data();

	if (vkCreateDevice(phys_device, &device_create_info, pallocator, device) != VK_SUCCESS) {
		log_error("vkCreateDevice: Failed to create logical device!");
		return false;
	}
	return true;
}

bool create_semaphore(VkDevice device, VkAllocationCallbacks* pallocator, VkSemaphore* semaphore) {
	VkSemaphoreCreateInfo semaphore_create_info = {
		VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, // VkStructureType          sType
		nullptr,								 // const void*              pNext
		0										 // VkSemaphoreCreateFlags   flags
	};

	if (vkCreateSemaphore(device, &semaphore_create_info, pallocator, semaphore) != VK_SUCCESS) {
		log_error("vkCreateSemaphore: failed\n");
		return false;
	}
	return true;
}

VkFormat translate_f(RHIFormat fmt);
RHISharingMode::Value untranslate_sharing_mode(VkSharingMode sharing_mode);

bool create_swap_chain(const SwapChainData& swap_chain_data, VkDevice device, VkSurfaceKHR surface,
	const QueueFamilies& qfi, VkAllocationCallbacks* pallocator, SwapChain& swap_chain, VkSwapchainKHR old_swapchain) {

	// select format 
	VkSurfaceFormatKHR format = { VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_MAX_ENUM_KHR };
	const RHIFormat swap_chain_surface_fmt = RHIFormat::kB8G8R8A8_SRGB;
	const VkFormat vk_swap_chain_surface_fmt = translate_f(swap_chain_surface_fmt);

	for (int i = 0; i < (int)swap_chain_data.formats_.size(); ++i) {
		const VkSurfaceFormatKHR& fmt = swap_chain_data.formats_[i];
		if (fmt.format == vk_swap_chain_surface_fmt && fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			format = fmt;
			break;
		}
	}

	if (format.format == VK_FORMAT_UNDEFINED)
		return false;

	VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
	for (int i = 0; i < (int)swap_chain_data.present_modes_.size(); ++i) {
		const VkPresentModeKHR& pmode = swap_chain_data.present_modes_[i];
		if (pmode == VK_PRESENT_MODE_MAILBOX_KHR) {
			present_mode = pmode;
			break;
		}
	}

	//int drawable_width;
	//int drawable_height;
	//SDL_Vulkan_GetDrawableSize(rw->window_, &drawable_width, &drawable_height);

	VkExtent2D extent = { 1024, 768 };
	if (swap_chain_data.capabilities_.currentExtent.width != UINT32_MAX) {
		extent = swap_chain_data.capabilities_.currentExtent;
	}
	else {
		extent.width = (uint32_t)clamp(
			extent.width, (float)swap_chain_data.capabilities_.minImageExtent.width,
			(float)swap_chain_data.capabilities_.maxImageExtent.width);
		extent.height = (uint32_t)clamp(
			extent.height, (float)swap_chain_data.capabilities_.minImageExtent.height,
			(float)swap_chain_data.capabilities_.maxImageExtent.height);
	}

	// on my Linux box maxImageCount is 0 for some reason :-/
	uint32_t maxImgCount = max(swap_chain_data.capabilities_.maxImageCount, swap_chain_data.capabilities_.minImageCount);
	// +1 to avoid waiting for a driver 
	uint32_t image_count = min(swap_chain_data.capabilities_.minImageCount + 1, maxImgCount);

	RHIImageUsageFlags rhi_usage = RHIImageUsageFlagBits::ColorAttachmentBit;
	VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if (swap_chain_data.capabilities_.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
		usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		rhi_usage = RHIImageUsageFlagBits::TransferDstBit;
	}

	VkSwapchainCreateInfoKHR create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	create_info.surface = surface;
	create_info.minImageCount = image_count;
	create_info.imageFormat = format.format;
	create_info.imageColorSpace = format.colorSpace;
	create_info.imageExtent = extent;
	create_info.imageArrayLayers = 1;
	create_info.imageUsage = usage;

	uint32_t qf_indices[2] = { qfi.graphics_ , qfi.present_ };
	if (qfi.graphics_ != qfi.present_) {
		// slower than VK_SHARING_MODE_EXCLUSIVE but requires explicit transfering of images from a queue to a queue
		create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		create_info.queueFamilyIndexCount = 2;
		create_info.pQueueFamilyIndices = qf_indices;
	}
	else {
		create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		create_info.queueFamilyIndexCount = 0; // Optional
		create_info.pQueueFamilyIndices = nullptr; // Optional
	}

	create_info.preTransform = swap_chain_data.capabilities_.currentTransform;
	create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	create_info.presentMode = present_mode;
	// do not care about pixels obscured by another window as we are not going to read from a window
	create_info.clipped = VK_TRUE;
	create_info.oldSwapchain = old_swapchain;

	if (V.fpCreateSwapchainKHR(device, &create_info, pallocator, &swap_chain.swap_chain_) != VK_SUCCESS) {
		log_error("vkCreateSwapchainKHR: failed to create swap chain!");
		return false;
	}
	swap_chain.extent_ = extent;
	swap_chain.format_ = format.format;

	uint32_t img_count;
	V.fpGetSwapchainImagesKHR(device, swap_chain.swap_chain_, &img_count, nullptr);
	swap_chain.images_.resize(img_count);
	swap_chain.views_.resize(img_count);
	std::vector<VkImage> vk_images(img_count);
	V.fpGetSwapchainImagesKHR(device, swap_chain.swap_chain_, &img_count, vk_images.data());

	int idx = 0;
	for (int i = 0; i < (int)vk_images.size(); ++i) {
		const VkImage& img = vk_images[i];
		VkImageViewCreateInfo ci = {};
		ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		ci.image = img;
		ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
		ci.format = swap_chain.format_;
		ci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		ci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		ci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		ci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		ci.subresourceRange.baseMipLevel = 0;
		ci.subresourceRange.levelCount = 1;
		ci.subresourceRange.baseArrayLayer = 0;
		ci.subresourceRange.layerCount = 1;

		VkImageView view;
		if (vkCreateImageView(device, &ci, pallocator, &view) != VK_SUCCESS) {
			log_error("vkCreateImageView: failed to create image views!\n");
			return false;
		}
		// swap chain images are created with this initial layout
		VkImageLayout vk_layout = VK_IMAGE_LAYOUT_UNDEFINED;//  VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		// TODO: copy correct flags from swap chain create info
		RHIImageDesc image_desc;
		image_desc.width = extent.width;
		image_desc.height = extent.height;
		image_desc.depth = 1;
		image_desc.arraySize = 1;
		image_desc.format = swap_chain_surface_fmt;
		image_desc.numMips = 1;
		image_desc.numMips = 1;
		image_desc.numSamples = RHISampleCount::k1Bit;
		image_desc.usage = rhi_usage;
		image_desc.sharingMode = untranslate_sharing_mode(create_info.imageSharingMode);
		image_desc.type = RHIImageType::k2D;
		//image_desc.tiling =  ???

		VkMemoryPropertyFlags mem_props = 0; // ???
		RHIImageVk* image = new RHIImageVk(img, image_desc, mem_props, vk_layout);
		swap_chain.images_[idx] = image;
		// swap chain images are created with these flags?
		swap_chain.images_[idx]->vk_access_flags_ = VK_ACCESS_MEMORY_READ_BIT;
		swap_chain.views_[idx] = new RHIImageViewVk(view, image);
		++idx;
	}
	return true;
}

VkAllocationCallbacks* create_allocator() {
	return nullptr;
}
void destroy_allocator() { }

bool vulkan_initialize(HWND rw_handle) {

	assert(!vk_dev.is_initialized_);

	vk_dev.pallocator_ = create_allocator();

	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> extensions(extensionCount);
	if (vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data()) != VK_SUCCESS) {
		return false;
	}
	// place to check for required extensions
	for (int i = 0; i < (int)extensions.size(); ++i) {
		const VkExtensionProperties& ext = extensions[i];
		log_info("%s : %d\n", ext.extensionName, ext.specVersion);
	}


	req_device_ext.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

	if (!create_instance(rw_handle, vk_dev.pallocator_, &vk_dev.instance_, &vk_dev.debug_messenger_)) {
		return false;
	}

	if (!create_surface(rw_handle, vk_dev.instance_, vk_dev.surface_)) {
		return false;
	}

	if (!pick_phys_device(vk_dev.instance_, vk_dev.surface_, vk_dev.phys_device_, vk_dev.vk_phys_device_prop_)) {
		return false;
	}

	// fill device properties
	vk_dev.phys_device_prop_.minUniformBufferOffsetAlignment =
		vk_dev.vk_phys_device_prop_.limits.minUniformBufferOffsetAlignment;

#if USE_GLAD_LOADER
    int glad_vk_version = gladLoaderLoadVulkan(vk_dev.instance_, vk_dev.phys_device_, NULL);
    if (!glad_vk_version) {
        log_error("gladLoad Failure: Unable to re-load Vulkan symbols with instance!\n");
		return false;
    }
#endif

	if (!query_swapchain_data(vk_dev.phys_device_, vk_dev.surface_, vk_dev.swap_chain_data_)) {
		return false;
	}

	vk_dev.queue_families_ = find_queue_families(vk_dev.phys_device_, vk_dev.surface_);

	if (!create_logical_device(vk_dev.phys_device_, vk_dev.queue_families_, vk_dev.pallocator_, &vk_dev.device_)) {
		return false;
	}

    GET_DEVICE_PROC_ADDR(vk_dev.instance_, vk_dev.device_, CreateSwapchainKHR);
    GET_DEVICE_PROC_ADDR(vk_dev.instance_, vk_dev.device_, DestroySwapchainKHR);
    GET_DEVICE_PROC_ADDR(vk_dev.instance_, vk_dev.device_, GetSwapchainImagesKHR);
    GET_DEVICE_PROC_ADDR(vk_dev.instance_, vk_dev.device_, AcquireNextImageKHR);
    GET_DEVICE_PROC_ADDR(vk_dev.instance_, vk_dev.device_, QueuePresentKHR);

	vkGetDeviceQueue(vk_dev.device_, vk_dev.queue_families_.graphics_, 0, &vk_dev.graphics_queue_);
	vkGetDeviceQueue(vk_dev.device_, vk_dev.queue_families_.present_, 0, &vk_dev.present_queue_);

	// TODO: this belongs to renderer!
	VkFenceCreateInfo fence_create_info = {
		VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, // VkStructureType                sType
		nullptr,							 // const void                    *pNext
		VK_FENCE_CREATE_SIGNALED_BIT		 // VkFenceCreateFlags             flags
	};

	for (uint32_t i = 0; i < kNumBufferedFrames; ++i) {
		if (!create_semaphore(vk_dev.device_, vk_dev.pallocator_, &vk_dev.img_avail_sem_[i]) ||
			!create_semaphore(vk_dev.device_, vk_dev.pallocator_,
				&vk_dev.rendering_finished_sem_[i])) {
			return false;
		}

		if (vkCreateFence(vk_dev.device_, &fence_create_info, nullptr,
			&vk_dev.frame_fence_[i]) != VK_SUCCESS) {
			log_error("Could not create a fence!\n");
			return false;
		}
	}
	//

	if (!create_swap_chain(vk_dev.swap_chain_data_, vk_dev.device_, vk_dev.surface_,
		vk_dev.queue_families_, vk_dev.pallocator_, vk_dev.swap_chain_, 0)) {
		return false;
	}

	vk_dev.is_initialized_ = true;

	return true;
}


uint32_t get_window_flags(void) {
	return 0;// SDL_WINDOW_VULKAN;
}

void make_current_context() {}

void destroy_swapchain(VulkanDevice& dev) {

	// please forbid me... will change later
	// make IRHIDevice::Destroy() ao that is object is passed we already know real device type
	// => no conversions + no need for virtual Destroy() in every resource class
	// can be just Destroy(VkDevice dev, VkAllocatorPointers pallocator)
	// who knows... but maybe just make IRHIInastance and have IRHIDevice there
	RHIDeviceVk tmp(dev);
	for (int i = 0; i < (int)dev.swap_chain_.views_.size(); ++i) {
		RHIImageViewVk *view = dev.swap_chain_.views_[i];
		view->Destroy(&tmp);
	}

	V.fpDestroySwapchainKHR(dev.device_, dev.swap_chain_.swap_chain_, dev.pallocator_);

	// images are destroyed by DestroySwapchainKHR
	dev.swap_chain_.images_.clear();

	dev.swap_chain_.views_.clear();
	dev.swap_chain_.swap_chain_ = VK_NULL_HANDLE;
	dev.swap_chain_data_ = { 0 };
}

bool vulkan_finalize() {

	vkDeviceWaitIdle(vk_dev.device_);

	destroy_swapchain(vk_dev);

	// TODO: this belongs to the renderer
	for (uint32_t i = 0; i < kNumBufferedFrames; ++i) {
		vkDestroySemaphore(vk_dev.device_, vk_dev.img_avail_sem_[i], vk_dev.pallocator_);
		vkDestroyFence(vk_dev.device_, vk_dev.frame_fence_[i], vk_dev.pallocator_);
	}

	vkDeviceWaitIdle(vk_dev.device_);
	vkDestroyDevice(vk_dev.device_, vk_dev.pallocator_);

	vkDestroySurfaceKHR(vk_dev.instance_, vk_dev.surface_, vk_dev.pallocator_);

	if (s_enable_validation_layers) {
		destroy_debug_utils_messenger_EXT(vk_dev.instance_, vk_dev.debug_messenger_, vk_dev.pallocator_);
	}
	vkDestroyInstance(vk_dev.instance_, nullptr);
	vk_dev.is_initialized_ = false;
	return true;
}

IRHIDevice* create_device() {
	assert(vk_dev.is_initialized_);
	return new RHIDeviceVk(vk_dev);
}

void destroy_device(IRHIDevice* device) {
	assert(device);
	delete device;
}

#if 0
bool CreateRHI_Vulkan(struct rhi* backend) {
	assert(backend);
	backend->initialize_rhi = rhi_vulkan::initialize;
	backend->finalize_rhi = rhi_vulkan::finalize;
	backend->make_current_context = rhi_vulkan::make_current_context;
	backend->get_window_flags = rhi_vulkan::get_window_flags;
	backend->create_device = rhi_vulkan::create_device;
	backend->destroy_device = rhi_vulkan::destroy_device;

	return true;
}
#endif
