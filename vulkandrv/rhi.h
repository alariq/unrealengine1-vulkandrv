#pragma once

#include "utils/vec.h"
#include <vector> // :-(
#include <stdint.h>
#include <cassert>

class IRHIDevice;
class IRHIImageView;
class IRHIImage;
class IRHIGraphicsPipeline;
class IRHIRenderPass;
class IRHIFrameBuffer;
class IRHIEvent;

typedef uint32_t RHIFlags;

typedef void(*fpOnSwapChainRecreated)(void* user_ptr);

enum class RHIIndexType {
    kUint16 = 0,
    kUint32 = 1,
};

//#if defined(USE_QueueType_TRANSLATION)
struct RHIQueueType { enum Value: uint32_t {
	kUnknown = 0x0,
	kGraphics = 0x1,
	kCompute = 0x2,
	kPresentation = 0x4,
};
};
//#endif

#define USE_Format_TRANSLATION
#if defined(USE_Format_TRANSLATION)
enum class RHIFormat : uint32_t {
    kUNDEFINED = 0,

	kR8G8B8_UNORM,
	kR8G8B8_UINT,
	kR8G8B8_SRGB,

	kR8G8B8A8_UNORM ,
	kR8G8B8A8_UINT,
	kR8G8B8A8_SRGB,

	kR32_UINT,
    kR32_SINT,
	kR32_SFLOAT,

    kR32G32_UINT,
    kR32G32_SINT,
    kR32G32_SFLOAT,

    kR32G32B32_UINT,
    kR32G32B32_SINT,
    kR32G32B32_SFLOAT,

    kR32G32B32A32_UINT,
    kR32G32B32A32_SINT,
    kR32G32B32A32_SFLOAT,

    kB8G8R8A8_UNORM,
    kB8G8R8A8_UINT,
    kB8G8R8A8_SRGB,

    kD32_SFLOAT,
    kD32_SFLOAT_S8_UINT,

};
#else
#define RHIFormat VkFormat
#endif

#define USE_PipelineStageFlags_TRANSLATION
#if defined(USE_PipelineStageFlags_TRANSLATION)
struct RHIPipelineStageFlags { enum Value: uint32_t {
	kTopOfPipe = 0x00000001,
	kDrawIndirect = 0x00000002,
	kVertexInput = 0x00000004,
	kVertexShader = 0x00000008,
	kTessellationControlShader = 0x00000010,
	kTessellationEvaluationShader = 0x00000020,
	kGeometryShader = 0x00000040,
	kFragmentShader = 0x00000080,
	kEarlyFragmentTests = 0x00000100,
	kLateFragmentTests = 0x00000200,
	kColorAttachmentOutput = 0x00000400,
	kComputeShader = 0x00000800,
	kTransfer = 0x00001000,
	kBottomOfPipe = 0x00002000,
	kHost = 0x00004000,
	kAllGraphics = 0x00008000,
	kAllCommands = 0x00010000,
};
};
#else
#define RHIPipelineStageFlags VkPipelineStageFlags 
#endif


#define USE_ACCESS_FLAGS_TRANSLATION
#if defined(USE_ACCESS_FLAGS_TRANSLATION)
struct RHIAccessFlagBits { enum Value: uint32_t {
	kIndirectCommandRead = 0x00000001,
	kIndexRead = 0x00000002,
	kVertexAttributeRead = 0x00000004,
	kUniformRead = 0x00000008,
	kInputAttachmentRead = 0x00000010,
	kShaderRead = 0x00000020,
	kShaderWrite = 0x00000040,
	kColorAttachmentRead = 0x00000080,
	kColorAttachmentWrite = 0x00000100,
	kDepthStencilAttachmentRead = 0x00000200,
	kDepthStencilAttachmentWrite = 0x00000400,
	kTransferRead = 0x00000800,
	kTransferWrite = 0x00001000,
	kHostRead = 0x00002000,
	kHostWrite = 0x00004000,
	kMemoryRead = 0x00008000,
	kMemoryWrite = 0x00010000,
};
};
#else
#define RHIAccessFlagBits VkAccessFlags
#endif

typedef RHIFlags RHIAccessFlags;


