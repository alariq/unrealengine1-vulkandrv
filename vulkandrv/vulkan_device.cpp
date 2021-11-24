#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "vulkan_common.h"
#include "rhi.h"
#include "vulkan_device.h"
#include "utils/logging.h"
#include "utils/macros.h"
#include <unordered_map>
#include <cassert>
#include <memory>
#include <vector>

#define VK_CHECK(x){\
    VkResult r = (x);\
    if(VK_SUCCESS != r) {\
        log_error("%s, VKResult = %d", "Error calling: " #x, r);\
    }\
}

////////////////////////////////////////////////////////////////////////////////
template <typename T> struct ResImplType;
template<> struct ResImplType<IRHIDevice> { typedef RHIDeviceVk Type; };
template<> struct ResImplType<IRHICmdBuf> { typedef RHICmdBufVk Type; };
template<> struct ResImplType<IRHIImage> { typedef RHIImageVk Type; };
template<> struct ResImplType<IRHIImageView> { typedef RHIImageViewVk Type; };
template<> struct ResImplType<IRHISampler> { typedef RHISamplerVk Type; };
template<> struct ResImplType<IRHIDescriptorSetLayout> { typedef RHIDescriptorSetLayoutVk Type; };
template<> struct ResImplType<IRHIDescriptorSet> { typedef RHIDescriptorSetVk Type; };
template<> struct ResImplType<IRHIRenderPass> { typedef RHIRenderPassVk Type; };
template<> struct ResImplType<IRHIFrameBuffer> { typedef RHIFrameBufferVk Type; };
template<> struct ResImplType<IRHIGraphicsPipeline> { typedef RHIGraphicsPipelineVk Type; };
template<> struct ResImplType<IRHIPipelineLayout> { typedef RHIPipelineLayoutVk Type; };
template<> struct ResImplType<IRHIShader> { typedef RHIShaderVk Type; };
template<> struct ResImplType<IRHIBuffer> { typedef RHIBufferVk Type; };
template<> struct ResImplType<IRHIFence> { typedef RHIFenceVk Type; };
template<> struct ResImplType<IRHIEvent> { typedef RHIEventVk Type; };

template <typename R> 
typename ResImplType<R>::Type* ResourceCast(R* obj) {
	return static_cast<typename ResImplType<R>::Type*>(obj);
}

template <typename R> 
const typename ResImplType<R>::Type* ResourceCast(const R* obj) {
	return static_cast<const typename ResImplType<R>::Type*>(obj);
}
#if defined(USE_Format_TRANSLATION)
////////////////////////////////////////////////////////////////////////////////
VkFormat translate(RHIFormat fmt) {
	static VkFormat formats[] = {
        VK_FORMAT_UNDEFINED,
		VK_FORMAT_R8G8B8_UNORM,		 VK_FORMAT_R8G8B8_UINT,		  VK_FORMAT_R8G8B8_SRGB,
		VK_FORMAT_R8G8B8A8_UNORM,	 VK_FORMAT_R8G8B8A8_UINT,	  VK_FORMAT_R8G8B8A8_SRGB,
		VK_FORMAT_R32_UINT,			 VK_FORMAT_R32_SINT,		  VK_FORMAT_R32_SFLOAT,
		VK_FORMAT_R32G32_UINT,		 VK_FORMAT_R32G32_SINT,		  VK_FORMAT_R32G32_SFLOAT,
		VK_FORMAT_R32G32B32_UINT,	 VK_FORMAT_R32G32B32_SINT,	  VK_FORMAT_R32G32B32_SFLOAT,
		VK_FORMAT_R32G32B32A32_UINT, VK_FORMAT_R32G32B32A32_SINT, VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_FORMAT_B8G8R8A8_UNORM,	 VK_FORMAT_B8G8R8A8_UINT,	  VK_FORMAT_B8G8R8A8_SRGB,
	};
	assert((uint32_t)fmt < countof(formats));
	return formats[(uint32_t)fmt];
}

RHIFormat untranslate(VkFormat fmt) {
    switch(fmt) {
        case VK_FORMAT_R8G8B8_UNORM: return RHIFormat::kR8G8B8_UNORM;
        case VK_FORMAT_R8G8B8_UINT:return RHIFormat::kR8G8B8_UINT;
        case VK_FORMAT_R8G8B8_SRGB:return RHIFormat::kR8G8B8_SRGB;
        case VK_FORMAT_R8G8B8A8_UNORM: return RHIFormat::kR8G8B8A8_UNORM;
        case VK_FORMAT_R8G8B8A8_UINT:return RHIFormat::kR8G8B8A8_UINT;
        case VK_FORMAT_R8G8B8A8_SRGB:return RHIFormat::kR8G8B8A8_SRGB;
		case VK_FORMAT_R32_UINT: return RHIFormat::kR32_UINT;
        case VK_FORMAT_R32_SINT: return RHIFormat::kR32_SINT;		 
        case VK_FORMAT_R32_SFLOAT: return RHIFormat::kR32_SFLOAT;
		case VK_FORMAT_R32G32_UINT:return RHIFormat::kR32G32_UINT;		 
        case VK_FORMAT_R32G32_SINT:return RHIFormat::kR32G32_SINT;		  
        case VK_FORMAT_R32G32_SFLOAT:return RHIFormat::kR32G32_SFLOAT;
		case VK_FORMAT_R32G32B32_UINT:return RHIFormat::kR32G32B32_UINT;	 
        case VK_FORMAT_R32G32B32_SINT:return RHIFormat::kR32G32B32_SINT;	  
        case VK_FORMAT_R32G32B32_SFLOAT:return RHIFormat::kR32G32B32_SFLOAT;
		case VK_FORMAT_R32G32B32A32_UINT:return RHIFormat::kR32G32B32A32_UINT; 
        case VK_FORMAT_R32G32B32A32_SINT:return RHIFormat::kR32G32B32A32_SINT;
        case VK_FORMAT_R32G32B32A32_SFLOAT:return RHIFormat::kR32G32B32A32_SFLOAT;
		case VK_FORMAT_B8G8R8A8_UNORM:return RHIFormat::kB8G8R8A8_UNORM;	 
        case VK_FORMAT_B8G8R8A8_UINT:return RHIFormat::kB8G8R8A8_UINT;	  
		case VK_FORMAT_B8G8R8A8_SRGB:return RHIFormat::kB8G8R8A8_SRGB;
        default:
		    assert(0 && "Incorrect format");
    		return RHIFormat::kUNDEFINED;
    }
}

VkFormat translate_f(RHIFormat fmt) {
	return translate(fmt);
}
RHIFormat untranslate_f(VkFormat fmt) {
	return untranslate(fmt);
}
#else
VkFormat translate_f(RHIFormat fmt) {
	return fmt;
}
RHIFormat untranslate_f(VkFormat fmt) {
	return fmt;
}
#endif

#if defined(USE_ImageViewType_TRANSLATION) 
VkImageViewType translate(RHIImageViewType::Value type) {
	static VkImageViewType types[] = {
		VK_IMAGE_VIEW_TYPE_1D,
		VK_IMAGE_VIEW_TYPE_2D,
		VK_IMAGE_VIEW_TYPE_3D,
		VK_IMAGE_VIEW_TYPE_CUBE,
		VK_IMAGE_VIEW_TYPE_1D_ARRAY,
		VK_IMAGE_VIEW_TYPE_2D_ARRAY,
		VK_IMAGE_VIEW_TYPE_CUBE_ARRAY,
	};
	assert((uint32_t)type < countof(types));
	return types[(uint32_t)type];
};
VkImageViewType translate_ivt(RHIImageViewType::Value type) {
	return translate(type);
}
#else

#endif


VkSampleCountFlagBits translate_num_samples(int num_samples) {
	switch (num_samples) {
	case 1: return VK_SAMPLE_COUNT_1_BIT;
	case 2: return VK_SAMPLE_COUNT_2_BIT;
	case 4: return VK_SAMPLE_COUNT_4_BIT;
	case 8: return VK_SAMPLE_COUNT_8_BIT;
	case 16: return VK_SAMPLE_COUNT_16_BIT;
	case 32: return VK_SAMPLE_COUNT_32_BIT;
	case 64: return VK_SAMPLE_COUNT_64_BIT;
	default:
		assert(0 && "Incorrect sample cunt");
		return VK_SAMPLE_COUNT_1_BIT;
	}
}

VkAttachmentLoadOp translate_alo(RHIAttachmentLoadOp::Value load_op) {
	static VkAttachmentLoadOp ops[] = {
		VK_ATTACHMENT_LOAD_OP_LOAD,
		VK_ATTACHMENT_LOAD_OP_CLEAR,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,
	};
	assert((uint32_t)load_op < countof(ops));
	return ops[(uint32_t)load_op];
}
VkAttachmentLoadOp translate(RHIAttachmentLoadOp::Value load_op) {
	return translate_alo(load_op);
}

VkAttachmentStoreOp translate_aso(RHIAttachmentStoreOp::Value store_op) {
	static VkAttachmentStoreOp ops[] = {
		VK_ATTACHMENT_STORE_OP_STORE,
		VK_ATTACHMENT_STORE_OP_DONT_CARE,
	};
	assert((uint32_t)store_op < countof(ops));
	return ops[(uint32_t)store_op];
}
VkAttachmentStoreOp translate(RHIAttachmentStoreOp::Value store_op) {
	return translate_aso(store_op);
}

#define USE_ImageLayout_TRANSLATION
#if defined(USE_ImageLayout_TRANSLATION)
VkImageLayout translate(RHIImageLayout::Value layout) {
	static VkImageLayout layouts[] = {
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
	};
	assert((uint32_t)layout< countof(layouts));
	return layouts[(uint32_t)layout];
}
VkImageLayout translate_il(RHIImageLayout::Value layout) {
	return translate(layout);
}
#else
VkImageLayout translate_il(RHIImageLayout layout) {
	return layout;
}
#endif

VkImageTiling translate(RHIImageTiling::Value tiling) {
	VkImageTiling tiling_arr[] = {
	VK_IMAGE_TILING_OPTIMAL,
	VK_IMAGE_TILING_LINEAR,
	VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT,
	};
	assert((uint32_t)tiling < countof(tiling_arr));
	return tiling_arr[(uint32_t)tiling];
};

VkImageTiling translate_it(RHIImageTiling::Value tiling) {
	return translate(tiling);
}

VkImageType translate(RHIImageType::Value type) {
	VkImageType types[] = {
    VK_IMAGE_TYPE_1D,
    VK_IMAGE_TYPE_2D,
    VK_IMAGE_TYPE_3D,
	};
	assert((uint32_t)type < countof(types));
	return types[(uint32_t)type];
};

VkImageType translate_itype(RHIImageType::Value type) {
	return translate(type);
}

VkSampleCountFlagBits translate(RHISampleCount::Value samples) {
	VkSampleCountFlagBits samples_arr[] = {
    VK_SAMPLE_COUNT_1_BIT,
    VK_SAMPLE_COUNT_2_BIT,
    VK_SAMPLE_COUNT_4_BIT,
    VK_SAMPLE_COUNT_8_BIT,
    VK_SAMPLE_COUNT_16_BIT,
    VK_SAMPLE_COUNT_32_BIT,
    VK_SAMPLE_COUNT_64_BIT,
	};
	assert((uint32_t)samples< countof(samples_arr));
	return samples_arr[(uint32_t)samples];
};

VkSampleCountFlagBits translate_sc(RHISampleCount::Value type) {
	return translate(type);
}


VkImageUsageFlags  translate_image_usage_flags(uint32_t image_usage_flags) {
	VkImageUsageFlags  f = 0;
	f |= (image_usage_flags & RHIImageUsageFlagBits::TransferSrcBit) ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0;
	f |= (image_usage_flags & RHIImageUsageFlagBits::TransferDstBit) ? VK_IMAGE_USAGE_TRANSFER_DST_BIT : 0;
	f |= (image_usage_flags & RHIImageUsageFlagBits::SampledBit) ? VK_IMAGE_USAGE_SAMPLED_BIT : 0;
	f |= (image_usage_flags & RHIImageUsageFlagBits::StorageBit) ? VK_IMAGE_USAGE_STORAGE_BIT : 0;
	f |= (image_usage_flags & RHIImageUsageFlagBits::ColorAttachmentBit) ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : 0;
	f |= (image_usage_flags & RHIImageUsageFlagBits::DepthStencilAttachmentBit) ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : 0;
	f |= (image_usage_flags & RHIImageUsageFlagBits::TransientAttachmentBit) ? VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT : 0;
	f |= (image_usage_flags & RHIImageUsageFlagBits::InputAttachmentBit) ? VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT : 0;
	return f;
}

