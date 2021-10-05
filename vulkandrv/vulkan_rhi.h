#pragma once

#include "rhi.h"

enum {
    kNumBufferedFrames = 3
};

//bool vulkan_initialize(HWND rw_handle);
bool vulkan_finalize();
IRHIDevice* create_device();
void destroy_device(IRHIDevice* device);