#define USE_DependencyFlags_TRANSLATION
#if defined(USE_DependencyFlags_TRANSLATION)
struct RHIDependencyFlags { enum Value : uint32_t {
    kByRegion = 0x00000001,
    kDeviceGroup = 0x00000004,
    kViewLocal = 0x00000002,
};
};
#else
#define RHIDependencyFlags VkDependencyFlags;
#endif

#define USE_ImageAspectFlags_TRANSLATION
#if defined(USE_ImageAspectFlags_TRANSLATION)
struct RHIImageAspectFlags { enum: uint32_t {
	kColor = 0x1,
	kDepth = 0x2,
	kStencil = 0x4
};
};
#else
struct RHIImageAspectFlags { enum: uint32_t {
	kColor = VK_IMAGE_ASPECT_COLOR_BIT,
	kDepth = VK_IMAGE_ASPECT_DEPTH_BIT,
	kStencil = VK_IMAGE_ASPECT_STENCIL_BIT
};
};
#endif
#define USE_ImageLayout_TRANSLATION
#if defined(USE_ImageLayout_TRANSLATION)
struct RHIImageLayout { enum Value: uint32_t {
	kUndefined = 0,
	kGeneral = 1,
	kColorOptimal = 2,
    kDepthStencilOptimal = 3,
    kDepthStencilReadOnlyOptimal = 4,
    kShaderReadOnlyOptimal = 5,
    kTransferSrcOptimal = 6,
    kTransferDstOptimal = 7,
	kPresent = 8,
};
};
#else
#define RHIImageLayout VkImageLayout 
#endif

struct RHIImageTiling { enum Value : uint32_t {
        kOptimal = 0,
        kLinear = 1,
        kDrmFormatModifierExt = 2,
    };
};

struct RHIImageType { enum Value : uint32_t {
        k1D = 0,
        k2D = 1,
        k3D = 2
    };
};

struct RHIImageUsageFlagBits { enum Value : uint32_t {
        TransferSrcBit = 0x01,
        TransferDstBit = 0x02,
        SampledBit = 0x04,
        StorageBit = 0x08,
        ColorAttachmentBit = 0x10,
        DepthStencilAttachmentBit = 0x20,
        TransientAttachmentBit = 0x40,
        InputAttachmentBit = 0x80,
    };
};

typedef RHIFlags RHIImageUsageFlags;

struct RHIFilter { enum Value : uint32_t {
        kNearest = 0,
        kLinear = 1,
        kMaxEnum = 0x7fffffff
    };
};

struct RHISamplerMipmapMode { enum Value : uint32_t {
        kNearest = 0,
        kLinear = 1,
        kMaxEnum = 0x7fffffff
    };
};

struct RHISamplerAddressMode { enum Value: uint32_t {
    kRepeat = 0,
    kMirroredRepeat = 1,
    kClampToEdge = 2,
    kClampToBorder = 3,
    kMirrorClampToEdge = 4,
    kMaxEnum = 0x7fffffff
};
};

struct RHISampleCount { enum Value : uint32_t {
        k1Bit = 0,
        k2Bit = 1,
        k4Bit = 2,
        k8Bit = 3,
        k16Bit = 4,
        k32Bit = 5,
        k64Bit = 6,
    };
};

struct RHIAttachmentStoreOp { enum Value: uint32_t {
		kStore = 0,
		kDoNotCare = 1,
	};
};
struct RHIAttachmentLoadOp { enum Value: uint32_t {
		kLoad = 0,
		kClear = 1,
		kDoNotCare = 2,
	};
};
#define USE_PipelineBindPoint_TRANSLATION
#if defined(USE_PipelineBindPoint_TRANSLATION) 
struct RHIPipelineBindPoint { enum Value: uint32_t {
		kGraphics = 0,
		kCompute = 1,
		kRayTracing = 2,
	};
};
#else
#define RHIPipelineBindPoint VkPipelineBindPoint 
#endif

struct RHIDynamicState { enum Value : uint32_t {
        kViewport = 0,
        kScissor = 1,
        kLineWidth = 2,
        kDepthBias = 3,
        kBlendConstants = 4,
        //...
};};

#define USE_ImageViewType_TRANSLATION
#if defined(USE_ImageViewType_TRANSLATION) 
struct RHIImageViewType { enum Value: uint32_t {
    k1d = 0,
    k2d = 1,
    k3d = 2,
    kCube = 3,
    k1dArray = 4,
    k2dArray = 5,
    kCubeArray = 6,
};
};
#else
#define RHIImageViewType VkImageViewType;
#endif