#if defined(USE_PipelineStageFlags_TRANSLATION)
VkPipelineStageFlags translate(RHIPipelineStageFlags::Value pipeline_stage) {
	switch (pipeline_stage) {
	case RHIPipelineStageFlags::kTopOfPipe: return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	case RHIPipelineStageFlags::kDrawIndirect: return VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
		case RHIPipelineStageFlags::kVertexInput: return VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
		case RHIPipelineStageFlags::kVertexShader: return VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
		case RHIPipelineStageFlags::kTessellationControlShader: return VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT;
		case RHIPipelineStageFlags::kTessellationEvaluationShader: return VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
		case RHIPipelineStageFlags::kGeometryShader: return VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
		case RHIPipelineStageFlags::kFragmentShader: return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		case RHIPipelineStageFlags::kEarlyFragmentTests: return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		case RHIPipelineStageFlags::kLateFragmentTests: return VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		case RHIPipelineStageFlags::kColorAttachmentOutput: return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		case RHIPipelineStageFlags::kComputeShader: return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		case RHIPipelineStageFlags::kTransfer: return VK_PIPELINE_STAGE_TRANSFER_BIT;
		case RHIPipelineStageFlags::kBottomOfPipe: return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		case RHIPipelineStageFlags::kHost: return VK_PIPELINE_STAGE_HOST_BIT;
		case RHIPipelineStageFlags::kAllGraphics: return VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
		case RHIPipelineStageFlags::kAllCommands: return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		default:
			assert(0 && "Invalid opipeline stage!");
			return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	};
}
VkPipelineStageFlags translate_ps(RHIPipelineStageFlags::Value pipeline_stage) {
	return translate(pipeline_stage);
}
#else
VkPipelineStageFlags translate_ps(RHIPipelineStageFlags pipeline_stage) {
	return pipeline_stage;
}
#endif

#if defined(USE_ACCESS_FLAGS_TRANSLATION)
VkAccessFlags translate(RHIAccessFlags::Value access_flags) {
	switch (access_flags) {
	case RHIAccessFlags::kIndirectCommandRead: return VK_ACCESS_INDIRECT_COMMAND_READ_BIT ;
		case RHIAccessFlags::kIndexRead: return VK_ACCESS_INDEX_READ_BIT ;
		case RHIAccessFlags::kVertexAttributeRead: return VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT ;
		case RHIAccessFlags::kUniformRead : return VK_ACCESS_UNIFORM_READ_BIT ;
		case RHIAccessFlags::kInputAttachmentRead : return VK_ACCESS_INPUT_ATTACHMENT_READ_BIT ;
		case RHIAccessFlags::kShaderRead : return VK_ACCESS_SHADER_READ_BIT ;
		case RHIAccessFlags::kShaderWrite : return VK_ACCESS_SHADER_WRITE_BIT ;
		case RHIAccessFlags::kColorAttachmentRead : return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT ;
		case RHIAccessFlags::kColorAttachmentWrite : return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT ;
		case RHIAccessFlags::kDepthStencilAttachmentRead : return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT ;
		case RHIAccessFlags::kDepthStencilAttachmentWrite : return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT ;
		case RHIAccessFlags::kTransferRead : return VK_ACCESS_TRANSFER_READ_BIT ;
		case RHIAccessFlags::kTransferWrite : return VK_ACCESS_TRANSFER_WRITE_BIT ;
		case RHIAccessFlags::kHostRead : return VK_ACCESS_HOST_READ_BIT ;
		case RHIAccessFlags::kHostWrite : return VK_ACCESS_HOST_WRITE_BIT ;
		case RHIAccessFlags::kMemoryRead : return VK_ACCESS_MEMORY_READ_BIT ;
		case RHIAccessFlags::kMemoryWrite : return VK_ACCESS_MEMORY_WRITE_BIT ;
		default:
			assert(0 && "Invalid access flags!");
			return VK_ACCESS_FLAG_BITS_MAX_ENUM;
	};
}
VkAccessFlags translate_af(RHIAccessFlags::Value access_flags) {
	return translate(access_flags);
}
#else
VkAccessFlags translate_af(RHIAccessFlags access_flags) {
	return access_flags;
}
#endif

#if defined(USE_DependencyFlags_TRANSLATION)
VkDependencyFlags translate_dependency_flags(uint32_t dep_flags) {
	uint32_t vk_dep_flags = (dep_flags & (uint32_t)RHIDependencyFlags::kByRegion) ? VK_DEPENDENCY_BY_REGION_BIT : 0;
	vk_dep_flags |= (dep_flags & (uint32_t)RHIDependencyFlags::kDeviceGroup)? VK_DEPENDENCY_DEVICE_GROUP_BIT: 0;
	vk_dep_flags |= (dep_flags & (uint32_t)RHIDependencyFlags::kViewLocal) ? VK_DEPENDENCY_VIEW_LOCAL_BIT: 0;
    return vk_dep_flags;
}
#else
#endif

VkShaderStageFlagBits translate(RHIShaderStageFlagBits::Value stage_flag) {
	switch (stage_flag) {
	case RHIShaderStageFlagBits::kVertex: return VK_SHADER_STAGE_VERTEX_BIT ;
	case RHIShaderStageFlagBits::kTessellationControl: return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
	case RHIShaderStageFlagBits::kTessellationEvaluation: return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT ;
	case RHIShaderStageFlagBits::kGeometry: return VK_SHADER_STAGE_GEOMETRY_BIT ;
	case RHIShaderStageFlagBits::kFragment: return VK_SHADER_STAGE_FRAGMENT_BIT ;
	case RHIShaderStageFlagBits::kCompute: return VK_SHADER_STAGE_COMPUTE_BIT ;
	default:
		assert(0 && "Invalid shader stage flag!");
		return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
	}
}

VkShaderStageFlagBits translate_ssflag(RHIShaderStageFlagBits::Value stage_flag) {
	return translate(stage_flag);
}

VkShaderStageFlags translate_ssflags(RHIShaderStageFlags stage_flags) {
	VkShaderStageFlags flags = 0;
	flags |= (stage_flags & (uint32_t)RHIShaderStageFlagBits::kVertex) ? VK_SHADER_STAGE_VERTEX_BIT : 0;
	flags |= (stage_flags & (uint32_t)RHIShaderStageFlagBits::kTessellationControl) ? VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT : 0;
	flags |= (stage_flags & (uint32_t)RHIShaderStageFlagBits::kTessellationEvaluation) ? VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT : 0;
	flags |= (stage_flags & (uint32_t)RHIShaderStageFlagBits::kGeometry) ? VK_SHADER_STAGE_GEOMETRY_BIT : 0;
	flags |= (stage_flags & (uint32_t)RHIShaderStageFlagBits::kFragment) ? VK_SHADER_STAGE_FRAGMENT_BIT : 0;
	flags |= (stage_flags & (uint32_t)RHIShaderStageFlagBits::kCompute) ? VK_SHADER_STAGE_COMPUTE_BIT : 0;
	return flags;
}


#if defined(USE_PipelineBindPoint_TRANSLATION) 
VkPipelineBindPoint translate_pbp(RHIPipelineBindPoint::Value pipeline_bind_point) {
	static VkPipelineBindPoint bind_points[] = {
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		VK_PIPELINE_BIND_POINT_COMPUTE,
		VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR
	};
	assert((uint32_t)pipeline_bind_point < countof(bind_points));
	return bind_points[(uint32_t)pipeline_bind_point];
}
VkPipelineBindPoint translate(RHIPipelineBindPoint::Value pipeline_bind_point) {
	return translate_pbp(pipeline_bind_point);
}
#else
VkPipelineBindPoint translate_pbp(RHIPipelineBindPoint pipeline_bind_point) {
	return pipeline_bind_point;
}
#endif

#if defined(USE_ImageAspectFlags_TRANSLATION)
VkImageAspectFlags translate_image_aspect(uint32_t bits) {
	VkImageAspectFlags rv = (bits & (uint32_t)RHIImageAspectFlags::kColor) ? VK_IMAGE_ASPECT_COLOR_BIT: 0;
	rv |= (bits & (uint32_t)RHIImageAspectFlags::kDepth) ? VK_IMAGE_ASPECT_DEPTH_BIT : 0;
	rv |= (bits & (uint32_t)RHIImageAspectFlags::kStencil) ? VK_IMAGE_ASPECT_STENCIL_BIT: 0;
	return rv;
}
#else
VkImageAspectFlags translate_image_aspect(uint32_t bits) {
	return bits;
}
#endif

#if defined(USE_VertexInputRate_TRANSLATION)
VkVertexInputRate translate(RHIVertexInputRate input_rate) {
	return input_rate == RHIVertexInputRate::kVertex ? VK_VERTEX_INPUT_RATE_VERTEX
													 : VK_VERTEX_INPUT_RATE_INSTANCE;
}
VkVertexInputRate translate_vir(RHIVertexInputRate input_rate) {
	return translate(input_rate);
}
#else
VkVertexInputRate translate_vir(RHIVertexInputRate input_rate) {
	return input_rate;
}
#endif

VkPrimitiveTopology translate(RHIPrimitiveTopology::Value prim_topology) {
	switch (prim_topology) {
	case RHIPrimitiveTopology::kPointList: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
	case RHIPrimitiveTopology::kLineList: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	case RHIPrimitiveTopology::kLineStrip: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
	case RHIPrimitiveTopology::kTriangleList: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	case RHIPrimitiveTopology::kTriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	case RHIPrimitiveTopology::kTriangleFan: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
	case RHIPrimitiveTopology::kLineListWithAdjacency: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY;
	case RHIPrimitiveTopology::kLineStripWithAdjacency: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY;
	case RHIPrimitiveTopology::kTriangleListWithAdjacency: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY;
	case RHIPrimitiveTopology::kTriangleStripWithAdjacency: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY;
	case RHIPrimitiveTopology::kPatchList: return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
	default:
		assert(0 && "Invalid primitive topology!");
		return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
	}
} 
VkPrimitiveTopology translate_pt(RHIPrimitiveTopology::Value prim_topology) {
	return translate(prim_topology);
}

VkPolygonMode translate_pm(RHIPolygonMode::Value polygon_mode) {
	switch (polygon_mode) {
	case RHIPolygonMode::kFill : return VK_POLYGON_MODE_FILL;
	case RHIPolygonMode::kLine: return VK_POLYGON_MODE_LINE;
	case RHIPolygonMode::kPoint: return VK_POLYGON_MODE_POINT;
	default:
		assert(0 && "Invalid polygon mode!");
		return VK_POLYGON_MODE_MAX_ENUM;
	}
};
VkPolygonMode translate(RHIPolygonMode::Value polygon_mode) {
	return translate_pm(polygon_mode);
}
  
VkCullModeFlags translate_cm(RHICullModeFlags::Value cull_mode) {
	switch (cull_mode) {
	case RHICullModeFlags::kNone: return VK_CULL_MODE_NONE;
	case RHICullModeFlags::kFront: return VK_CULL_MODE_FRONT_BIT;
	case RHICullModeFlags::kBack: return VK_CULL_MODE_BACK_BIT;
	case RHICullModeFlags::kFrontAndBack: return VK_CULL_MODE_FRONT_AND_BACK;
	default:
		assert(0 && "Invalid cull mode!");
		return VK_CULL_MODE_FLAG_BITS_MAX_ENUM;
	}
};
VkCullModeFlags translate(RHICullModeFlags::Value cull_mode) {
	return translate_cm(cull_mode);
}

VkFrontFace translate_ff(RHIFrontFace::Value front_face) {
	return front_face == RHIFrontFace::kClockwise ? VK_FRONT_FACE_CLOCKWISE
												  : VK_FRONT_FACE_COUNTER_CLOCKWISE;
};
VkFrontFace translate(RHIFrontFace::Value front_face) {
	return translate_ff(front_face);
}

VkBlendFactor translate_bf(RHIBlendFactor::Value blend_factor) {
	switch (blend_factor) {
	case RHIBlendFactor::Zero: return VK_BLEND_FACTOR_ZERO;
	case RHIBlendFactor::One: return VK_BLEND_FACTOR_ONE;
	case RHIBlendFactor::SrcColor: return VK_BLEND_FACTOR_SRC_COLOR;
	case RHIBlendFactor::OneMinusSrcColor: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
	case RHIBlendFactor::DstColor: return VK_BLEND_FACTOR_DST_COLOR;
	case RHIBlendFactor::OneMinusDstColor: return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
	case RHIBlendFactor::SrcAlpha: return VK_BLEND_FACTOR_SRC_ALPHA;
	case RHIBlendFactor::OneMinusSrcAlpha: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	case RHIBlendFactor::DstAlpha: return VK_BLEND_FACTOR_DST_ALPHA;
	case RHIBlendFactor::OneMinusDstAlpha: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
	default:
		assert(0 && "Invalid blend factor!");
		return VK_BLEND_FACTOR_MAX_ENUM;
	}
}
VkBlendFactor translate(RHIBlendFactor::Value blend_factor) {
	return translate_bf(blend_factor);
}

VkBlendOp translate_bo(RHIBlendOp::Value blend_op) {
	switch (blend_op) {
	case RHIBlendOp::kAdd: return VK_BLEND_OP_ADD;
	case RHIBlendOp::kSubtract: return VK_BLEND_OP_SUBTRACT;
	case RHIBlendOp::kReverseSubtract: return VK_BLEND_OP_REVERSE_SUBTRACT;
	case RHIBlendOp::kMin: return VK_BLEND_OP_MIN;
	case RHIBlendOp::kMax: return VK_BLEND_OP_MAX;
	default:
		assert(0 && "Invalid blend op!");
		return VK_BLEND_OP_MAX_ENUM;
	}
}
VkBlendOp translate(RHIBlendOp::Value blend_op) {
	return translate_bo(blend_op);
}

