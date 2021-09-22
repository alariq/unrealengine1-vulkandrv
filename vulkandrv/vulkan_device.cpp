#include "rhi.h"
#include "vulkan_device.h"
#include "utils/logging.h"
#include "utils/macros.h"
#include <unordered_map>
#include <cassert>
#include <memory>
#include <vector>
#include <vulkan/vulkan_core.h>

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
////////////////////////////////////////////////////////////////////////////////

VkFormat translate(RHIFormat fmt) {
	static VkFormat formats[] = {
        VK_FORMAT_UNDEFINED,
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

VkImageViewType translate(RHIImageViewType type) {
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

VkAttachmentLoadOp translate(RHIAttachmentLoadOp load_op) {
	static VkAttachmentLoadOp ops[] = {
		VK_ATTACHMENT_LOAD_OP_LOAD,
		VK_ATTACHMENT_LOAD_OP_CLEAR,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,
	};
	assert((uint32_t)load_op < countof(ops));
	return ops[(uint32_t)load_op];
}

VkAttachmentStoreOp translate(RHIAttachmentStoreOp store_op) {
	static VkAttachmentStoreOp ops[] = {
		VK_ATTACHMENT_STORE_OP_STORE,
		VK_ATTACHMENT_STORE_OP_DONT_CARE,
	};
	assert((uint32_t)store_op < countof(ops));
	return ops[(uint32_t)store_op];
}

VkImageLayout translate(RHIImageLayout layout) {
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

VkPipelineStageFlags translate(RHIPipelineStageFlags pipeline_stage) {
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

VkAccessFlags translate(RHIAccessFlags access_flags) {
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

VkDependencyFlags translate_dependency_flags(uint32_t dep_flags) {
	uint32_t vk_dep_flags = (dep_flags & (uint32_t)RHIDependencyFlags::kByRegion) ? VK_DEPENDENCY_BY_REGION_BIT : 0;
	vk_dep_flags |= (dep_flags & (uint32_t)RHIDependencyFlags::kDeviceGroup)? VK_DEPENDENCY_DEVICE_GROUP_BIT: 0;
	vk_dep_flags |= (dep_flags & (uint32_t)RHIDependencyFlags::kViewLocal) ? VK_DEPENDENCY_VIEW_LOCAL_BIT: 0;
    return vk_dep_flags;
};

VkShaderStageFlagBits translate(RHIShaderStageFlags stage_flags) {
	switch (stage_flags) {
	case RHIShaderStageFlags::kVertex: return VK_SHADER_STAGE_VERTEX_BIT ;
	case RHIShaderStageFlags::kTessellationControl: return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
	case RHIShaderStageFlags::kTessellationEvaluation: return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT ;
	case RHIShaderStageFlags::kGeometry: return VK_SHADER_STAGE_GEOMETRY_BIT ;
	case RHIShaderStageFlags::kFragment: return VK_SHADER_STAGE_FRAGMENT_BIT ;
	case RHIShaderStageFlags::kCompute: return VK_SHADER_STAGE_COMPUTE_BIT ;
	default:
		assert(0 && "Invalid shader stage flag!");
		return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
	}
};

VkPipelineBindPoint translate(RHIPipelineBindPoint pipeline_bind_point) {
	static VkPipelineBindPoint bind_points[] = {
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		VK_PIPELINE_BIND_POINT_COMPUTE,
		VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR
	};
	assert((uint32_t)pipeline_bind_point < countof(bind_points));
	return bind_points[(uint32_t)pipeline_bind_point];
}

VkImageAspectFlags translate_image_aspect(uint32_t bits) {
	VkImageAspectFlags rv = (bits & (uint32_t)RHIImageAspectFlags::kColor) ? VK_IMAGE_ASPECT_COLOR_BIT: 0;
	rv |= (bits & (uint32_t)RHIImageAspectFlags::kDepth) ? VK_IMAGE_ASPECT_DEPTH_BIT : 0;
	rv |= (bits & (uint32_t)RHIImageAspectFlags::kStencil) ? VK_IMAGE_ASPECT_STENCIL_BIT: 0;
	return rv;
}

VkVertexInputRate translate(RHIVertexInputRate input_rate) {
	return input_rate == RHIVertexInputRate::kVertex ? VK_VERTEX_INPUT_RATE_VERTEX
													 : VK_VERTEX_INPUT_RATE_INSTANCE;
}
VkPrimitiveTopology translate(RHIPrimitiveTopology prim_topology) {
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

VkPolygonMode translate(RHIPolygonMode polygon_mode) {
	switch (polygon_mode) {
	case RHIPolygonMode::kFill : return VK_POLYGON_MODE_FILL;
	case RHIPolygonMode::kLine: return VK_POLYGON_MODE_LINE;
	case RHIPolygonMode::kPoint: return VK_POLYGON_MODE_POINT;
	default:
		assert(0 && "Invalid polygon mode!");
		return VK_POLYGON_MODE_MAX_ENUM;
	}
};
  
VkCullModeFlags translate(RHICullModeFlags cull_mode) {
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

VkFrontFace translate(RHIFrontFace front_face) {
	return front_face == RHIFrontFace::kClockwise ? VK_FRONT_FACE_CLOCKWISE
												  : VK_FRONT_FACE_COUNTER_CLOCKWISE;
};

VkBlendFactor translate(RHIBlendFactor blend_factor) {
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

VkBlendOp translate(RHIBlendOp blend_op) {
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

VkColorComponentFlags translate_color_comp(uint32_t col_cmp_flags) {
    VkColorComponentFlags flags;
    flags  = (col_cmp_flags & (uint32_t)RHIColorComponentFlags::kR) ? VK_COLOR_COMPONENT_R_BIT : 0;
    flags |= (col_cmp_flags & (uint32_t)RHIColorComponentFlags::kG) ? VK_COLOR_COMPONENT_G_BIT : 0;
    flags |= (col_cmp_flags & (uint32_t)RHIColorComponentFlags::kB) ? VK_COLOR_COMPONENT_B_BIT : 0;
    flags |= (col_cmp_flags & (uint32_t)RHIColorComponentFlags::kA) ? VK_COLOR_COMPONENT_A_BIT : 0;
    return flags;
}

VkLogicOp translate(RHILogicOp logic_op) {
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

VkSharingMode translate(RHISharingMode sharing_mode) {
    return sharing_mode == RHISharingMode::kConcurrent ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
}

VkMemoryPropertyFlags translate_mem_prop(uint32_t memprop) {
    VkMemoryPropertyFlags vk_flags = (memprop & RHIMemoryPropertyFlags::kDeviceLocal) ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT : 0;
    vk_flags |= (memprop & RHIMemoryPropertyFlags::kHostVisible) ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT: 0;
    vk_flags |= (memprop & RHIMemoryPropertyFlags::kHostCoherent) ? VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : 0;
    vk_flags |= (memprop & RHIMemoryPropertyFlags::kHostCached) ? VK_MEMORY_PROPERTY_HOST_CACHED_BIT : 0;
    vk_flags |= (memprop & RHIMemoryPropertyFlags::kLazilyAllocated) ? VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT: 0;
    vk_flags |= (memprop & RHIMemoryPropertyFlags::kProtectedBit) ? VK_MEMORY_PROPERTY_PROTECTED_BIT : 0;
    vk_flags |= (memprop & RHIMemoryPropertyFlags::kDeviceCoherentAMD) ? VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD : 0;
    vk_flags |= (memprop & RHIMemoryPropertyFlags::kDeviceUncachedAMD) ? VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD : 0;
    return vk_flags;
}

////////////// Image //////////////////////////////////////////////////

void RHIImageVk::Destroy(IRHIDevice* device) {
	RHIDeviceVk* dev = ResourceCast(device);
	vkDestroyImage(dev->Handle(), handle_, dev->Allocator());
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

////////////// Frame Buffer //////////////////////////////////////////////////

void RHIFrameBufferVk::Destroy(IRHIDevice* device) {
	RHIDeviceVk* dev = ResourceCast(device);
	vkDestroyFramebuffer(dev->Handle(), handle_, dev->Allocator());
	delete this;
}

////////////// Render Pass //////////////////////////////////////////////////

void RHIRenderPassVk::Destroy(IRHIDevice* device) {
	RHIDeviceVk* dev = ResourceCast(device);
	vkDestroyRenderPass(dev->Handle(), handle_, dev->Allocator());
	delete this;
}

////////////// Shader //////////////////////////////////////////////////

void RHIShaderVk::Destroy(IRHIDevice* device) {
	RHIDeviceVk* dev = ResourceCast(device);
	vkDestroyShaderModule(dev->Handle(), shader_module_, dev->Allocator());
	delete this;
}

RHIShaderVk *RHIShaderVk::Create(IRHIDevice *device, const uint32_t *pdata, uint32_t size,
								 RHIShaderStageFlags stage) {
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
	shader->vk_stage_ = translate(stage);
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
												 IRHIDescriptorSetLayout *desc_set_layout) {
	VkPipelineLayoutCreateInfo ci = {
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, 
      nullptr,                                        
      0,                                              
      0,                                              // uint32_t                       setLayoutCount
      nullptr,                                        // const VkDescriptorSetLayout   *pSetLayouts
      0,                                              // uint32_t                       pushConstantRangeCount
      nullptr                                         // const VkPushConstantRange     *pPushConstantRanges
    };

	VkPipelineLayout pipeline_layout;
	RHIDeviceVk *dev = ResourceCast(device);
	if (vkCreatePipelineLayout(dev->Handle(), &ci, dev->Allocator(), &pipeline_layout) !=
		VK_SUCCESS) {
		log_error("Could not create pipeline layout!");
		return nullptr;
	}

	RHIPipelineLayoutVk *pl = new RHIPipelineLayoutVk();
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
			translate(shader_stage[i].stage),
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
		vertex_input_bindings[i].inputRate = translate(vertex_input_state->pVertexBindingDesc[i].inputRate);
	}

	std::vector<VkVertexInputAttributeDescription> vertex_input_attributes(
		vertex_input_state->vertexAttributeDescCount);
	for (uint32_t i = 0; i < (uint32_t)vertex_input_attributes.size(); ++i) {
		vertex_input_attributes[i].location = vertex_input_state->pVertexAttributeDesc[i].location;
		vertex_input_attributes[i].binding = vertex_input_state->pVertexAttributeDesc[i].binding;
		vertex_input_attributes[i].format = translate(vertex_input_state->pVertexAttributeDesc[i].format);
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
      translate(input_assembly_state->topology),
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
		translate(raster_state->polygonMode),
		translate(raster_state->cullMode),
		translate(raster_state->frontFace),
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
		state.srcColorBlendFactor = translate(cb_state->srcColorBlendFactor);
		state.dstColorBlendFactor = translate(cb_state->dstColorBlendFactor);
		state.colorBlendOp = translate(cb_state->colorBlendOp);
		state.srcAlphaBlendFactor = translate(cb_state->srcAlphaBlendFactor);
		state.dstAlphaBlendFactor = translate(cb_state->dstAlphaBlendFactor);
		state.alphaBlendOp = translate(cb_state->alphaBlendOp);
		state.colorWriteMask = translate_color_comp(cb_state->colorWriteMask);

		arr_color_blend_attachment_state.push_back(state);
	}

	VkPipelineColorBlendStateCreateInfo color_blend_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType sType
		nullptr,						  // const void                                    *pNext
		0,								  // VkPipelineColorBlendStateCreateFlags           flags
		color_blend_state->logicOpEnable, // VkBool32 logicOpEnable
		translate(color_blend_state->logicOp),			   // VkLogicOp logicOp
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
      playout ? playout->Handle() : nullptr,                        // VkPipelineLayout                               layout
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

	RHIGraphicsPipelineVk* vk_pipeline = new RHIGraphicsPipelineVk();
	vk_pipeline->handle_ = pipeline;
	return vk_pipeline;
}


////////////// Buffer //////////////////////////////////////////////////
RHIBufferVk* RHIBufferVk::Create(IRHIDevice* device, uint32_t size, uint32_t usage, uint32_t memprop, RHISharingMode sharing) {
    VkBufferCreateInfo buffer_create_info = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,             // VkStructureType        sType
        nullptr,                                          // const void            *pNext
        0,                                                // VkBufferCreateFlags    flags
        size,                                            // VkDeviceSize           size
        translate_buffer_usage(usage),                // VkBufferUsageFlags     usage
        translate(sharing),                        // VkSharingMode          sharingMode
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
      mapped_size_                                      // VkDeviceSize           size
    };
	vkFlushMappedMemoryRanges(dev->Handle(), 1, &flush_range);

	vkUnmapMemory(dev->Handle(), backing_mem_);
    is_mapped_ = false;
}

////////////////////////////////////////////////////////////////////////
IRHIShader* RHIDeviceVk::CreateShader(RHIShaderStageFlags stage, const uint32_t *pdata, uint32_t size) {
    return RHIShaderVk::Create(this, pdata, size, stage);
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

void Barrier(RHICmdBufVk *cb, RHIImageVk* image,
			 VkPipelineStageFlags src_pipeline_stage_bits,
			 VkPipelineStageFlags dst_pipeline_stage_bits, VkAccessFlags new_access_flags,
			 VkImageLayout new_layout) {

	// TODO: incorporate this int oparameters, or use image view somehow and gfrab it from that
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

	vkCmdPipelineBarrier(cb->Handle(), src_pipeline_stage_bits, dst_pipeline_stage_bits, 0, 0,
						 nullptr, 0, nullptr, 1, &barrier_copy2present);

	// warning: updating states like this is not always correct if we have differect CBs and submit
	// them in different order
	image->vk_access_flags_ = new_access_flags;
	image->vk_layout_ = new_layout;

}

void RHICmdBufVk::BufferBarrier(IRHIBuffer *i_buffer, RHIAccessFlags src_acc_flags,
								RHIPipelineStageFlags src_stage, RHIAccessFlags dst_acc_fags,
								RHIPipelineStageFlags dst_stage) {
	RHIBufferVk* buffer = ResourceCast(i_buffer);

    VkBufferMemoryBarrier buffer_memory_barrier = {
      VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,          // VkStructureType                        sType
      nullptr,                                          // const void                            *pNext
      translate(src_acc_flags),                       // VkAccessFlags                          srcAccessMask
      translate(dst_acc_fags),              // VkAccessFlags                          dstAccessMask
      VK_QUEUE_FAMILY_IGNORED,                          // uint32_t                               srcQueueFamilyIndex
      VK_QUEUE_FAMILY_IGNORED,                          // uint32_t                               dstQueueFamilyIndex
      buffer->Handle(),                       // VkBuffer                               buffer
      0,                                                // VkDeviceSize                           offset
      VK_WHOLE_SIZE                                     // VkDeviceSize                           size
    };
	vkCmdPipelineBarrier(cb_, translate(src_stage), translate(dst_stage), 0, 0, nullptr, 1,
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
    auto& attachments = fb->GetAttachments();
    uint32_t i=0;
    for(auto att : attachments) {
        RHIImageVk* img = ResourceCast(att->GetImage());
        img->vk_layout_ = translate(rp->GetFinalLayout(i));
        ++i;
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

void RHICmdBufVk::BindPipeline(RHIPipelineBindPoint bind_point, IRHIGraphicsPipeline* i_pipeline) {
    assert(is_recording_);
    const RHIGraphicsPipelineVk* pipeline = ResourceCast(i_pipeline);

    vkCmdBindPipeline(cb_, translate(bind_point), pipeline->Handle());
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

    VkBufferCopy buffer_copy_info = {
      .srcOffset = src_offset,                                   // VkDeviceSize                           srcOffset
      .dstOffset = dst_offset,                                                // VkDeviceSize                           dstOffset
      .size = size                      // VkDeviceSize                           size
    };

    const RHIBufferVk* dst = ResourceCast(i_dst);
    const RHIBufferVk* src = ResourceCast(i_src);
    assert(dst->Size() >= dst_offset + size);
    assert(src->Size() >= src_offset + size);

    vkCmdCopyBuffer(cb_, src->Handle(), dst->Handle(), 1, &buffer_copy_info );
}

void RHICmdBufVk::SetEvent(IRHIEvent* i_event, RHIPipelineStageFlags stage) {
    const RHIEventVk* event = ResourceCast(i_event); 
    vkCmdSetEvent(cb_, event->Handle(), translate(stage));
}
void RHICmdBufVk::ResetEvent(IRHIEvent* i_event, RHIPipelineStageFlags stage) {
    const RHIEventVk* event = ResourceCast(i_event); 
    vkCmdResetEvent(cb_, event->Handle(), translate(stage));
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
	Barrier(this, image, VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_ACCESS_MEMORY_READ_BIT,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}
void RHICmdBufVk::Barrier_PresentToClear(IRHIImage *image_in) {
	RHIImageVk* image = ResourceCast(image_in);
	Barrier(this, image, VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
}

void RHICmdBufVk::Barrier_PresentToDraw(IRHIImage *image_in) {
	RHIImageVk* image = ResourceCast(image_in);
	Barrier(this, image, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
}

void RHICmdBufVk::Barrier_DrawToPresent(IRHIImage *image_in) {
	RHIImageVk* image = ResourceCast(image_in);
	Barrier(this, image, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_ACCESS_MEMORY_READ_BIT,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}

////////////////RHI Device /////////////////////////////////////////////////////

RHIDeviceVk::~RHIDeviceVk() {
}


// should this be mobved to command buffer class / .cpp file and just pass Device as a parameter ?
IRHICmdBuf* RHIDeviceVk::CreateCommandBuffer(RHIQueueType queue_type) {

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
	std::vector<VkAttachmentReference> input_ref_arr;
	std::vector<VkAttachmentReference> color_ref_arr;
	std::vector<VkAttachmentReference> depth_stencil_ref_arr;
	std::vector<VkSubpassDependency> dep_arr(desc->dependencyCount);

	std::vector<RHIImageLayout> att_final_layouts(desc->attachmentCount);
	RHIAttachmentDesc* att_desc = desc->attachmentDesc;
	for (int i = 0; i < desc->attachmentCount; ++i) {
		att_desc_arr[i].flags = 0;
		att_desc_arr[i].format = translate(att_desc[i].format);
		att_desc_arr[i].samples = translate_num_samples(att_desc[i].numSamples);
		att_desc_arr[i].loadOp = translate(att_desc[i].loadOp);
		att_desc_arr[i].storeOp = translate(att_desc[i].storeOp);
		att_desc_arr[i].stencilLoadOp = translate(att_desc[i].stencilLoadOp);
		att_desc_arr[i].stencilStoreOp = translate(att_desc[i].stencilStoreOp);
		att_desc_arr[i].initialLayout = translate(att_desc[i].initialLayout);
		att_desc_arr[i].finalLayout = translate(att_desc[i].finalLayout);

        att_final_layouts[i] = att_desc[i].finalLayout;
	}

	for (int i = 0; i < desc->subpassCount; ++i) {
		const RHISubpassDesc& sp_desc = desc->subpassDesc[i];
		subpass_desc_arr[i].flags = 0;
		subpass_desc_arr[i].pipelineBindPoint = translate(sp_desc.bindPoint);
		subpass_desc_arr[i].inputAttachmentCount = sp_desc.inputAttachmentCount;
		subpass_desc_arr[i].colorAttachmentCount = sp_desc.colorAttachmentCount;
		subpass_desc_arr[i].preserveAttachmentCount = sp_desc.preserveAttachmentCount;

		for (int ia = 0; ia < sp_desc.inputAttachmentCount; ++ia) {
			VkAttachmentReference ar;
			ar.attachment = sp_desc.inputAttachments[ia].index;
			ar.layout = translate(sp_desc.inputAttachments[ia].layout);
			input_ref_arr.push_back(ar);
		}
		subpass_desc_arr[i].pInputAttachments = input_ref_arr.data();

		for (int ca = 0; ca < sp_desc.colorAttachmentCount; ++ca) {
			VkAttachmentReference ar;
			ar.attachment = sp_desc.colorAttachments[ca].index;
			ar.layout = translate(sp_desc.colorAttachments[ca].layout);
			color_ref_arr.push_back(ar);
		}
		subpass_desc_arr[i].pColorAttachments = color_ref_arr.data();

		for (int dsa = 0; dsa < sp_desc.depthStencilAttachmentCount; ++dsa) {
			VkAttachmentReference ar;
			ar.attachment = sp_desc.depthStencilAttachments[dsa].index;
			ar.layout = translate(sp_desc.depthStencilAttachments[dsa].layout);
			depth_stencil_ref_arr.push_back(ar);
		}
		subpass_desc_arr[i].pDepthStencilAttachment = depth_stencil_ref_arr.data();
		subpass_desc_arr[i].pPreserveAttachments = subpass_desc_arr[i].pPreserveAttachments;
	}

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

IRHIImageView* RHIDeviceVk::CreateImageView(const RHIImageViewDesc* desc) {
	assert(desc);
	RHIImageVk* image = ResourceCast(desc->image);

	VkImageViewCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	ci.image = image->Handle();
	ci.viewType = translate(desc->viewType);
	ci.format = translate(desc->format);
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

IRHIPipelineLayout* RHIDeviceVk::CreatePipelineLayout(IRHIDescriptorSetLayout* desc_set_layout) {
    return RHIPipelineLayoutVk::Create(this, desc_set_layout);
}

IRHIBuffer *RHIDeviceVk::CreateBuffer(uint32_t size, uint32_t usage, uint32_t memprop_flags,
									  RHISharingMode sharing) {
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
			return false;
		default:
			log_error("Problem occurred during swap chain image acquisition!\n");
			return false;
	}

	between_begin_frame = true;
	return true;
}

bool RHIDeviceVk::Submit(IRHICmdBuf* cb_in, RHIQueueType queue_type) {

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
	case VK_SUBOPTIMAL_KHR:
		log_error("vkQueuePresentKHR: VK_SUBOPTIMAL_KHR\n");
		return false; 
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
	return untranslate(dev_.swap_chain_.format_);
}
IRHIImageView* RHIDeviceVk::GetSwapChainImageView(uint32_t index) {
	assert(index < GetSwapChainSize());
	return dev_.swap_chain_.views_[index];
}
IRHIImage* RHIDeviceVk::GetSwapChainImage(uint32_t index) {
	assert(index < GetSwapChainSize());
	return dev_.swap_chain_.images_[index];
}