struct RHIPrimitiveTopology { enum Value: uint32_t {
    kPointList = 0,
    kLineList = 1,
    kLineStrip = 2,
    kTriangleList = 3,
    kTriangleStrip = 4,
    kTriangleFan = 5,
    kLineListWithAdjacency = 6,
    kLineStripWithAdjacency = 7,
    kTriangleListWithAdjacency = 8,
    kTriangleStripWithAdjacency = 9,
    kPatchList = 10,
};
};

struct RHIPolygonMode { enum Value: uint32_t {
    kFill = 0,
    kLine = 1,
    kPoint = 2,
};
};

struct RHICullModeFlags { enum Value: uint32_t {
    kNone = 0,
    kFront = 0x00000001,
    kBack = 0x00000002,
    kFrontAndBack = 0x00000003,
};
};

struct RHIFrontFace { enum Value: uint32_t {
    kCounterClockwise = 0,
    kClockwise = 1
};
};

struct RHICompareOp { enum Value: uint32_t {
	kNever = 0,
	kLess = 1,
	kEqual = 2,
	kLessOrEqual = 3,
	kGreater = 4,
	kNotEqual = 5,
	kGreaterOrEqual = 6,
	kAlways = 7,
};
};

struct RHIStencilOp { enum Value: uint32_t {
    kKeep = 0,
    kZero = 1,
    kReplace = 2,
    kIncrementAndClamp = 3,
    kDecrementAndClamp = 4,
    kInvert = 5,
    kIncrementAndWrap = 6,
    kDecrementAndWrap = 7,
};
};

struct RHILogicOp { enum Value: uint32_t {
    kClear = 0,
    kAnd = 1,
    kAndReverse = 2,
    kCopy = 3,
    kAndInverted = 4,
    kNoOp = 5,
    kXor = 6,
    kOr = 7,
    kNor = 8,
    kEquivalent = 9,
    kInvert = 10,
    kOrReverse = 11,
    kCopyInverted = 12,
    kOrInverted = 13,
    kNand = 14,
    kSet = 15,
};
};

struct RHIBlendFactor { enum Value: uint32_t {
    Zero = 0,
    One = 1,
    SrcColor = 2,
    OneMinusSrcColor = 3,
    DstColor = 4,
    OneMinusDstColor = 5,
    SrcAlpha = 6,
    OneMinusSrcAlpha = 7,
    DstAlpha = 8,
    OneMinusDstAlpha = 9,
};
};

struct RHIBlendOp { enum Value: uint32_t {
    kAdd = 0,
    kSubtract = 1,
    kReverseSubtract = 2,
    kMin = 3,
    kMax = 4,
};
};

struct RHIColorComponentFlags { enum: uint32_t {
    kR = 0x00000001,
    kG = 0x00000002,
    kB = 0x00000004,
    kA = 0x00000008,
};
};

struct RHIBufferUsageFlagBits { enum: uint32_t {
    kTransferSrcBit = 0x00000001,
    kTransferDstBit = 0x00000002,
    kUniformTexelBufferBit = 0x00000004,
    kStorageTexelBufferBit = 0x00000008,
    kUniformBufferBit = 0x00000010,
    kStorageBufferBit = 0x00000020,
    kIndexBufferBit = 0x00000040,
    kVertexBufferBit = 0x00000080
};
};
typedef RHIFlags RHIBufferUsageFlags;

// flags really should not be class
struct RHIMemoryPropertyFlagBits { enum: uint32_t {
    kDeviceLocal = 0x00000001,
    kHostVisible = 0x00000002,
    kHostCoherent = 0x00000004,
    kHostCached = 0x00000008,
    kLazilyAllocated = 0x00000010,
    kProtectedBit = 0x00000020,
    kDeviceCoherentAMD = 0x00000040,
    kDeviceUncachedAMD = 0x00000080,
};
};

typedef RHIFlags RHIMemoryPropertyFlags;

struct RHISharingMode { enum Value: uint32_t {
    kExclusive = 0,
    kConcurrent = 1
};
};

