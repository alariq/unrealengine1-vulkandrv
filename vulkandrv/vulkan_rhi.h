#pragma once

#include "rhi.h"

enum {
    kNumBufferedFrames = 3
};

//bool vulkan_initialize(HWND rw_handle);
bool vulkan_finalize();
IRHIDevice* create_device();
void destroy_device(IRHIDevice* device);

struct SwapChainData;
bool query_swapchain_data(VkPhysicalDevice phys_device, VkSurfaceKHR surface, SwapChainData& swap_chain);

struct SwapChain;
struct QueueFamilies;
bool create_swap_chain(const SwapChainData& swap_chain_data, VkDevice device, VkSurfaceKHR surface,
	const QueueFamilies& qfi, VkAllocationCallbacks* pallocator, SwapChain& swap_chain, VkSwapchainKHR old_swapchain);

struct VulkanDevice;
void destroy_swapchain(VulkanDevice& dev);