VkColorComponentFlags translate_color_comp(uint32_t col_cmp_flags) {
    VkColorComponentFlags flags;
    flags  = (col_cmp_flags & (uint32_t)RHIColorComponentFlags::kR) ? VK_COLOR_COMPONENT_R_BIT : 0;
    flags |= (col_cmp_flags & (uint32_t)RHIColorComponentFlags::kG) ? VK_COLOR_COMPONENT_G_BIT : 0;
    flags |= (col_cmp_flags & (uint32_t)RHIColorComponentFlags::kB) ? VK_COLOR_COMPONENT_B_BIT : 0;
    flags |= (col_cmp_flags & (uint32_t)RHIColorComponentFlags::kA) ? VK_COLOR_COMPONENT_A_BIT : 0;
    return flags;
}

VkLogicOp translate_lo(RHILogicOp::Value logic_op) {
	switch (logic_op) {
	case RHILogicOp::kClear: return VK_LOGIC_OP_CLEAR;
	case RHILogicOp::kAnd:return VK_LOGIC_OP_AND;
	case RHILogicOp::kAndReverse:return VK_LOGIC_OP_AND_REVERSE;
	case RHILogicOp::kCopy:return VK_LOGIC_OP_COPY;
	case RHILogicOp::kAndInverted:return VK_LOGIC_OP_AND_INVERTED;
	case RHILogicOp::kNoOp:return VK_LOGIC_OP_NO_OP;
	case RHILogicOp::kXor:return VK_LOGIC_OP_XOR;
	case RHILogicOp::kOr:return VK_LOGIC_OP_OR;
	case RHILogicOp::kNor:return VK_LOGIC_OP_NOR;
	case RHILogicOp::kEquivalent:return VK_LOGIC_OP_EQUIVALENT;
	case RHILogicOp::kInvert:return VK_LOGIC_OP_INVERT;
	case RHILogicOp::kOrReverse:return VK_LOGIC_OP_OR_REVERSE;
	case RHILogicOp::kCopyInverted:return VK_LOGIC_OP_COPY_INVERTED;
	case RHILogicOp::kOrInverted:return VK_LOGIC_OP_OR_INVERTED;
	case RHILogicOp::kNand:return VK_LOGIC_OP_NAND;
	case RHILogicOp::kSet:return VK_LOGIC_OP_SET;
	default:
		assert(0 && "Invalid logic op!");
		return VK_LOGIC_OP_MAX_ENUM;
	}
}
VkLogicOp translate(RHILogicOp::Value logic_op) {
	return translate_lo(logic_op);
}

VkBufferUsageFlags translate_buffer_usage(uint32_t usage) {

    VkBufferUsageFlags vk_usage = (usage & RHIBufferUsageFlags::kTransferSrcBit) ? VK_BUFFER_USAGE_TRANSFER_SRC_BIT : 0;
    vk_usage |= (usage & RHIBufferUsageFlags::kTransferDstBit) ? VK_BUFFER_USAGE_TRANSFER_DST_BIT : 0;
    vk_usage |= (usage & RHIBufferUsageFlags::kUniformTexelBufferBit) ? VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT: 0;
    vk_usage |= (usage & RHIBufferUsageFlags::kStorageTexelBufferBit) ? VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT: 0;
    vk_usage |= (usage & RHIBufferUsageFlags::kStorageBufferBit) ? VK_BUFFER_USAGE_STORAGE_BUFFER_BIT: 0;
    vk_usage |= (usage & RHIBufferUsageFlags::kUniformBufferBit) ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT: 0;
    vk_usage |= (usage & RHIBufferUsageFlags::kIndexBufferBit) ? VK_BUFFER_USAGE_INDEX_BUFFER_BIT: 0;
    vk_usage |= (usage & RHIBufferUsageFlags::kVertexBufferBit) ? VK_BUFFER_USAGE_VERTEX_BUFFER_BIT: 0;
    return vk_usage;
};

VkSharingMode translate_sharing_mode(RHISharingMode::Value sharing_mode) {
    return sharing_mode == RHISharingMode::kConcurrent ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
}

RHISharingMode::Value untranslate_sharing_mode(VkSharingMode sharing_mode) {
	return sharing_mode == VK_SHARING_MODE_CONCURRENT ? RHISharingMode::kConcurrent : RHISharingMode::kExclusive;
}

VkMemoryPropertyFlags translate_mem_prop(RHIMemoryPropertyFlags memprop) {
    VkMemoryPropertyFlags vk_flags = (memprop & RHIMemoryPropertyFlagBits::kDeviceLocal) ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT : 0;
    vk_flags |= (memprop & RHIMemoryPropertyFlagBits::kHostVisible) ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT: 0;
    vk_flags |= (memprop & RHIMemoryPropertyFlagBits::kHostCoherent) ? VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : 0;
    vk_flags |= (memprop & RHIMemoryPropertyFlagBits::kHostCached) ? VK_MEMORY_PROPERTY_HOST_CACHED_BIT : 0;
    vk_flags |= (memprop & RHIMemoryPropertyFlagBits::kLazilyAllocated) ? VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT: 0;
    vk_flags |= (memprop & RHIMemoryPropertyFlagBits::kProtectedBit) ? VK_MEMORY_PROPERTY_PROTECTED_BIT : 0;
    vk_flags |= (memprop & RHIMemoryPropertyFlagBits::kDeviceCoherentAMD) ? VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD : 0;
    vk_flags |= (memprop & RHIMemoryPropertyFlagBits::kDeviceUncachedAMD) ? VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD : 0;
    return vk_flags;
}

VkFilter translate_filter(RHIFilter::Value filter) {
	switch (filter) {
	case RHIFilter::kNearest: return VK_FILTER_NEAREST;
	case RHIFilter::kLinear: return VK_FILTER_LINEAR;
	default:
		assert(0 && "Invalid filter!");
		return VK_FILTER_MAX_ENUM;
	}
}
VkSamplerMipmapMode translate_sampler_mipmap_mode(RHISamplerMipmapMode::Value mipmap_mode) {
	switch (mipmap_mode) {
	case RHISamplerMipmapMode::kNearest: return VK_SAMPLER_MIPMAP_MODE_NEAREST;
	case RHISamplerMipmapMode::kLinear: return VK_SAMPLER_MIPMAP_MODE_LINEAR;
	default:
		assert(0 && "Invalid mipmap mode!");
		return VK_SAMPLER_MIPMAP_MODE_MAX_ENUM;
	}
}

VkSamplerAddressMode translate_sampler_address_mode(RHISamplerAddressMode::Value address_mode) {
	switch (address_mode) {
	case RHISamplerAddressMode::kRepeat: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
	case RHISamplerAddressMode::kMirroredRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
	case RHISamplerAddressMode::kClampToEdge: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	case RHISamplerAddressMode::kClampToBorder: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	case RHISamplerAddressMode::kMirrorClampToEdge: return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
	default:
		assert(0 && "Invalid mipmap mode!");
		return VK_SAMPLER_ADDRESS_MODE_MAX_ENUM;
	}
}

VkCompareOp translate_compare_op(RHICompareOp::Value compare_op) {
	switch(compare_op) {
	case RHICompareOp::kNever: return VK_COMPARE_OP_NEVER;
	case RHICompareOp::kLess: return VK_COMPARE_OP_LESS;
	case RHICompareOp::kEqual: return VK_COMPARE_OP_EQUAL;
	case RHICompareOp::kLessOrEqual: return VK_COMPARE_OP_LESS_OR_EQUAL;
	case RHICompareOp::kGreater: return VK_COMPARE_OP_GREATER;
	case RHICompareOp::kNotEqual: return VK_COMPARE_OP_NOT_EQUAL;
	case RHICompareOp::kGreaterOrEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
	case RHICompareOp::kAlways: return VK_COMPARE_OP_ALWAYS;
	default:
		assert(0 && "Invalid mipmap mode!");
		return VK_COMPARE_OP_MAX_ENUM;
	}
}

VkDescriptorType translate_desc_type(RHIDescriptorType::Value type) {
	switch (type) {
	case RHIDescriptorType::kSampler: return VK_DESCRIPTOR_TYPE_SAMPLER;
	case RHIDescriptorType::kCombinedImageSampler:return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	case RHIDescriptorType::kSampledImage:return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	case RHIDescriptorType::kStorageImage:return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	case RHIDescriptorType::kUniformTexelBuffer:return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
	case RHIDescriptorType::kStorageTexelBuffer:return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
	case RHIDescriptorType::kUniformBuffer:return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	case RHIDescriptorType::kStorageBuffer:return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	case RHIDescriptorType::kUniformBufferDynamic:return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	case RHIDescriptorType::kStorageBufferDynamic:return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
	case RHIDescriptorType::kInputAttachment:return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	case RHIDescriptorType::kInlineUniformBlockExt:return VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT;
	case RHIDescriptorType::kAccelerationStructureKHR:return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	default:
		assert(0 && "Invalid descriptor type!");
		return VK_DESCRIPTOR_TYPE_MAX_ENUM;
	}
}

//////////////////////// Common functions ////////////////////////////////////////////////////////
VkDeviceMemory allocate_memory(const VulkanDevice& device, const VkMemoryRequirements &mem_req,
							   const VkMemoryPropertyFlags& mem_prop) {

	VkPhysicalDevice phys_dev = device.phys_device_;
	VkDevice dev = device.device_;

	VkPhysicalDeviceMemoryProperties memory_properties;
	vkGetPhysicalDeviceMemoryProperties(phys_dev, &memory_properties);

	VkDeviceMemory vk_mem = VK_NULL_HANDLE;
	for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
		if ((mem_req.memoryTypeBits & (1 << i)) &&
			(memory_properties.memoryTypes[i].propertyFlags & mem_prop)) {

			VkMemoryAllocateInfo mem_alloc_info = {
				VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, // VkStructureType sType
				nullptr,	  // const void                            *pNext
				mem_req.size, // VkDeviceSize allocationSize
				i			  // uint32_t                               memoryTypeIndex
			};

			if (vkAllocateMemory(dev, &mem_alloc_info, device.pallocator_, &vk_mem) ==
				VK_SUCCESS) {
				break;
			} else {
				log_error("allocate_memory: failed to allocate memory, looking for another memory type\n");
			}
		}
	}
	assert(vk_mem);
	return vk_mem;
}

////////////// Image //////////////////////////////////////////////////

void RHIImageVk::Destroy(IRHIDevice* device) {
	RHIDeviceVk* dev = ResourceCast(device);
	vkDestroyImage(dev->Handle(), handle_, dev->Allocator());
	// TODO: should we destroy image memory?
}

//void RHIImageVk::SetImage(const rhi_vulkan::Image& image) {
//	image_ = image;
//	format_ = untranslate(image.format_);
//}

////////////// Image View //////////////////////////////////////////////////
void RHIImageViewVk::Destroy(IRHIDevice* device) {
	RHIDeviceVk* dev = ResourceCast(device);
	vkDestroyImageView(dev->Handle(), handle_, dev->Allocator());
}

////////////// Sampler //////////////////////////////////////////////////
void RHISamplerVk::Destroy(IRHIDevice* device) {
	RHIDeviceVk* dev = ResourceCast(device);
	vkDestroySampler(dev->Handle(), handle_, dev->Allocator());
}

////////////// DescriptorSetLayout //////////////////////////////////////////////////
static std::vector<VkDescriptorSetLayoutBinding>
translate_dsl_bindings(const RHIDescriptorSetLayoutDesc *desc, int count);
RHIDescriptorSetLayoutVk::RHIDescriptorSetLayoutVk(VkDescriptorSetLayout dsl,
												   const RHIDescriptorSetLayoutDesc *desc,
												   int count)
	: IRHIDescriptorSetLayout(desc, count), handle_(dsl) {
	vk_bindings_ = translate_dsl_bindings(desc, count);
}

void RHIDescriptorSetLayoutVk::Destroy(IRHIDevice* device) {
	RHIDeviceVk* dev = ResourceCast(device);
	vkDestroyDescriptorSetLayout(dev->Handle(), handle_, dev->Allocator());
}

////////////// Frame Buffer //////////////////////////////////////////////////

void RHIFrameBufferVk::Destroy(IRHIDevice* device) {
	RHIDeviceVk* dev = ResourceCast(device);
	vkDestroyFramebuffer(dev->Handle(), handle_, dev->Allocator());
	delete this;
}

#if 0
////////////// Render Pass //////////////////////////////////////////////////

void RHIRenderPassVk::Destroy(IRHIDevice* device) {
	RHIDeviceVk* dev = ResourceCast(device);
	vkDestroyRenderPass(dev->Handle(), handle_, dev->Allocator());
	delete this;
}

#endif

////////////// Shader //////////////////////////////////////////////////

void RHIShaderVk::Destroy(IRHIDevice* device) {
	RHIDeviceVk* dev = ResourceCast(device);
	vkDestroyShaderModule(dev->Handle(), shader_module_, dev->Allocator());
	delete this;
}