struct RHIDescriptorType {
	enum Value {
		kSampler = 0,
		kCombinedImageSampler = 1,
		kSampledImage = 2,
		kStorageImage = 3,
		kUniformTexelBuffer = 4,
		kStorageTexelBuffer = 5,
		kUniformBuffer = 6,
		kStorageBuffer = 7,
		kUniformBufferDynamic = 8,
		kStorageBufferDynamic = 9,
		kInputAttachment = 10,
		kInlineUniformBlockExt = 11,
		kAccelerationStructureKHR = 12,
        kCount = 13
	};
};

enum : uint32_t {
    kSubpassExternal = ~0U
};


////////////////////////////////////////////////////////////////////////////////
struct RHIImageSubresourceRange {
    uint32_t			aspectMask;
    uint32_t            baseMipLevel;
    uint32_t            levelCount;
    uint32_t            baseArrayLayer;
    uint32_t            layerCount;
};
	
////////////////////////////////////////////////////////////////////////////////
struct RHIImageViewDesc {
	IRHIImage*                    image;
    RHIImageViewType::Value            viewType;
    RHIFormat                       format;
    //RHIComponentMapping         components;
    RHIImageSubresourceRange    subresourceRange;
};

////////////////////////////////////////////////////////////////////////////////
struct RHIImageDesc {
    RHIImageType::Value type;
    RHIFormat format;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t arraySize;
    uint32_t numMips;
    RHISampleCount::Value numSamples;
    RHIImageTiling::Value tiling;
    RHIImageUsageFlags usage;
    RHISharingMode::Value sharingMode;
};

////////////////////////////////////////////////////////////////////////////////
struct RHISamplerDesc {
	RHIFilter::Value magFilter;
	RHIFilter::Value minFilter;
	RHISamplerMipmapMode::Value mipmapMode;
	RHISamplerAddressMode::Value addressModeU;
	RHISamplerAddressMode::Value addressModeV;
	RHISamplerAddressMode::Value addressModeW;
	float mipLodBias;
	bool anisotropyEnable;
	float maxAnisotropy;
	bool compareEnable;
	RHICompareOp::Value compareOp;
	float minLod;
	float maxLod;
	// do not care for now
	// RHIBorderColor           borderColor;
	bool unnormalizedCoordinates;
};


////////////////////////////////////////////////////////////////////////////////
struct RHIFrameBufferDesc {
    uint32_t                    attachmentCount;
    IRHIImageView* const*	    pAttachments;
    uint32_t                    width_;
    uint32_t                    height_;
    uint32_t                    layers_;
	RHIFrameBufferDesc& width(uint32_t w) { width_ = w; return *this; }
	RHIFrameBufferDesc& height(uint32_t h) { height_ = h; return *this; }
	RHIFrameBufferDesc& layers(uint32_t l) { layers_ = l; return *this; }
};

////////////////////////////////////////////////////////////////////////////////
struct RHIShaderStageFlagBits { enum Value: uint32_t {
	kVertex = 0x00000001,
	kTessellationControl = 0x00000002,
	kTessellationEvaluation = 0x00000004,
	kGeometry = 0x00000008,
	kFragment = 0x00000010,
	kCompute = 0x00000020,
};
};
typedef RHIFlags RHIShaderStageFlags;

////////////////////////////////////////////////////////////////////////////////
struct RHIAttachmentRef {
	int index;
	RHIImageLayout::Value layout;
};
struct RHIAttachmentDesc {
    RHIFormat                        format;
    uint32_t						numSamples;
    RHIAttachmentLoadOp::Value              loadOp;
    RHIAttachmentStoreOp::Value             storeOp;
    RHIAttachmentLoadOp::Value              stencilLoadOp;
    RHIAttachmentStoreOp::Value             stencilStoreOp;
    RHIImageLayout::Value                   initialLayout;
    RHIImageLayout::Value                   finalLayout;
};

////////////////////////////////////////////////////////////////////////////////
struct RHISubpassDesc {
	RHIPipelineBindPoint::Value bindPoint;
	int inputAttachmentCount;
	RHIAttachmentRef* inputAttachments;
	int colorAttachmentCount;
	RHIAttachmentRef* colorAttachments;
	RHIAttachmentRef* depthStencilAttachment; // one
	int preserveAttachmentCount;
	uint32_t* preserveAttachments;
};

