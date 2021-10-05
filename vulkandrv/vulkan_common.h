#pragma once

#if USE_GLAD
	// we are using header only version of glad
	//#if USE_VULKAN_IMPL
	//	#define GLAD_VULKAN_IMPLEMENTATION
	//#endif
	#include "glad/vulkan.h"
#elif USE_FLEXT
	#include "flext/flextVk.h"
#else
	#include <vulkan/vulkan.h>
#endif