RHIShaderVk *RHIShaderVk::Create(IRHIDevice *device, const uint32_t *pdata, uint32_t size,
								 RHIShaderStageFlagBits::Value stage) {
	VkShaderModuleCreateInfo ci = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr, 0, size,
								   pdata};

	VkShaderModule shader_module;
	RHIDeviceVk *dev = ResourceCast(device);
	if (vkCreateShaderModule(dev->Handle(), &ci, dev->Allocator(), &shader_module) != VK_SUCCESS) {
		log_error("Could not create shader module\n");
		return nullptr;
	}

	RHIShaderVk *shader = new RHIShaderVk();
	shader->shader_module_ = shader_module;
	shader->code_.resize(size);
	memcpy(shader->code_.data(), pdata, size);
	shader->vk_stage_ = translate_ssflag(stage);
	shader->stage_ = stage;

	return shader;
}


////////////// Pipeline Layout //////////////////////////////////////////////////

void RHIPipelineLayoutVk::Destroy(IRHIDevice* device) {
	RHIDeviceVk* dev = ResourceCast(device);
	vkDestroyPipelineLayout(dev->Handle(), handle_, dev->Allocator());
	delete this;
}

RHIPipelineLayoutVk *RHIPipelineLayoutVk::Create(IRHIDevice *device,
												 const IRHIDescriptorSetLayout* const* desc_set_layouts, uint32_t count) {
	std::vector<VkDescriptorSetLayout> vk_layout_arr(count);
	for (int i = 0; i < (int)count; ++i) {
		const RHIDescriptorSetLayoutVk* ds_layout = ResourceCast(desc_set_layouts[i]);
		vk_layout_arr[i] = ds_layout->Handle();
	}

	VkPipelineLayoutCreateInfo ci = {
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, 
      nullptr,                                        
      0,                                              
      (uint32_t)vk_layout_arr.size(),// uint32_t                       setLayoutCount
      vk_layout_arr.data(),          // const VkDescriptorSetLayout   *pSetLayouts
      0,                             // uint32_t                       pushConstantRangeCount
      nullptr                        // const VkPushConstantRange     *pPushConstantRanges
    };

	VkPipelineLayout pipeline_layout;
	RHIDeviceVk *dev = ResourceCast(device);
	if (vkCreatePipelineLayout(dev->Handle(), &ci, dev->Allocator(), &pipeline_layout) !=
		VK_SUCCESS) {
		log_error("Could not create pipeline layout!");
		return nullptr;
	}

	RHIPipelineLayoutVk *pl = new RHIPipelineLayoutVk();
	for (int i = 0; i < (int)count; ++i) {
		pl->ds_layouts.push_back(desc_set_layouts[i]);
	}
	pl->handle_ = pipeline_layout;
	return pl;
}

////////////// Graphics Pipeline //////////////////////////////////////////////////
void RHIGraphicsPipelineVk::Destroy(IRHIDevice* device) {
	RHIDeviceVk* dev = ResourceCast(device);
	vkDestroyPipeline(dev->Handle(), handle_, dev->Allocator());
	delete this;
}

RHIGraphicsPipelineVk *RHIGraphicsPipelineVk::Create(
	IRHIDevice *device, const RHIShaderStage *shader_stage, uint32_t shader_stage_count,
	const RHIVertexInputState *vertex_input_state,
	const RHIInputAssemblyState *input_assembly_state, const RHIViewportState *viewport_state,
	const RHIRasterizationState *raster_state, const RHIMultisampleState *multisample_state,
	const RHIColorBlendState *color_blend_state, const IRHIPipelineLayout *i_pipleline_layout,
	const IRHIRenderPass *i_render_pass) {

	RHIDeviceVk* dev = ResourceCast(device);
	std::vector<VkPipelineShaderStageCreateInfo> vk_shader_stage(shader_stage_count);
	for (uint32_t i = 0; i < shader_stage_count; ++i) {
		vk_shader_stage[i] = {
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			translate_ssflag(shader_stage[i].stage),
			ResourceCast(shader_stage[i].module)->Handle(),
			shader_stage[i].pEntryPointName,
			nullptr
		};
	}

	std::vector<VkVertexInputBindingDescription> vertex_input_bindings(
		vertex_input_state->vertexBindingDescCount);
	for (uint32_t i = 0; i < (uint32_t)vertex_input_bindings.size(); ++i) {
		vertex_input_bindings[i].binding = vertex_input_state->pVertexBindingDesc[i].binding;
		vertex_input_bindings[i].stride = vertex_input_state->pVertexBindingDesc[i].stride;
		vertex_input_bindings[i].inputRate = translate_vir(vertex_input_state->pVertexBindingDesc[i].inputRate);
	}

	std::vector<VkVertexInputAttributeDescription> vertex_input_attributes(
		vertex_input_state->vertexAttributeDescCount);
	for (uint32_t i = 0; i < (uint32_t)vertex_input_attributes.size(); ++i) {
		vertex_input_attributes[i].location = vertex_input_state->pVertexAttributeDesc[i].location;
		vertex_input_attributes[i].binding = vertex_input_state->pVertexAttributeDesc[i].binding;
		vertex_input_attributes[i].format = translate_f(vertex_input_state->pVertexAttributeDesc[i].format);
		vertex_input_attributes[i].offset = vertex_input_state->pVertexAttributeDesc[i].offset;
	}

	VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info = {
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      nullptr,
      0,
      (uint32_t)vertex_input_bindings.size(),
      vertex_input_bindings.data(),
      (uint32_t)vertex_input_attributes.size(),
      vertex_input_attributes.data(),
    };

	VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info = {
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      nullptr,
      0,
      translate_pt(input_assembly_state->topology),
	  input_assembly_state->primitiveRestartEnable
    };


	std::vector<VkViewport> vk_viewports(viewport_state->viewportCount);
	for (uint32_t i = 0; i < (uint32_t)vk_viewports.size(); ++i) {
		vk_viewports[i].x = viewport_state->pViewports[i].x;
		vk_viewports[i].y = viewport_state->pViewports[i].y;
		vk_viewports[i].width = viewport_state->pViewports[i].width;
		vk_viewports[i].height = viewport_state->pViewports[i].height;
		vk_viewports[i].minDepth = viewport_state->pViewports[i].minDepth;
		vk_viewports[i].maxDepth = viewport_state->pViewports[i].maxDepth;
	}

	std::vector<VkRect2D> vk_scissors(viewport_state->scissorCount);
	for (uint32_t i = 0; i < (uint32_t)vk_scissors.size(); ++i) {
		vk_scissors[i].offset.x = viewport_state->pScissors[i].x;
		vk_scissors[i].offset.y = viewport_state->pScissors[i].y;
		vk_scissors[i].extent.width = viewport_state->pScissors[i].width;
		vk_scissors[i].extent.height = viewport_state->pScissors[i].height;
	}

	VkPipelineViewportStateCreateInfo viewport_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		nullptr,
		0,
		(uint32_t)vk_viewports.size(),
		vk_viewports.data(),
		(uint32_t)vk_scissors.size(),
		vk_scissors.data(),
	};
		
	VkPipelineRasterizationStateCreateInfo rasterization_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		nullptr,
		0,
		raster_state->depthClampEnable,
		raster_state->rasterizerDiscardEnable,
		translate_pm(raster_state->polygonMode),
		translate_cm(raster_state->cullMode),
		translate_ff(raster_state->frontFace),
		raster_state->depthBiasEnable,
        raster_state->depthBiasConstantFactor,
        raster_state->depthBiasClamp,
        raster_state->depthBiasSlopeFactor,
        raster_state->lineWidth
    };

	VkPipelineMultisampleStateCreateInfo multisample_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType sType
		nullptr, // const void                                    *pNext
		0,		 // VkPipelineMultisampleStateCreateFlags          flags
		translate_num_samples(
			multisample_state->rasterizationSamples), // VkSampleCountFlagBits rasterizationSamples
		multisample_state->sampleShadingEnable,		  // VkBool32 sampleShadingEnable
		multisample_state->minSampleShading,		  // float minSampleShading
		multisample_state->pSampleMask,				  // const VkSampleMask *pSampleMask
		multisample_state->alphaToCoverageEnable,	  // VkBool32 alphaToCoverageEnable
		multisample_state->alphaToOneEnable			  // VkBool32 alphaToOneEnable
    };

	std::vector<VkPipelineColorBlendAttachmentState> arr_color_blend_attachment_state;
	for (uint32_t i = 0; i < color_blend_state->attachmentCount; ++i) {
		const RHIColorBlendAttachmentState* cb_state = color_blend_state->pAttachments + i;
		VkPipelineColorBlendAttachmentState state;
		state.blendEnable = cb_state->blendEnable;
		state.srcColorBlendFactor = translate_bf(cb_state->srcColorBlendFactor);
		state.dstColorBlendFactor = translate_bf(cb_state->dstColorBlendFactor);
		state.colorBlendOp = translate_bo(cb_state->colorBlendOp);
		state.srcAlphaBlendFactor = translate_bf(cb_state->srcAlphaBlendFactor);
		state.dstAlphaBlendFactor = translate_bf(cb_state->dstAlphaBlendFactor);
		state.alphaBlendOp = translate_bo(cb_state->alphaBlendOp);
		state.colorWriteMask = translate_color_comp(cb_state->colorWriteMask);

		arr_color_blend_attachment_state.push_back(state);
	}

	VkPipelineColorBlendStateCreateInfo color_blend_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType sType
		nullptr,						  // const void                                    *pNext
		0,								  // VkPipelineColorBlendStateCreateFlags           flags
		color_blend_state->logicOpEnable, // VkBool32 logicOpEnable
		translate_lo(color_blend_state->logicOp),			   // VkLogicOp logicOp
		(uint32_t)arr_color_blend_attachment_state.size(), // uint32_t attachmentCount
		arr_color_blend_attachment_state.data(), 
		{
			color_blend_state->blendConstants[0],
			color_blend_state->blendConstants[1],
			color_blend_state->blendConstants[2],
			color_blend_state->blendConstants[3],
		}                                    
    };

	const RHIPipelineLayoutVk* playout = ResourceCast(i_pipleline_layout);
	const RHIRenderPassVk* render_pass = ResourceCast(i_render_pass);

	VkGraphicsPipelineCreateInfo pipeline_create_info = {
      VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,              // VkStructureType                                sType
      nullptr,                                                      // const void                                    *pNext
      0,                                                            // VkPipelineCreateFlags                          flags
      (uint32_t)vk_shader_stage.size(),      // uint32_t                                       stageCount
      vk_shader_stage.data(),                             // const VkPipelineShaderStageCreateInfo         *pStages
      &vertex_input_state_create_info,                              // const VkPipelineVertexInputStateCreateInfo    *pVertexInputState;
      &input_assembly_state_create_info,                            // const VkPipelineInputAssemblyStateCreateInfo  *pInputAssemblyState
      nullptr,                                                      // const VkPipelineTessellationStateCreateInfo   *pTessellationState
      &viewport_state_create_info,                                  // const VkPipelineViewportStateCreateInfo       *pViewportState
      &rasterization_state_create_info,                             // const VkPipelineRasterizationStateCreateInfo  *pRasterizationState
      &multisample_state_create_info,                               // const VkPipelineMultisampleStateCreateInfo    *pMultisampleState
      nullptr,                                                      // const VkPipelineDepthStencilStateCreateInfo   *pDepthStencilState
      &color_blend_state_create_info,                               // const VkPipelineColorBlendStateCreateInfo     *pColorBlendState
      nullptr,                                                      // const VkPipelineDynamicStateCreateInfo        *pDynamicState
      playout ? playout->Handle() : VK_NULL_HANDLE,                 // VkPipelineLayout                               layout
      render_pass->Handle(),                                        // VkRenderPass                                   renderPass
      0,                                                            // uint32_t                                       subpass
      VK_NULL_HANDLE,                                               // VkPipeline                                     basePipelineHandle
      -1                                                            // int32_t                                        basePipelineIndex
    };

	VkPipeline pipeline;
	if (vkCreateGraphicsPipelines(dev->Handle(), VK_NULL_HANDLE, 1, &pipeline_create_info,
								  dev->Allocator(), &pipeline) != VK_SUCCESS) {
		log_error("Could not create graphics pipeline!\n");
		return nullptr;
	}

	RHIGraphicsPipelineVk* vk_pipeline = new RHIGraphicsPipelineVk(pipeline, playout);
	return vk_pipeline;
}

////////////////////////////////////////////////////////////////////////
IRHIShader* RHIDeviceVk::CreateShader(RHIShaderStageFlagBits::Value stage, const uint32_t *pdata, uint32_t size) {
    return RHIShaderVk::Create(this, pdata, size, stage);
}