struct RHISubpassDependency {
    uint32_t         srcSubpass;
    uint32_t         dstSubpass;
    RHIPipelineStageFlags::Value srcStageMask;
    RHIPipelineStageFlags::Value dstStageMask;
    RHIAccessFlagBits::Value   srcAccessMask;
    RHIAccessFlagBits::Value  dstAccessMask;
    uint32_t        dependencyFlags; // RHIDependencyFlags 
};

struct RHIRenderPassDesc {
	int attachmentCount;
	RHIAttachmentDesc* attachmentDesc;
	int subpassCount;
	RHISubpassDesc* subpassDesc;
	int dependencyCount;
	RHISubpassDependency* dependencies;
};
////////////////////////////////////////////////////////////////////////////////
#define USE_VertexInputRate_TRANSLATION
#if defined(USE_VertexInputRate_TRANSLATION)
enum RHIVertexInputRate: uint32_t {
    kVertex = 0,
    kInstance = 1,
};
#else
#define RHIVertexInputRate VkVertexInputRate
#endif

struct RHIVertexInputBindingDesc {
    uint32_t            binding;
    uint32_t            stride;
    RHIVertexInputRate	inputRate;
};

struct RHIVertexInputAttributeDesc {
    uint32_t    location;
    uint32_t    binding;
    RHIFormat   format;
    uint32_t    offset;
};

struct RHIStencilOpState {
	RHIStencilOp::Value failOp;
	RHIStencilOp::Value passOp;
	RHIStencilOp::Value depthFailOp;
	RHICompareOp::Value compareOp;
	uint32_t compareMask;
	uint32_t writeMask;
	uint32_t reference;
};

struct RHIDescriptorSetLayoutDesc {
    RHIDescriptorType::Value type;
    RHIFlags    shader_stage_flags;
    uint32_t    count;
    uint32_t    binding;
};

struct RHIDescriptorWriteDesc {
    RHIDescriptorType::Value type;
    const class IRHIDescriptorSet* set;
    uint32_t binding;
    union {
        struct Image {
            const class IRHISampler* sampler;
            const class IRHIImageView* image_view;
            RHIImageLayout::Value image_layout;
        } img;
        struct Buffer {
            const class IRHIBuffer* buffer;
            uint64_t offset;
            uint64_t range;
        } buf;
        // TODO:
        //struct BufferView {
        //
        //};
    };
};

////////////////////////////////////////////////////////////////////////////////
struct RHIViewport {
	float x;
	float y;
	float width;
	float height;
	float minDepth;
	float maxDepth;
};

struct RHIScissor {
	int32_t x;
	int32_t y;
	int32_t width;
	int32_t height;
};

struct RHIShaderStage {
	RHIShaderStageFlagBits::Value stage;
	class IRHIShader *module;
	const char *pEntryPointName;
};

struct RHIVertexInputState {
	uint32_t vertexBindingDescCount;
	const RHIVertexInputBindingDesc *pVertexBindingDesc;
	uint32_t vertexAttributeDescCount;
	const RHIVertexInputAttributeDesc *pVertexAttributeDesc;
};

struct RHIInputAssemblyState {
    RHIPrimitiveTopology::Value topology;
    bool primitiveRestartEnable;
};

struct RHIViewportState {
    uint32_t                              viewportCount;
    const RHIViewport*                     pViewports;
    uint32_t                              scissorCount;
    const RHIScissor*                       pScissors;
};

struct RHIRasterizationState {
	bool depthClampEnable;
	bool rasterizerDiscardEnable;
	RHIPolygonMode::Value polygonMode;
	RHICullModeFlags::Value cullMode;
	RHIFrontFace::Value frontFace;
	bool depthBiasEnable;
	float depthBiasConstantFactor;
	float depthBiasClamp;
	float depthBiasSlopeFactor;
	float lineWidth;
};

struct RHIMultisampleState {
	uint32_t rasterizationSamples;
	bool sampleShadingEnable;
	float minSampleShading;
	const uint32_t *pSampleMask;
	bool alphaToCoverageEnable;
	bool alphaToOneEnable;
};

struct RHIColorBlendAttachmentState {
    uint32_t				  blendEnable;
    RHIBlendFactor::Value       srcColorBlendFactor;
    RHIBlendFactor::Value      dstColorBlendFactor;
    RHIBlendOp::Value                colorBlendOp;
    RHIBlendFactor::Value            srcAlphaBlendFactor;
    RHIBlendFactor::Value            dstAlphaBlendFactor;
    RHIBlendOp::Value                alphaBlendOp;
    uint32_t                  colorWriteMask; // RHIColorComponentFlags    
};

struct RHIDepthStencilState {
	bool depthTestEnable;
	bool depthWriteEnable;
	RHICompareOp::Value depthCompareOp;
	bool depthBoundsTestEnable;
	bool stencilTestEnable;
	RHIStencilOpState front;
	RHIStencilOpState back;
	float minDepthBounds;
	float maxDepthBounds;
};

struct RHIColorBlendState {
	bool logicOpEnable;
	RHILogicOp::Value logicOp;
	uint32_t attachmentCount;
	const RHIColorBlendAttachmentState *pAttachments;
	float blendConstants[4];
};

struct RHIClearValue {
    vec4 colour;
    float depth;
    uint32_t stencil;
};

////////////////////////////////////////////////////////////////////////////////

class IRHIImage {
protected:
    RHIImageDesc desc_;
public:
    IRHIImage(const RHIImageDesc& desc) : desc_(desc) {}
    const RHIImageDesc& GetDesc() const {
        return desc_;
    }
	uint32_t Width() const { return desc_.width; }
	uint32_t Height() const { return desc_.height; }
	RHIFormat Format() const { return desc_.format; }

	virtual void Destroy(IRHIDevice *device) = 0;
};

class IRHIImageView {
protected:
public:
    virtual ~IRHIImageView() = 0;
    virtual const IRHIImage* GetImage() const = 0;
    virtual IRHIImage* GetImage() = 0;
    virtual void Destroy(IRHIDevice *) = 0;
};

//
//class IRHIQueue {
//public:
//	virtual void Clear(IRHIRenderTarget* rt, const vec4& color) = 0;
//	virtual ~IRHIQueue() = 0 {};
//};

class IRHICmdBuf {
public:

	virtual bool Begin() = 0;
	virtual bool BeginRenderPass(IRHIRenderPass *i_rp, IRHIFrameBuffer *i_fb, const ivec4 *render_area,
					   const RHIClearValue *clear_values, uint32_t count) = 0;
	virtual void BindPipeline(RHIPipelineBindPoint::Value bind_point, IRHIGraphicsPipeline* pipeline) = 0;
	virtual void BindDescriptorSets(RHIPipelineBindPoint::Value bind_point,
									const class IRHIPipelineLayout* pipeline_layout,
									const class IRHIDescriptorSet * const*desc_sets, uint32_t count) = 0;
	virtual void Draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex,
					  uint32_t first_instance) = 0;
	virtual void DrawIndexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index,
							 uint32_t vertex_offset, uint32_t first_instance) = 0;

    virtual void BindVertexBuffers(class IRHIBuffer** i_vb, uint32_t first_binding, uint32_t count) = 0;
    virtual void BindIndexBuffer(class IRHIBuffer* i_ib, uint32_t offset, RHIIndexType type) = 0;

	virtual void CopyBuffer(class IRHIBuffer *dst, uint32_t dst_offset, class IRHIBuffer *src,
							uint32_t src_offset, uint32_t size) = 0;

    virtual void CopyBufferToImage2D(class IRHIImage *i_dst, class IRHIBuffer *i_src) = 0;

    virtual void SetEvent(IRHIEvent* event, RHIPipelineStageFlags::Value stage) = 0;
    virtual void ResetEvent(IRHIEvent* event, RHIPipelineStageFlags::Value stage) = 0;
    //TODO: implement
    //virtual void WaitForEvent(IRHIEvent* event, ...) = 0;

    virtual void SetViewport(const RHIViewport* viewport, uint32_t count) = 0;

	virtual bool End() = 0;
	virtual void EndRenderPass(const IRHIRenderPass *i_rp, IRHIFrameBuffer *i_fb) = 0;

	virtual void BufferBarrier(IRHIBuffer *i_buffer, RHIAccessFlags src_acc_flags,
					   RHIPipelineStageFlags::Value src_stage, RHIAccessFlags dst_acc_fags,
					   RHIPipelineStageFlags::Value dst_stage) = 0;

	virtual void Barrier_ClearToPresent(IRHIImage* image) = 0;
	virtual void Barrier_PresentToClear(IRHIImage* image) = 0;
	virtual void Barrier_PresentToDraw(IRHIImage* image) = 0;
	virtual void Barrier_DrawToPresent(IRHIImage* image) = 0;
	virtual void Barrier_UndefinedToTransfer(IRHIImage *image) = 0;
	virtual void Barrier_TransferToShaderRead(IRHIImage *image) = 0;
    virtual void Barrier_ShaderReadToTransfer(IRHIImage* image) = 0;

    virtual void Clear(IRHIImage *image_in, const vec4 &color, uint32_t img_aspect_bits,
					   IRHIImage *ds_image_in, float depth, uint32_t stencil,
					   uint32_t ds_img_aspect_bits) = 0;
	virtual ~IRHICmdBuf() = 0;
};