////////////// Buffer //////////////////////////////////////////////////
RHIBufferVk* RHIBufferVk::Create(IRHIDevice* device, uint32_t size, uint32_t usage, uint32_t memprop, RHISharingMode::Value sharing) {
    VkBufferCreateInfo buffer_create_info = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,             // VkStructureType        sType
        nullptr,                                          // const void            *pNext
        0,                                                // VkBufferCreateFlags    flags
        size,                                            // VkDeviceSize           size
        translate_buffer_usage(usage),                // VkBufferUsageFlags     usage
        translate_sharing_mode(sharing),				// VkSharingMode          sharingMode
        0,                                                // uint32_t               queueFamilyIndexCount
        nullptr                                           // const uint32_t        *pQueueFamilyIndices
    };

    VkBuffer vk_buffer;
	RHIDeviceVk* dev = ResourceCast(device);
    if( vkCreateBuffer(dev->Handle(), &buffer_create_info, dev->Allocator(), &vk_buffer) != VK_SUCCESS ) {
        log_error("Could not create a vertex buffer!\n");
        return nullptr;
    }

    VkMemoryRequirements buffer_memory_req;
    vkGetBufferMemoryRequirements(dev->Handle(), vk_buffer, &buffer_memory_req);

    // TODO: maybe store this in device itself?
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(dev->PhysDeviceHandle(), &memory_properties );

    VkDeviceMemory vk_mem = VK_NULL_HANDLE;
    VkMemoryPropertyFlags vk_memprop = translate_mem_prop(memprop);
	for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
		if ((buffer_memory_req.memoryTypeBits & (1 << i)) &&
			(memory_properties.memoryTypes[i].propertyFlags & vk_memprop)) {

			VkMemoryAllocateInfo mem_alloc_info = {
				VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, // VkStructureType sType
				nullptr,						        // const void                            *pNext
				buffer_memory_req.size,                 // VkDeviceSize allocationSize
				i                                       // uint32_t                               memoryTypeIndex
			};

			if (vkAllocateMemory(dev->Handle(), &mem_alloc_info, dev->Allocator(), &vk_mem) == VK_SUCCESS) {
				break;
			}
		}
	}

	if (!vk_mem) return nullptr;

	if (vkBindBufferMemory(dev->Handle(), vk_buffer, vk_mem, 0) != VK_SUCCESS) {
		log_error("Could not bind memory for a vertex buffer!\n");
		return nullptr;
	}

	RHIBufferVk* buffer = new RHIBufferVk;
    buffer->handle_ = vk_buffer;
    buffer->backing_mem_ = vk_mem;
    buffer->buf_size_ = size;
    buffer->usage_flags_ = usage;
    buffer->mem_flags_ = memprop;
    buffer->is_mapped_ = false;

    return buffer;
}

void RHIBufferVk::Destroy(IRHIDevice* device) {
	RHIDeviceVk* dev = ResourceCast(device);
	vkDestroyBuffer(dev->Handle(), handle_, dev->Allocator());
    vkFreeMemory(dev->Handle(), backing_mem_, dev->Allocator());
	delete this;
}

void *RHIBufferVk::Map(IRHIDevice* device, uint32_t offset, uint32_t size, uint32_t map_flags) {
    assert(!is_mapped_);
	assert(this->buf_size_ >= offset + size);

	RHIDeviceVk* dev = ResourceCast(device);
	void *ptr;
	if (vkMapMemory(dev->Handle(), backing_mem_, offset, size, map_flags, &ptr) != VK_SUCCESS) {
		log_error("Could not map memory and upload data to a vertex buffer!\n");
		return nullptr;
	}

    is_mapped_ = true;
    mapped_offset_ = offset;
    mapped_size_ = size;
    mapped_flags_ = map_flags;

    return ptr;
}

void RHIBufferVk::Unmap(IRHIDevice* device) {
    assert(is_mapped_);

	RHIDeviceVk* dev = ResourceCast(device);
	
    VkMappedMemoryRange flush_range = {
      VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,            // VkStructureType        sType
      nullptr,                                          // const void            *pNext
      backing_mem_,                                     // VkDeviceMemory         memory
      mapped_offset_,                                   // VkDeviceSize           offset
      VK_WHOLE_SIZE,//  mapped_size_                                      // VkDeviceSize           size
    };
	vkFlushMappedMemoryRanges(dev->Handle(), 1, &flush_range);

	vkUnmapMemory(dev->Handle(), backing_mem_);
    is_mapped_ = false;
}

/////////////////////// Fence //////////////////////////////////////////////////
void RHIFenceVk::Reset(IRHIDevice *device) {
	RHIDeviceVk *dev = ResourceCast(device);
    VK_CHECK(vkResetFences(dev->Handle(), 1, &handle_));
}
void RHIFenceVk::Wait(IRHIDevice *device, uint64_t timeout) {
	RHIDeviceVk *dev = ResourceCast(device);
    VK_CHECK(vkWaitForFences(dev->Handle(), 1, &handle_, false, timeout));
}
bool RHIFenceVk::IsSignalled(IRHIDevice *device) {
	RHIDeviceVk *dev = ResourceCast(device);
    VkResult status = vkGetFenceStatus(dev->Handle(), handle_);
    return VK_SUCCESS == status;
}
void RHIFenceVk::Destroy(IRHIDevice *device) {
	RHIDeviceVk *dev = ResourceCast(device);
    vkDestroyFence(dev->Handle(), handle_, dev->Allocator());
    delete this;
}
RHIFenceVk *RHIFenceVk::Create(IRHIDevice *device, bool create_signalled) {
	VkFenceCreateInfo fence_create_info = {
		VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,				 // VkStructureType                sType
		nullptr,											 // const void                    *pNext
		create_signalled ? VK_FENCE_CREATE_SIGNALED_BIT : 0u // VkFenceCreateFlags             flags
	};

	RHIDeviceVk *dev = ResourceCast(device);

	VkFence vk_fence;
	if (vkCreateFence(dev->Handle(), &fence_create_info, dev->Allocator(), &vk_fence) !=
		VK_SUCCESS) {
		log_error("Could not create a fence!\n");
		return nullptr;
	}

	RHIFenceVk *fence = new RHIFenceVk();
	fence->handle_ = vk_fence;
	return fence;
}

/////////////////////// Event //////////////////////////////////////////////////
void RHIEventVk::Set(IRHIDevice *device) {
	RHIDeviceVk *dev = ResourceCast(device);
    VK_CHECK(vkSetEvent(dev->Handle(), handle_));
}
void RHIEventVk::Reset(IRHIDevice *device) {
	RHIDeviceVk *dev = ResourceCast(device);
    VK_CHECK(vkResetEvent(dev->Handle(), handle_));
}
bool RHIEventVk::IsSet(IRHIDevice *device) {
	RHIDeviceVk *dev = ResourceCast(device);
    VkResult status = vkGetEventStatus(dev->Handle(), handle_);
    return VK_EVENT_SET == status;
}
void RHIEventVk::Destroy(IRHIDevice *device) {
	RHIDeviceVk *dev = ResourceCast(device);
    vkDestroyEvent(dev->Handle(), handle_, dev->Allocator());
    delete this;
}

RHIEventVk *RHIEventVk::Create(IRHIDevice *device) {
	VkEventCreateInfo event_create_info = {
		VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,				 // VkStructureType                sType
		nullptr,											 // const void                    *pNext
		0                                                   // VkEventCreateFlags             flags
	};

	RHIDeviceVk *dev = ResourceCast(device);

	VkEvent vk_event;
	if (vkCreateEvent(dev->Handle(), &event_create_info, dev->Allocator(), &vk_event) !=
		VK_SUCCESS) {
		log_error("Could not create an event !\n");
		return nullptr;
	}

	RHIEventVk *event = new RHIEventVk();
	event->handle_ = vk_event;
	return event;
}

////////////// Command buffer //////////////////////////////////////////////////

void Barrier(VkCommandBuffer cb, RHIImageVk* image,
			 VkPipelineStageFlags src_pipeline_stage_bits,
			 VkPipelineStageFlags dst_pipeline_stage_bits, VkAccessFlags new_access_flags,
			 VkImageLayout new_layout) {

	// TODO: incorporate this int oparameters, or use image view somehow and grab it from that
	VkImageSubresourceRange image_subresource_range = {
		VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags                     aspectMask
		0,						   // uint32_t                               baseMipLevel
		1,						   // uint32_t                               levelCount
		0,						   // uint32_t                               baseArrayLayer
		1						   // uint32_t                               layerCount
	};

	//VkImageLayout new_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	//VkAccessFlagBits new_access_flag_bits = VK_ACCESS_MEMORY_READ_BIT;
	VkImageMemoryBarrier barrier_copy2present = {
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType                        sType
		nullptr,								// const void                            *pNext
		image->vk_access_flags_,					// VkAccessFlags                          srcAccessMask
		new_access_flags,						// VkAccessFlags                          dstAccessMask
		image->vk_layout_,							// VkImageLayout                          oldLayout
		new_layout,								// VkImageLayout                          newLayout
		VK_QUEUE_FAMILY_IGNORED,				// uint32_t                               srcQueueFamilyIndex
		VK_QUEUE_FAMILY_IGNORED,				// uint32_t                               dstQueueFamilyIndex
		image->Handle(),							// VkImage                                image
		image_subresource_range					// VkImageSubresourceRange                subresourceRange
	};

	vkCmdPipelineBarrier(cb, src_pipeline_stage_bits, dst_pipeline_stage_bits, 0, 0,
						 nullptr, 0, nullptr, 1, &barrier_copy2present);

	// warning: updating states like this is not always correct if we have different CBs and submit
	// them in different order
	image->vk_access_flags_ = new_access_flags;
	image->vk_layout_ = new_layout;
}

void RHICmdBufVk::BufferBarrier(IRHIBuffer *i_buffer, RHIAccessFlags::Value src_acc_flags,
								RHIPipelineStageFlags::Value src_stage, RHIAccessFlags::Value dst_acc_fags,
								RHIPipelineStageFlags::Value dst_stage) {
	RHIBufferVk* buffer = ResourceCast(i_buffer);

    VkBufferMemoryBarrier buffer_memory_barrier = {
      VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,          // VkStructureType                        sType
      nullptr,                                          // const void                            *pNext
      translate_af(src_acc_flags),                       // VkAccessFlags                          srcAccessMask
      translate_af(dst_acc_fags),              // VkAccessFlags                          dstAccessMask
      VK_QUEUE_FAMILY_IGNORED,                          // uint32_t                               srcQueueFamilyIndex
      VK_QUEUE_FAMILY_IGNORED,                          // uint32_t                               dstQueueFamilyIndex
      buffer->Handle(),                       // VkBuffer                               buffer
      0,                                                // VkDeviceSize                           offset
      VK_WHOLE_SIZE                                     // VkDeviceSize                           size
    };
	vkCmdPipelineBarrier(cb_, translate_ps(src_stage), translate_ps(dst_stage), 0, 0, nullptr, 1,
						 &buffer_memory_barrier, 0, nullptr);
}

bool RHICmdBufVk::Begin() {

	VkCommandBufferBeginInfo cmd_buffer_begin_info = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // VkStructureType                        sType
		nullptr,									 // const void                            *pNext
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, // VkCommandBufferUsageFlags flags
		nullptr // const VkCommandBufferInheritanceInfo  *pInheritanceInfo
	};

	vkBeginCommandBuffer(cb_, &cmd_buffer_begin_info );

	is_recording_ = true;
	return true;
}

bool RHICmdBufVk::End() {
	if (vkEndCommandBuffer(cb_) != VK_SUCCESS) {
		log_error("vkEndCommandBuffer: Could not record command buffers!\n");
		return false;
	}
	is_recording_ = false;
	return true;
}

void RHICmdBufVk::EndRenderPass(const IRHIRenderPass *i_rp, IRHIFrameBuffer *i_fb) {
    assert(is_recording_);
    vkCmdEndRenderPass(cb_);

	const RHIRenderPassVk* rp = ResourceCast(i_rp);
    RHIFrameBufferVk* fb = ResourceCast(i_fb);
	const std::vector<RHIImageViewVk*>& attachments = fb->GetAttachments();
	for (int i = 0; i < (int)attachments.size();++i) {
		RHIImageViewVk* att = attachments[i];
        RHIImageVk* img = ResourceCast(att->GetImage());
        img->vk_layout_ = translate_il(rp->GetFinalLayout(i));
    }
}

bool RHICmdBufVk::BeginRenderPass(IRHIRenderPass *i_rp, IRHIFrameBuffer *i_fb, const ivec4 *render_area,
						const RHIClearValue *clear_values, uint32_t count) {
    assert(is_recording_);

	const RHIRenderPassVk* rp = ResourceCast(i_rp);
    const RHIFrameBufferVk* fb = ResourceCast(i_fb);

    VkRect2D ra;
    ra.offset.x = render_area->x;
    ra.offset.y = render_area->y;
    ra.extent.width = render_area->z;
    ra.extent.height = render_area->w;

    std::vector<VkClearValue> vk_clear_values(count);
    for(uint32_t i=0; i<count; ++i) {
        vk_clear_values[i].color.float32[0] = clear_values[i].colour.x;
        vk_clear_values[i].color.float32[1] = clear_values[i].colour.y;
        vk_clear_values[i].color.float32[2] = clear_values[i].colour.z;
        vk_clear_values[i].color.float32[3] = clear_values[i].colour.w;
        vk_clear_values[i].depthStencil.depth = clear_values[i].depth;
        vk_clear_values[i].depthStencil.stencil = clear_values[i].stencil;
    }

    VkRenderPassBeginInfo render_pass_begin_info = {
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        nullptr,
        rp->Handle(),
        fb->Handle(),
        ra,
        (uint32_t)vk_clear_values.size(),
        vk_clear_values.data()
    };
    vkCmdBeginRenderPass(cb_, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE );

    return true;
}