class IRHIRenderPass {
public:
	virtual ~IRHIRenderPass() = 0;
};

class IRHIFrameBuffer {
public:
    virtual void Destroy(IRHIDevice* device) = 0;
	virtual ~IRHIFrameBuffer() = 0;
};

class IRHIShader {
public:
	virtual ~IRHIShader() = 0;
};

class IRHISampler {
public:
    virtual ~IRHISampler() = 0;
};

class IRHIDescriptorSetLayout {
protected:
public:
	std::vector<RHIDescriptorSetLayoutDesc> bindings_;
    IRHIDescriptorSetLayout(const RHIDescriptorSetLayoutDesc* desc, int count):bindings_(count) {
	    bindings_.resize(count);
	    for (int i = 0; i < count; ++i) {
		    bindings_[i] = desc[i];
	    }
    };
	virtual ~IRHIDescriptorSetLayout() = 0;
    const RHIDescriptorSetLayoutDesc* getBindings(int& count) const {
        count = (int)bindings_.size();
        return bindings_.data();
    }
};

class IRHIDescriptorSet {
public:
	virtual ~IRHIDescriptorSet() = 0;

	virtual const IRHIDescriptorSetLayout* getLayout() const = 0;
};

class IRHIPipelineLayout {
public:
	virtual ~IRHIPipelineLayout() = 0;
};

class IRHIGraphicsPipeline{
public:
	virtual ~IRHIGraphicsPipeline() = 0; 
    virtual const IRHIPipelineLayout* Layout() const = 0;
};

class IRHIBuffer {
public:
  virtual void Destroy(IRHIDevice *device) = 0;
  virtual void *Map(IRHIDevice *device, uint32_t offset, uint32_t size, uint32_t map_flags) = 0;
  virtual void Unmap(IRHIDevice *device) = 0;
  virtual void Flush(IRHIDevice* device, uint32_t offset, uint32_t size) = 0;
  virtual uint32_t Size() const = 0;

  virtual ~IRHIBuffer() = 0; 
};

class IRHIFence {
    public:
        virtual void Reset(IRHIDevice *device) = 0;
        virtual void Wait(IRHIDevice *device, uint64_t timeout) = 0;
        virtual bool IsSignalled(IRHIDevice *device) = 0;
        virtual void Destroy(IRHIDevice *device) = 0;
        virtual ~IRHIFence() = 0; 
};

class IRHIEvent {
    public:
        virtual void Set(IRHIDevice *device) = 0;
        virtual void Reset(IRHIDevice *device) = 0;
        virtual bool IsSet(IRHIDevice *device) = 0;
        virtual void Destroy(IRHIDevice *device) = 0;
        virtual ~IRHIEvent() = 0; 
};


class IRHIDevice {
public:

	virtual IRHICmdBuf*			CreateCommandBuffer(RHIQueueType::Value queue_type) = 0;
	virtual IRHIRenderPass*		CreateRenderPass(const RHIRenderPassDesc* desc) = 0;
	virtual IRHIFrameBuffer*	CreateFrameBuffer(RHIFrameBufferDesc* desc, const IRHIRenderPass* rp_in) = 0;
	virtual IRHIImage*		    CreateImage(const RHIImageDesc* desc, RHIImageLayout::Value initial_layout, RHIMemoryPropertyFlags mem_prop) = 0;
	virtual IRHIImageView*		CreateImageView(const RHIImageViewDesc* desc) = 0;
    virtual IRHISampler*        CreateSampler(const RHISamplerDesc& desc) = 0;
	virtual IRHIBuffer*		    CreateBuffer(uint32_t size, uint32_t usage, uint32_t memprop, RHISharingMode::Value sharing) = 0;
	virtual IRHIDescriptorSetLayout*    CreateDescriptorSetLayout(const RHIDescriptorSetLayoutDesc* desc, int count) = 0;

    virtual IRHIDescriptorSet* AllocateDescriptorSet(const IRHIDescriptorSetLayout* layout) = 0;
    virtual void UpdateDescriptorSet(const RHIDescriptorWriteDesc* desc, int count) = 0;

    virtual IRHIFence*          CreateFence(bool create_signalled) = 0;
    virtual IRHIEvent*          CreateEvent() = 0;

    virtual IRHIGraphicsPipeline *CreateGraphicsPipeline(
            const RHIShaderStage *shader_stage, uint32_t shader_stage_count,
            const RHIVertexInputState *vertex_input_state,
            const RHIInputAssemblyState *input_assembly_state, const RHIViewportState *viewport_state,
            const RHIRasterizationState *raster_state, const RHIMultisampleState *multisample_state,
			const RHIDepthStencilState* depth_stencil_state,
            const RHIColorBlendState *color_blend_state, const IRHIPipelineLayout *i_pipleline_layout,
            const RHIDynamicState::Value* dynamic_state, const uint32_t dynamic_state_count,
            const IRHIRenderPass *i_render_pass) = 0;

    virtual IRHIPipelineLayout* CreatePipelineLayout(const IRHIDescriptorSetLayout* const* desc_set_layouts, uint32_t count) = 0;
    virtual IRHIShader* CreateShader(RHIShaderStageFlagBits::Value stage, const uint32_t *pdata, uint32_t size) = 0;


	virtual RHIFormat               GetSwapChainFormat() = 0;
	virtual uint32_t				GetSwapChainSize() = 0;
	virtual IRHIImageView*	        GetSwapChainImageView(uint32_t index) = 0;
	virtual class IRHIImage*	    GetSwapChainImage(uint32_t index) = 0;
	virtual IRHIImage*				GetCurrentSwapChainImage() = 0;

    virtual uint32_t GetNumBufferedFrames() = 0;

	virtual bool Submit(IRHICmdBuf* cb, RHIQueueType::Value queue_type) = 0;
	virtual bool BeginFrame() = 0;
	virtual bool Present() = 0;
	virtual bool EndFrame() = 0;
	virtual ~IRHIDevice() {};

	virtual bool OnWindowSizeChanged(uint32_t width, uint32_t height, bool fullscreen) = 0;
    virtual void SetOnSwapChainRecreatedCallback(fpOnSwapChainRecreated callback, void* user_ptr) = 0;
    virtual void WaitIdle() = 0;
};
#if 0
struct RenderContext {
		HWND rw_handle_;
		void* platform_data_;
};
#endif

////////////////////////////////////////////////////////////////////////////////
////////////////// Utils ///////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

struct RHIDescriptorWriteDescBuilder {
	// ehh, remove vector from .h file!
	RHIDescriptorWriteDesc *const desc_;
	const int count_;

	// can go without having cur_index and just decrease count_ and increase desc_ pointer but this
	// is too "smart ass"
	int cur_index;
	RHIDescriptorWriteDescBuilder(RHIDescriptorWriteDesc *desc, int count)
		: desc_(desc), count_(count), cur_index(0) {}

	// can move RHI independent members up to Interfaces  (e.g. getLayout() and implement these in
	// platform independent way because we do not need platform dependent data here, or can just
	// make add() functions virtual and each plarform will implement them as it wishes, not sure
	// what is better for now
	RHIDescriptorWriteDescBuilder &add(const IRHIDescriptorSet *ds, int binding,
									   const IRHISampler *sampler);

	RHIDescriptorWriteDescBuilder &add(const IRHIDescriptorSet *ds, int binding,
									   const IRHISampler *sampler, RHIImageLayout::Value img_layout,
									   const IRHIImageView *img_view);

	RHIDescriptorWriteDescBuilder &add(const IRHIDescriptorSet *ds, int binding,
									   const IRHIImageView *img_view,
									   RHIImageLayout::Value img_layout);

	RHIDescriptorWriteDescBuilder &add(const IRHIDescriptorSet *ds, int binding,
									   const IRHIBuffer *buffer, uint64_t offset, uint64_t range);
};