void RHICmdBufVk::BindPipeline(RHIPipelineBindPoint::Value bind_point, IRHIGraphicsPipeline* i_pipeline) {
    assert(is_recording_);
    const RHIGraphicsPipelineVk* pipeline = ResourceCast(i_pipeline);

    vkCmdBindPipeline(cb_, translate_pbp(bind_point), pipeline->Handle());
}

void RHICmdBufVk::BindDescriptorSets(RHIPipelineBindPoint::Value bind_point,
	const IRHIPipelineLayout* pipeline_layout,
	const IRHIDescriptorSet*const* desc_sets, uint32_t count) {

	const RHIPipelineLayoutVk* pipe_layout = ResourceCast(pipeline_layout);
	//TODO: stack allocated array
	std::vector<VkDescriptorSet> sets(count);
	for (int i = 0; i < (int)count; ++i) {
		// yep pointer compare, assume no same layouts in different objects, but can change in future
		const RHIDescriptorSetVk* set = ResourceCast(desc_sets[i]);
		bool b_found_compatible = false;
		for (int j = 0; j < pipe_layout->getDSLCount(); j++) {
			if (desc_sets[i]->getLayout() == pipe_layout->getLayout(j)) {
				b_found_compatible = true;
			}
		}
		assert(b_found_compatible);
		sets[i] = set->Handle();
	}
	// actually it is allowed to bind less descriptor sets than set in pipeline layout, but we have
	// a stronger condition here for now, just to catch some potential issues

	// !NB: not sure why stack gets corrupted on assert(s) :-/
	assert(pipe_layout->getDSLCount() == (int)count);
	vkCmdBindDescriptorSets(cb_, translate_pbp(bind_point), pipe_layout->Handle(), 0,
							(uint32_t)sets.size(), sets.data(), 0, nullptr);
}

void RHICmdBufVk::Draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex,
					  uint32_t first_instance) {
    assert(is_recording_);
    vkCmdDraw(cb_, vertex_count, instance_count, first_vertex, first_instance);
}

void RHICmdBufVk::BindVertexBuffers(IRHIBuffer** i_vb, uint32_t first_binding, uint32_t count) {
    std::vector<VkBuffer> vbs(count); // :-(
    std::vector<VkDeviceSize> offsets(count); // :-(
    for(uint32_t i=0;i<count; ++i) {
        RHIBufferVk* vb = ResourceCast(i_vb[i]);
        vbs[i] = vb->Handle();
        offsets[i] = 0;
    }
    vkCmdBindVertexBuffers(cb_, first_binding, count, vbs.data(), offsets.data());
}

void RHICmdBufVk::CopyBuffer(class IRHIBuffer *i_dst, uint32_t dst_offset, class IRHIBuffer *i_src,
							uint32_t src_offset, uint32_t size) {

	VkBufferCopy buffer_copy_info = {};
	buffer_copy_info.srcOffset = src_offset;// VkDeviceSize                           srcOffset
	buffer_copy_info.dstOffset = dst_offset;// VkDeviceSize                           dstOffset
	buffer_copy_info.size = size;         // VkDeviceSize                           size

    const RHIBufferVk* dst = ResourceCast(i_dst);
    const RHIBufferVk* src = ResourceCast(i_src);
    assert(dst->Size() >= dst_offset + size);
    assert(src->Size() >= src_offset + size);

    vkCmdCopyBuffer(cb_, src->Handle(), dst->Handle(), 1, &buffer_copy_info );
}

// just assumes should copy full image
void RHICmdBufVk::CopyBufferToImage2D(class IRHIImage*i_dst, class IRHIBuffer *i_src) {

	const RHIImageVk* img = ResourceCast(i_dst);
	const RHIBufferVk* buf = ResourceCast(i_src);

	VkBufferImageCopy buffer_image_copy_info = {
		0, // VkDeviceSize               bufferOffset
		0, // uint32_t                   bufferRowLength
		0, // uint32_t                   bufferImageHeight
		{
			// VkImageSubresourceLayers   imageSubresource
			VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags         aspectMask
			0,						   // uint32_t                   mipLevel
			0,						   // uint32_t                   baseArrayLayer
			1						   // uint32_t                   layerCount
		},
		{
			// VkOffset3D                 imageOffset
			0, // int32_t                    x
			0, // int32_t                    y
			0  // int32_t                    z
		},
		{
			// VkExtent3D                 imageExtent
			img->Width(),  // uint32_t                   width
			img->Height(), // uint32_t                   height
			1			   // uint32_t                   depth
		}
	};

	assert(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == img->vk_layout_);
	vkCmdCopyBufferToImage(cb_, buf->Handle(), img->Handle(), img->vk_layout_, 1, &buffer_image_copy_info);
}


void RHICmdBufVk::SetEvent(IRHIEvent* i_event, RHIPipelineStageFlags::Value stage) {
    const RHIEventVk* event = ResourceCast(i_event); 
    vkCmdSetEvent(cb_, event->Handle(), translate_ps(stage));
}
void RHICmdBufVk::ResetEvent(IRHIEvent* i_event, RHIPipelineStageFlags::Value stage) {
    const RHIEventVk* event = ResourceCast(i_event); 
    vkCmdResetEvent(cb_, event->Handle(), translate_ps(stage));
}

void RHICmdBufVk::Clear(IRHIImage* image_in, const vec4& color, uint32_t img_aspect_bits) {
	assert(is_recording_);
	VkImageAspectFlags clear_bits = translate_image_aspect(img_aspect_bits);

	RHIImageVk* image = ResourceCast(image_in);
	VkClearColorValue clear_color = {{color.x, color.y, color.z, color.w}};
	// TODO: use image view for this?
	VkImageSubresourceRange image_subresource_range = {
		clear_bits,					// VkImageAspectFlags                     aspectMask
		0,						   // uint32_t                               baseMipLevel
		1,						   // uint32_t                               levelCount
		0,						   // uint32_t                               baseArrayLayer
		1						   // uint32_t                               layerCount
	};

	assert(image->vk_layout_ == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	vkCmdClearColorImage(cb_, image->Handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_color,
						 1, &image_subresource_range);
}

void RHICmdBufVk::Barrier_ClearToPresent(IRHIImage *image_in) {
	RHIImageVk* image = ResourceCast(image_in);
	Barrier(this->Handle(), image, VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_ACCESS_MEMORY_READ_BIT,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}
void RHICmdBufVk::Barrier_PresentToClear(IRHIImage *image_in) {
	RHIImageVk* image = ResourceCast(image_in);
	Barrier(this->Handle(), image, VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
}

// FIXME: check if VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL is correct here
void RHICmdBufVk::Barrier_PresentToDraw(IRHIImage *image_in) {
	RHIImageVk* image = ResourceCast(image_in);
	Barrier(this->Handle(), image, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
}

void RHICmdBufVk::Barrier_DrawToPresent(IRHIImage *image_in) {
	RHIImageVk* image = ResourceCast(image_in);
	Barrier(this->Handle(), image, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_ACCESS_MEMORY_READ_BIT,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}

void RHICmdBufVk::Barrier_UndefinedToTransfer(IRHIImage *image_in) {
	RHIImageVk* image = ResourceCast(image_in);

	Barrier(this->Handle(), image, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
}

// NOTE: Fragment shader read!
void RHICmdBufVk::Barrier_TransferToShaderRead(IRHIImage *image_in) {
	RHIImageVk* image = ResourceCast(image_in);

	assert(image->vk_access_flags_ = VK_ACCESS_TRANSFER_WRITE_BIT);
	assert(image->vk_layout_ = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	Barrier(this->Handle(), image, VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}


////////////////RHI Device /////////////////////////////////////////////////////

// should this be mobved to command buffer class / .cpp file and just pass Device as a parameter ?
IRHICmdBuf* RHIDeviceVk::CreateCommandBuffer(RHIQueueType::Value queue_type) {

	assert(queue_type == RHIQueueType::kGraphics || queue_type == RHIQueueType::kPresentation);
	uint32_t qfi = (RHIQueueType::kGraphics == queue_type) ? dev_.queue_families_.graphics_
														   : dev_.queue_families_.present_;
	VkCommandPoolCreateFlags flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	VkCommandPoolCreateInfo cmd_pool_create_info = {
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, // VkStructureType              sType
		nullptr,									// const void*                  pNext
		flags,											// VkCommandPoolCreateFlags     flags
		qfi											// uint32_t                     queueFamilyIndex
	};

	VkCommandPool cmd_pool;
	if (!cmd_pools_.count(qfi)) {
		if (vkCreateCommandPool(dev_.device_, &cmd_pool_create_info, nullptr, &cmd_pool) !=
			VK_SUCCESS) {
			log_error("Could not create a command pool!\n");
			return nullptr;
		}
		cmd_pools_[qfi] = cmd_pool;
	}
	else {
		cmd_pool = cmd_pools_[qfi];
	}

	//uint32_t image_count = 0;
	//if ((vkGetSwapchainImagesKHR(dev_.device_, dev_.swap_chain_.swap_chain_, &image_count,
	//							 nullptr) != VK_SUCCESS) ||
	//	(image_count == 0)) {
	//	std::cout << "Could not get the number of swap chain images!" << std::endl;
	//	return false;
	//}

	//Vulkan.PresentQueueCmdBuffers.resize(image_count);

	VkCommandBufferAllocateInfo cmd_buffer_allocate_info = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // VkStructureType              sType
		nullptr,										// const void*                  pNext
		cmd_pool,										// VkCommandPool                commandPool
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,				// VkCommandBufferLevel         level
		1 /*image_count	*/									// uint32_t                     bufferCount
	};

	VkCommandBuffer cb;
	if (vkAllocateCommandBuffers(dev_.device_, &cmd_buffer_allocate_info, &cb) != VK_SUCCESS) {
		log_error("Could not allocate command buffers!\n");
		return nullptr;
	}

	return new RHICmdBufVk(cb/*, qfi, cmd_pool*/);

}

////////////////////////////////////////////////////////////////////////////////
IRHIRenderPass* RHIDeviceVk::CreateRenderPass(const RHIRenderPassDesc* desc) {
	assert(desc);
	std::vector<VkAttachmentDescription> att_desc_arr(desc->attachmentCount);
	std::vector<VkSubpassDescription> subpass_desc_arr(desc->subpassCount);
	const size_t fixed_array_size = desc->subpassCount * 8;
	size_t ir_idx = 0;
	size_t cr_idx = 0;
	std::vector<VkAttachmentReference> input_ref_arr(fixed_array_size);
	std::vector<VkAttachmentReference> color_ref_arr(fixed_array_size);
	std::vector<VkAttachmentReference> depth_stencil_ref_arr(desc->subpassCount);
	std::vector<VkSubpassDependency> dep_arr(desc->dependencyCount);

	std::vector<RHIImageLayout::Value> att_final_layouts(desc->attachmentCount);
	RHIAttachmentDesc* att_desc = desc->attachmentDesc;
	for (int i = 0; i < desc->attachmentCount; ++i) {
		att_desc_arr[i].flags = 0;
		att_desc_arr[i].format = translate_f(att_desc[i].format);
		att_desc_arr[i].samples = translate_num_samples(att_desc[i].numSamples);
		att_desc_arr[i].loadOp = translate_alo(att_desc[i].loadOp);
		att_desc_arr[i].storeOp = translate_aso(att_desc[i].storeOp);
		att_desc_arr[i].stencilLoadOp = translate_alo(att_desc[i].stencilLoadOp);
		att_desc_arr[i].stencilStoreOp = translate_aso(att_desc[i].stencilStoreOp);
		att_desc_arr[i].initialLayout = translate_il(att_desc[i].initialLayout);
		att_desc_arr[i].finalLayout = translate_il(att_desc[i].finalLayout);

        att_final_layouts[i] = att_desc[i].finalLayout;
	}

	int off_ir = 0;
	int off_cr = 0;
	for (int i = 0; i < desc->subpassCount; ++i) {
		const RHISubpassDesc& sp_desc = desc->subpassDesc[i];
		subpass_desc_arr[i].flags = 0;
		subpass_desc_arr[i].pipelineBindPoint = translate_pbp(sp_desc.bindPoint);
		subpass_desc_arr[i].inputAttachmentCount = sp_desc.inputAttachmentCount;
		subpass_desc_arr[i].colorAttachmentCount = sp_desc.colorAttachmentCount;
		subpass_desc_arr[i].preserveAttachmentCount = sp_desc.preserveAttachmentCount;

		for (int ia = 0; ia < sp_desc.inputAttachmentCount; ++ia) {
			VkAttachmentReference ar;
			ar.attachment = sp_desc.inputAttachments[ia].index;
			ar.layout = translate(sp_desc.inputAttachments[ia].layout);
			input_ref_arr[ir_idx++] = ar;
		}
		subpass_desc_arr[i].pInputAttachments =
			sp_desc.inputAttachmentCount ? input_ref_arr.data() + off_ir : nullptr;

		for (int ca = 0; ca < sp_desc.colorAttachmentCount; ++ca) {
			VkAttachmentReference ar;
			ar.attachment = sp_desc.colorAttachments[ca].index;
			ar.layout = translate(sp_desc.colorAttachments[ca].layout);
			color_ref_arr[cr_idx++] = ar;
		}
		subpass_desc_arr[i].pColorAttachments =
			sp_desc.colorAttachmentCount ? color_ref_arr.data() + off_ir : nullptr;

		subpass_desc_arr[i].pDepthStencilAttachment = nullptr;
		if(sp_desc.depthStencilAttachment)
		{
			VkAttachmentReference ar;
			ar.attachment = sp_desc.depthStencilAttachment->index;
			ar.layout = translate(sp_desc.depthStencilAttachment->layout);
			depth_stencil_ref_arr[i] = ar;
			subpass_desc_arr[i].pDepthStencilAttachment = &depth_stencil_ref_arr[i];
		}

		subpass_desc_arr[i].pPreserveAttachments = subpass_desc_arr[i].pPreserveAttachments;

		off_ir = ir_idx;
		off_cr = cr_idx;
	}

	assert(ir_idx <= fixed_array_size);
	assert(cr_idx <= fixed_array_size);

	for (int i = 0; i < desc->dependencyCount; ++i) {
		const RHISubpassDependency& sp_dep = desc->dependencies[i];
		dep_arr[i].srcSubpass = sp_dep.srcSubpass;
		dep_arr[i].dstSubpass = sp_dep.dstSubpass;
		dep_arr[i].srcStageMask = translate(sp_dep.srcStageMask);
		dep_arr[i].dstStageMask = translate(sp_dep.dstStageMask);
		dep_arr[i].srcAccessMask = translate(sp_dep.srcAccessMask);
		dep_arr[i].dstAccessMask = translate(sp_dep.dstAccessMask);
		dep_arr[i].dependencyFlags = translate_dependency_flags(sp_dep.dependencyFlags);
	}

	VkRenderPassCreateInfo info = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
								   nullptr,
								   0,
								   (uint32_t)att_desc_arr.size(),
								   att_desc_arr.data(),
								   (uint32_t)subpass_desc_arr.size(),
								   subpass_desc_arr.data(),
								   (uint32_t)dep_arr.size(),
								   dep_arr.data()};

	VkRenderPass rp;
	if (vkCreateRenderPass(dev_.device_, &info, dev_.pallocator_, &rp) != VK_SUCCESS) {
		log_error("Could not create render pass!\n");
		return nullptr;
	}
    return new RHIRenderPassVk(rp, att_final_layouts);
}

IRHIFrameBuffer* RHIDeviceVk::CreateFrameBuffer(RHIFrameBufferDesc* desc, const IRHIRenderPass* rp_in) {
	std::vector<VkImageView> vk_img_views_arr(desc->attachmentCount);
	std::vector<RHIImageViewVk*> img_views_arr(desc->attachmentCount);
	for (uint32_t i = 0; i < desc->attachmentCount; ++i) {
		RHIImageViewVk* view = ResourceCast(desc->pAttachments[i]);
		vk_img_views_arr[i] = view->Handle();
		img_views_arr[i] = view;
	}

	const RHIRenderPassVk* rp = ResourceCast(rp_in);

	VkFramebufferCreateInfo info = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
									nullptr,
									0,
									rp->Handle(),
									(uint32_t)vk_img_views_arr.size(),
									vk_img_views_arr.data(),
									desc->width_,
									desc->height_,
									desc->layers_};

	VkFramebuffer fb;
	if (vkCreateFramebuffer(dev_.device_, &info, dev_.pallocator_, &fb) != VK_SUCCESS) {
		log_error("Could not create a framebuffer!");
		return nullptr;
	}

	return new RHIFrameBufferVk(fb, img_views_arr);
}

IRHIImage *RHIDeviceVk::CreateImage(const RHIImageDesc *desc, RHIImageLayout::Value initial_layout, RHIMemoryPropertyFlags mem_prop) {

	VkImageCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ci.pNext = nullptr;
	ci.flags = 0;
	ci.imageType = translate_itype(desc->type);
	ci.format = translate_f(desc->format);
	ci.extent = { desc->width, desc->height, desc->depth };
	ci.mipLevels = desc->numMips;
	ci.arrayLayers = desc->arraySize;
	// TODO: check form multisampling support capabilities?
	ci.samples = translate_sc(desc->numSamples);
	ci.tiling = translate_it(desc->tiling);
	ci.usage = translate_image_usage_flags(desc->usage);
	ci.sharingMode = translate_sharing_mode(desc->sharingMode);

	assert(desc->sharingMode == RHISharingMode::kExclusive);
		
	ci.queueFamilyIndexCount = 0;
	ci.pQueueFamilyIndices = nullptr;

	ci.initialLayout = translate_il(initial_layout);

	VkImage vk_image;
	if(vkCreateImage(dev_.device_, &ci, dev_.pallocator_, &vk_image) != VK_SUCCESS) {
		log_error("vkCreateImageView: failed to create image views!\n");
		return nullptr;
	}

	VkMemoryRequirements image_mem_req;
	vkGetImageMemoryRequirements(dev_.device_, vk_image, &image_mem_req);

	VkMemoryPropertyFlags vk_mem_prop = translate_mem_prop(mem_prop);
	VkDeviceMemory mem = allocate_memory(dev_, image_mem_req, vk_mem_prop);

	if (vkBindImageMemory(dev_.device_, vk_image, mem, 0) != VK_SUCCESS) {
		log_error("CreateImage: Could not bind memory to an image!\n");
		return false;
	}

	RHIImageVk* image = new RHIImageVk(vk_image, *desc, vk_mem_prop, ci.initialLayout);
	return image;
}

IRHIImageView *RHIDeviceVk::CreateImageView(const RHIImageViewDesc *desc) {
	assert(desc);
	RHIImageVk* image = ResourceCast(desc->image);

	VkImageViewCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	ci.image = image->Handle();
	ci.viewType = translate_ivt(desc->viewType);
	ci.format = translate_f(desc->format);
	ci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	ci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	ci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	ci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	ci.subresourceRange.aspectMask = translate_image_aspect(desc->subresourceRange.aspectMask);
	ci.subresourceRange.baseMipLevel = desc->subresourceRange.baseMipLevel;
	ci.subresourceRange.levelCount = desc->subresourceRange.levelCount;
	ci.subresourceRange.baseArrayLayer = desc->subresourceRange.baseArrayLayer;
	ci.subresourceRange.layerCount = desc->subresourceRange.layerCount;

	VkImageView image_view;
	if (vkCreateImageView(dev_.device_, &ci, dev_.pallocator_, &image_view) != VK_SUCCESS) {
		log_error("vkCreateImageView: failed to create image views!\n");
		return nullptr;
	}
	return new RHIImageViewVk(image_view, image);
}

IRHISampler *RHIDeviceVk::CreateSampler(const RHISamplerDesc& desc) {
	VkSamplerCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	ci.pNext = nullptr;
	ci.flags = 0;
	ci.magFilter = translate_filter(desc.magFilter);
	ci.minFilter = translate_filter(desc.minFilter);
	ci.mipmapMode = translate_sampler_mipmap_mode(desc.mipmapMode);
	ci.addressModeU = translate_sampler_address_mode(desc.addressModeU);
	ci.addressModeV = translate_sampler_address_mode(desc.addressModeV);
	ci.addressModeW = translate_sampler_address_mode(desc.addressModeW);
	ci.mipLodBias = 0.0f;
	ci.anisotropyEnable = VK_FALSE;
	ci.maxAnisotropy = 1.0f;
	ci.compareEnable = desc.compareEnable;
	ci.compareOp = translate_compare_op(desc.compareOp);
	ci.minLod = 0.0f;
	ci.maxLod = 0.0f;
	ci.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	ci.unnormalizedCoordinates = desc.unnormalizedCoordinates;

	VkSampler sampler;
	if (vkCreateSampler(dev_.device_, &ci, dev_.pallocator_, &sampler) != VK_SUCCESS) {
		log_error("vkCreateSampler: failed to create sampler!\n");
		return nullptr;
	}

	return new RHISamplerVk(sampler, desc);
}

// yes, I am returning vector by value... should not all those cool compilers do a "move", RVO or any other shenanigans?
static std::vector<VkDescriptorSetLayoutBinding> translate_dsl_bindings(const RHIDescriptorSetLayoutDesc* desc, int count) {
	std::vector<VkDescriptorSetLayoutBinding> bindings(count);
	for (int i = 0; i < count; ++i) {
		bindings[i].binding = desc[i].binding;
		bindings[i].descriptorCount = desc[i].count;
		bindings[i].descriptorType = translate_desc_type(desc[i].type);
		bindings[i].stageFlags = translate_ssflags(desc[i].shader_stage_flags);
		bindings[i].pImmutableSamplers = nullptr; // TODO:
	}
	return bindings;
}

IRHIDescriptorSetLayout* RHIDeviceVk::CreateDescriptorSetLayout(const RHIDescriptorSetLayoutDesc* desc, int count) {

	std::vector<VkDescriptorSetLayoutBinding> bindings;
	bindings = translate_dsl_bindings(desc, count);
	 
	VkDescriptorSetLayoutCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	ci.pNext = nullptr;
	// TODO:
	ci.flags = 0; //VkDescriptorSetLayoutCreateFlags     flags
	ci.bindingCount = (uint32_t)bindings.size();
	ci.pBindings = bindings.data();

	VkDescriptorSetLayout dsl;

	if (vkCreateDescriptorSetLayout(dev_.device_, &ci, dev_.pallocator_, &dsl) != VK_SUCCESS) {
		log_error("vkCreateDescriptorSetLayout: failed to create descriptor set layout!\n");
		return nullptr;
	}

	return new RHIDescriptorSetLayoutVk(dsl, desc, count);
}

struct DescPoolInfo {
	VkDescriptorPool pool;
	int num_alloc;
	int max;
	DescPoolInfo* pNext;
};

IRHIDescriptorSet* RHIDeviceVk::AllocateDescriptorSet(const IRHIDescriptorSetLayout* layout) {

	int num2alloc = 1;

	const RHIDescriptorSetLayoutVk* dsl = ResourceCast(layout);

	DescPoolInfo* desc_pool_info = nullptr;
	if (desc_pools_.count(dsl)) {
		desc_pool_info = desc_pools_[dsl];
		// if this triggers refactor code below into a separate function and create a new pool and
		// assign it to the pNext
		assert(desc_pool_info->max >= desc_pool_info->num_alloc + num2alloc);
	}
	else {

		uint32_t desc_types[RHIDescriptorType::kCount] = {};
		for (int i = 0; i < (int)dsl->bindings_.size(); ++i) {
			RHIDescriptorType::Value type = dsl->bindings_[i].type;
			assert(type >= 0 && type < RHIDescriptorType::kCount);
			desc_types[type]++;
		}

		int idx = 0;
		VkDescriptorPoolSize pool_sizes[RHIDescriptorType::kCount] = {};
		for (int i = 0; i < RHIDescriptorType::kCount; ++i) {
			if (desc_types[i]) {
				pool_sizes[idx].descriptorCount = desc_types[i];
				pool_sizes[idx].type = translate_desc_type((RHIDescriptorType::Value)i);
				idx++;
			}
		}

		uint32_t MaxSets = num2alloc + 10; // future proof :-)
		VkDescriptorPoolCreateInfo ci = {};
		ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		ci.pNext = nullptr;
		ci.flags = 0; // VkDescriptorPoolCreateFlags    flags
		ci.maxSets = MaxSets;
		ci.poolSizeCount = idx;
		ci.pPoolSizes = &pool_sizes[0];

		VkDescriptorPool vk_desc_pool;
		if (vkCreateDescriptorPool(dev_.device_, &ci, dev_.pallocator_, &vk_desc_pool) != VK_SUCCESS) {
			log_error("vkCreateDescriptorPool: Could not create descriptor pool!");
			return nullptr;
		}

		desc_pool_info = new DescPoolInfo();
		desc_pool_info->pool = vk_desc_pool;
		desc_pool_info->num_alloc = 0;
		desc_pool_info->pNext = nullptr;
		desc_pool_info->max = MaxSets;

		desc_pools_.insert(std::make_pair(dsl, desc_pool_info));
	}

	// actual allocation


	desc_pool_info->num_alloc += num2alloc;

	VkDescriptorSetLayout layouts[] = {
		dsl->Handle()
	};

	VkDescriptorSetAllocateInfo dsa_ci = {};
	dsa_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	dsa_ci.pNext = nullptr;
	dsa_ci.descriptorPool = desc_pool_info->pool;
	// every set shold have its own layout pointer
	dsa_ci.descriptorSetCount = num2alloc;
	dsa_ci.pSetLayouts = &layouts[0];

	assert(num2alloc == 1);
	VkDescriptorSet vk_desc_sets[1]; // a bit explicit here to not forget that multiples can be allocated
	// every set shold have its own layout pointer
	assert(countof(vk_desc_sets) == countof(layouts));

	if (vkAllocateDescriptorSets(dev_.device_, &dsa_ci, &vk_desc_sets[0]) != VK_SUCCESS) {
		log_error("vkAllocateDescriptorSets: Could not allocate descriptor set!\n");
		return nullptr;
	}

	return new RHIDescriptorSetVk(vk_desc_sets[0], dsl);
}

VkWriteDescriptorSet fill_write_desc_set_buffer(VkDescriptorType desc_type, VkDescriptorSet set,
											uint32_t binding, const VkDescriptorBufferInfo* bi, uint32_t count) {
	// would be nice to pass our RHIBufferVk and check if its usage compatible with desc_type
	//RHIBufferVk* rhi_buf = ResourceCast(buf);

	VkWriteDescriptorSet wds = {};
	wds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	wds.pNext = nullptr;
    wds.dstSet = set;
	wds.dstBinding = binding;
	wds.dstArrayElement = 0;
	wds.descriptorCount = count;
	wds.descriptorType = desc_type;
	wds.pImageInfo = nullptr;
    wds.pBufferInfo = bi;
    wds.pTexelBufferView = nullptr;
	// rvo ftw
	return wds;
}

// view and layout can be null depending on a descriptor type
VkWriteDescriptorSet fill_write_desc_set_image(VkDescriptorType desc_type, VkDescriptorSet set,
											   uint32_t binding, const VkDescriptorImageInfo *ii, uint32_t count) {

	VkWriteDescriptorSet wds = {};
	wds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	wds.pNext = nullptr;
	wds.dstSet = set;
	wds.dstBinding = binding;
	wds.dstArrayElement = 0;
	wds.descriptorCount = count;
	wds.descriptorType = desc_type;
	wds.pImageInfo = ii;
	wds.pBufferInfo = nullptr;
	wds.pTexelBufferView = nullptr;
	// rvo ftw
	return wds;
}

// see https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkDescriptorType.html
// for more info
void RHIDeviceVk::UpdateDescriptorSet(const RHIDescriptorWriteDesc *desc, int count) {
	// !NB: Because we remember pointers to array elements we cannot reallocate them, so have to
	// account for a worth case, still those are temp arrays, so should allocate from a preallocated
	// scratch buffer or just use indices and resolve addresses later
	// NOTE: write_desc can be reallocated though
	std::vector<VkWriteDescriptorSet> write_desc(count);
	std::vector<VkDescriptorImageInfo> image_info(count);
	int ii_idx = 0;
	//std::vector<VkBufferView> buffer_view(count); // for future
	std::vector<VkDescriptorBufferInfo > buffer_info(count);
	int bi_idx = 0;
	for (int i = 0; i < count; ++i) {
		VkDescriptorType vk_type = translate_desc_type(desc[i].type);
		VkDescriptorSet vk_set = ResourceCast(desc[i].set)->Handle();
		switch (vk_type) {

		case VK_DESCRIPTOR_TYPE_SAMPLER: {
			assert(desc[i].img.sampler);
			VkSampler sampler = ResourceCast(desc[i].img.sampler)->Handle();
			image_info[ii_idx] = {sampler, VK_NULL_HANDLE, translate_il(RHIImageLayout::kUndefined)};
			write_desc[i] =
				fill_write_desc_set_image(vk_type, vk_set, desc[i].binding, &image_info[ii_idx], 1);
			ii_idx++;
		} break;
		case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
		case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
		case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: {
			assert(desc[i].img.image_view);
			VkImageView view = ResourceCast(desc[i].img.image_view)->Handle();
			image_info[ii_idx] = {VK_NULL_HANDLE, view, translate_il(desc[i].img.image_layout)};
			write_desc[i] =
				fill_write_desc_set_image(vk_type, vk_set, desc[i].binding, &image_info[ii_idx], 1);
			ii_idx++;
		} break;
		case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: {
			assert(desc[i].img.sampler && desc[i].img.image_view);
			VkSampler sampler = ResourceCast(desc[i].img.sampler)->Handle();
			VkImageView view = ResourceCast(desc[i].img.image_view)->Handle();
			image_info[ii_idx] = { sampler, view, translate_il(desc[i].img.image_layout) };
			write_desc[i] =
				fill_write_desc_set_image(vk_type, vk_set, desc[i].binding, &image_info[ii_idx], 1);
			ii_idx++;
		} break;
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: {
			assert(desc[i].buf.buffer);
			VkBuffer buffer = ResourceCast(desc[i].buf.buffer)->Handle();
			buffer_info[bi_idx] = { buffer, desc[i].buf.offset, desc[i].buf.range };
			write_desc[i] = fill_write_desc_set_buffer(vk_type, vk_set, desc[i].binding,
													   &buffer_info[bi_idx], 1);
			bi_idx++;
		} break;
		case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
		case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: {
			// TODO:
		} break;
		}
	}

	assert(ii_idx + bi_idx == count);

	vkUpdateDescriptorSets(dev_.device_, (uint32_t)write_desc.size(), write_desc.data(), 0,
						   nullptr);
}

IRHIGraphicsPipeline *RHIDeviceVk::CreateGraphicsPipeline(
	const RHIShaderStage *shader_stage, uint32_t shader_stage_count,
	const RHIVertexInputState *vertex_input_state,
	const RHIInputAssemblyState *input_assembly_state, const RHIViewportState *viewport_state,
	const RHIRasterizationState *raster_state, const RHIMultisampleState *multisample_state,
	const RHIColorBlendState *color_blend_state, const IRHIPipelineLayout *i_pipleline_layout,
	const IRHIRenderPass *i_render_pass) {

	return RHIGraphicsPipelineVk::Create(this, shader_stage, shader_stage_count, vertex_input_state,
										 input_assembly_state, viewport_state, raster_state,
										 multisample_state, color_blend_state, i_pipleline_layout,
										 i_render_pass);
}

IRHIPipelineLayout* RHIDeviceVk::CreatePipelineLayout(const IRHIDescriptorSetLayout*const* desc_set_layout, uint32_t count) {
    return RHIPipelineLayoutVk::Create(this, desc_set_layout, count);
}

IRHIBuffer *RHIDeviceVk::CreateBuffer(uint32_t size, uint32_t usage, uint32_t memprop_flags,
									  RHISharingMode::Value sharing) {
	return RHIBufferVk::Create(this, size, usage, memprop_flags, sharing);
};

IRHIFence *RHIDeviceVk::CreateFence(bool create_signalled) {
	return RHIFenceVk::Create(this, create_signalled);
};

IRHIEvent *RHIDeviceVk::CreateEvent() {
	return RHIEventVk::Create(this);
};


bool RHIDeviceVk::BeginFrame() {
	cur_frame_++;
    uint32_t frame_res_idx = cur_frame_ % GetNumBufferedFrames();

	if (vkWaitForFences(dev_.device_, 1, &dev_.frame_fence_[frame_res_idx], VK_FALSE, 1000000000) !=
		VK_SUCCESS) {
		log_error("Waiting for fence takes too long!\n");
		return false;
	}
	vkResetFences(dev_.device_, 1, &dev_.frame_fence_[frame_res_idx]);

	VkResult result = vkAcquireNextImageKHR(dev_.device_, dev_.swap_chain_.swap_chain_, UINT64_MAX,
											dev_.img_avail_sem_[frame_res_idx], VK_NULL_HANDLE,
											&cur_swap_chain_img_idx_);
	switch( result ) {
		case VK_SUCCESS:
		case VK_SUBOPTIMAL_KHR:
			break;
		case VK_ERROR_OUT_OF_DATE_KHR:
			log_warning("VK_ERROR_OUT_OF_DATE_KHR, probably need to resize window!\n");
			if (!OnWindowSizeChanged(0,0, false)) {
				return false;
			}
		default:
			log_error("Problem occurred during swap chain image acquisition!\n");
			return false;
	}

	between_begin_frame = true;
	return true;
}

bool RHIDeviceVk::Submit(IRHICmdBuf* cb_in, RHIQueueType::Value queue_type) {

	RHICmdBufVk* cb = ResourceCast(cb_in);
    uint32_t frame_res_idx = cur_frame_ % GetNumBufferedFrames();

	VkPipelineStageFlags wait_dst_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
	VkCommandBuffer cbs[] = { cb->Handle() };
	VkSubmitInfo submit_info = {
		VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 
		1, &dev_.img_avail_sem_[frame_res_idx], 
		&wait_dst_stage_mask, 
		countof(cbs), cbs, 
		1, &dev_.rendering_finished_sem_[frame_res_idx]
	};

	VkQueue queue = RHIQueueType::kGraphics == queue_type ? dev_.graphics_queue_ : dev_.present_queue_;
	if (vkQueueSubmit(queue, 1, &submit_info, dev_.frame_fence_[frame_res_idx]) != VK_SUCCESS) {
		log_error("vkQueueSubmit: failed\n");
		return false;
	}

	return true;
}

bool RHIDeviceVk::Present() {
    uint32_t frame_res_idx = cur_frame_ % GetNumBufferedFrames();

	VkPresentInfoKHR present_info = {
		VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, // VkStructureType              sType
		nullptr,							// const void                  *pNext
		1,									// uint32_t                     waitSemaphoreCount
		&dev_.rendering_finished_sem_[frame_res_idx],		// const VkSemaphore           *pWaitSemaphores
		1,									// uint32_t                     swapchainCount
		&dev_.swap_chain_.swap_chain_,		// const VkSwapchainKHR        *pSwapchains
		&cur_swap_chain_img_idx_,			// const uint32_t              *pImageIndices
		nullptr								// VkResult                    *pResults
	};

	VkResult result = vkQueuePresentKHR(dev_.present_queue_, &present_info);
	switch (result) {
	case VK_SUCCESS:
		break;
	case VK_ERROR_OUT_OF_DATE_KHR:
		log_warning("vkQueuePresentKHR: VK_ERROR_OUT_OF_DATE_KHR\n");
		if (!OnWindowSizeChanged(0,0, false)) {
			return false;
		}
		break;
	case VK_SUBOPTIMAL_KHR:
		log_warning("vkQueuePresentKHR: VK_SUBOPTIMAL_KHR\n");
		if (!OnWindowSizeChanged(0,0, false)) {
			return false;
		}
		break;
	default:
		log_error("vkQueuePresentKHR: Problem occurred during image presentation!\n");
		return false;
	}

	return true;
}

bool RHIDeviceVk::EndFrame() {
	assert(prev_frame_ == cur_frame_ - 1);
	prev_frame_ = cur_frame_;
	between_begin_frame = false;
	return true;
}

IRHIImage* RHIDeviceVk::GetCurrentSwapChainImage() { 
	assert(between_begin_frame);
	return dev_.swap_chain_.images_[cur_swap_chain_img_idx_];
}
RHIFormat RHIDeviceVk::GetSwapChainFormat() {
	return untranslate_f(dev_.swap_chain_.format_);
}
IRHIImageView* RHIDeviceVk::GetSwapChainImageView(uint32_t index) {
	assert(index < GetSwapChainSize());
	return dev_.swap_chain_.views_[index];
}
IRHIImage* RHIDeviceVk::GetSwapChainImage(uint32_t index) {
	assert(index < GetSwapChainSize());
	return dev_.swap_chain_.images_[index];
}

bool RHIDeviceVk::OnWindowSizeChanged(uint32_t width, uint32_t height, bool fullscreen) {

	uint32_t old_w = dev_.swap_chain_data_.capabilities_.currentExtent.width;
	uint32_t old_h = dev_.swap_chain_data_.capabilities_.currentExtent.height;

	log_info("OnWindowSizeChanged(res: %dx%d, fullscreen: %d)\n", width, height, fullscreen);

	SwapChainData new_swapchain_data;
	if (!query_swapchain_data(dev_.phys_device_, dev_.surface_, new_swapchain_data))
	{
		log_error("failed to query_swapchain_data\n");
		return false;
	}

	if (new_swapchain_data.capabilities_.currentExtent.height == 0 ||
		new_swapchain_data.capabilities_.currentExtent.width == 0)
	{
		if (width != 0 && height != 0) {
			log_info("Could not get swapchain res, using provided res: %d %d\n", width, height);
		} else {
			log_info("Could not get swapchain res, and provided res is 0, so using old res: %d %d\n", old_w, old_h);
			width = old_w;
			height = old_h;
		}

		new_swapchain_data.capabilities_.currentExtent = { width, height };
		new_swapchain_data.capabilities_.minImageExtent = { width, height };
		new_swapchain_data.capabilities_.maxImageExtent = { width, height };
	}

	log_info("Creating new swapchain: %d %d\n",
			 new_swapchain_data.capabilities_.currentExtent.width,
			 new_swapchain_data.capabilities_.currentExtent.height);
	SwapChain new_swapchain;
	if (!create_swap_chain(new_swapchain_data, dev_.device_, dev_.surface_,
		dev_.queue_families_, dev_.pallocator_, new_swapchain, dev_.swap_chain_.swap_chain_)) {
		return false;
	}

	// destroy old swap chain
	destroy_swapchain(dev_);

	dev_.swap_chain_data_ = new_swapchain_data;
	dev_.swap_chain_ = new_swapchain;

	if (fp_swap_chain_recreated_) {
		fp_swap_chain_recreated_(user_ptr_);
	}

	return true;
}

