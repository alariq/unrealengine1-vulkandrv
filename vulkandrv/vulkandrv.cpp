#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define USE_VULKAN_IMPLEMENTATION
#include "vulkan_common.h"

#include <stdio.h>
#include <io.h>
#include <FCNTL.H>
#include <stdlib.h>     // for _itow 
#include <new>
#include <vector>
#include <unordered_map>

//#include "resource.h"
#include "vulkandrv.h"
#include "utils/logging.h"
#include "utils/macros.h"
#include "utils/file_utils.h"
#include "utils/Image.h"
#include "utils/mesh_utils.h"
//#include "texconverter.h"
//#include "customflags.h"
#include "misc.h"
#include "texture_cache.h"
//#include "vertexformats.h"
//#include "shader_gouraudpolygon.h"
//#include "shader_tile.h"
//#include "shader_complexsurface.h"
//#include "shader_fogsurface.h"

#include "vulkan_rhi.h"
bool vulkan_initialize(HWND rw_handle);

//UObject glue
IMPLEMENT_PACKAGE(VulkanDrv);
IMPLEMENT_CLASS(UVulkanRenderDevice);

/**< RUNE:  This poly should be alpha blended. NOTE: as this uses the game-specific flag number, only check this for Rune */
// This flag has same value as PF_BigWavy, so watch out for it and maybe interpret it only in case or Rune
#define PF_AlphaBlend 0x00001000

static bool drawingWeapon = false; /** Whether the depth buffer was cleared and projection parameters set to draw the weapon model */
static bool drawingHUD = false;
static int customFOV = 0; /**Field of view calculated from aspect ratio */
static bool firstFrameOfLevel = false;
/** See SetSceneNode() */
static float zNear = 0.5f; //Default for the games is 1, but results in cut-off UT weapons with widescreen FOVs
static float zFar = 1.0f;
static LARGE_INTEGER perfCounterFreq;
//static TextureCache *textureCache;
//static TexConverter *texConverter;
//static Shader_GouraudPolygon *shader_GouraudPolygon;
//static Shader_Tile *shader_Tile;
//static Shader_ComplexSurface *shader_ComplexSurface;
//static Shader_FogSurface *shader_FogSurface;

static inline DWORD bgra_make(BYTE b, BYTE g, BYTE r, BYTE a) {
	return (a << 24) | (r << 16) | (g << 8) | b;
}
static inline DWORD FPlaneTo_BGRScaled_A255(const FPlane *pPlane, FLOAT rgbScale) {
	return bgra_make(appRound(pPlane->Z * rgbScale), appRound(pPlane->Y * rgbScale),
					 appRound(pPlane->X * rgbScale), 255);
}
static inline DWORD FPlaneTo_BGR_A0(const FPlane *pPlane) {
	return bgra_make(appRound(pPlane->Z * 255.0f), appRound(pPlane->Y * 255.0f),
					 appRound(pPlane->X * 255.0f), 0);
}
static inline DWORD FPlaneTo_BGRAClamped(const FPlane *pPlane) {
	return bgra_make(
		Clamp(appRound(pPlane->Z * 255.0f), 0, 255), Clamp(appRound(pPlane->Y * 255.0f), 0, 255),
		Clamp(appRound(pPlane->X * 255.0f), 0, 255), Clamp(appRound(pPlane->W * 255.0f), 0, 255));
}
static inline DWORD FPlaneTo_BGRClamped_A255(const FPlane *pPlane) {
	return bgra_make(Clamp(appRound(pPlane->Z * 255.0f), 0, 255),
					 Clamp(appRound(pPlane->Y * 255.0f), 0, 255),
					 Clamp(appRound(pPlane->X * 255.0f), 0, 255), 255);
}

template <typename T> // all memebers should be templated?
struct BufferView {
	typedef typename T El_t;
	uint8_t *data_;
	uint32_t el_size_; // aka "element stride"
	int size_;

	const El_t &operator[](int i) const {
		assert(i >= 0 && i < size_);
		return *(El_t *)(data_ + el_size_ * i);
	}
	El_t &operator[](int i) {
		assert(i >= 0 && i < size_);
		return *(El_t *)(data_ + el_size_ * i);
	}

	BufferView(void *data, uint32_t el_size, int el_count)
		: data_((uint8_t*)data), el_size_(el_size), size_(el_count) {}
};

static class TextureCache* g_texCache;

struct SimpleVertex {
    vec4 pos;
    vec4 color;
    vec2 uv;
};
struct UEVertexComplex {
	vec3 Pos;
	vec2 TexCoord;
	//uint32_t Flags;
};

struct UEVertexGouraud {
	vec3 Pos;
	vec4 TexCoord;
	DWORD Color;
	DWORD FogColor;
};

struct PerFrameUniforms {
	vec4 stuff;
	mat4 fake_camera;
	mat4 world;
	mat4 view;
	mat4 proj;
	mat4 normal_tr;
	mat4 vwp;
};

struct UEPerDrawCallUniformBuf {
	mat4 world;
};

struct UEPerDrawCallComplexVsData {
	vec4 XAxis_UDot;
	vec4 YAxis_VDot;
	vec4 Diffuse_PanXY_UVMult;
	vec4 Macro_PanXY_UVMult;
	vec4 HasMacro_UVScale;
	vec4 Lightmap_PanXY_UVMult;
	vec4 HasLightmap_UVScale;
	vec4 Detail_PanXY_UVMult;
	vec4 HasDetail_UVScale;
	mat4 proj;
};

struct UEPerDrawCallGouraudVsData {
	mat4 proj; // yes as it can be changed during the frame draw
	// other stuff?
};

struct UEPerFrameUniformBuf {
	mat4 proj;
	vec4 DetailColor;
};

mat4 g_current_projection = mat4::identity();
RHIViewport g_current_viewport;

RHIVertexInputBindingDesc vert_bindings_desc[] = {
	{0, sizeof(SimpleVertex), RHIVertexInputRate::kVertex}};

RHIVertexInputBindingDesc svd_vert_bindings_desc[] = {
	{0, sizeof(SVD), RHIVertexInputRate::kVertex}};

RHIVertexInputBindingDesc ue_complex_vert_bindings_desc[] = {
	{0, sizeof(UEVertexComplex), RHIVertexInputRate::kVertex}};

RHIVertexInputBindingDesc ue_gouraud_vert_bindings_desc[] = {
	{0, sizeof(UEVertexGouraud), RHIVertexInputRate::kVertex}};

RHIVertexInputAttributeDesc va_desc[] = {
	{0, vert_bindings_desc[0].binding, RHIFormat::kR32G32B32A32_SFLOAT,
	 offsetof(SimpleVertex, pos)},
	{1, vert_bindings_desc[0].binding, RHIFormat::kR32G32B32A32_SFLOAT,
	 offsetof(SimpleVertex, color)},
	{2, vert_bindings_desc[0].binding, RHIFormat::kR32G32_SFLOAT,
	 offsetof(SimpleVertex, uv)}};

RHIVertexInputAttributeDesc world_model_va_desc[] = {
	{0, svd_vert_bindings_desc[0].binding, RHIFormat::kR32G32B32_SFLOAT,
	 offsetof(SVD, pos)},
	{1, svd_vert_bindings_desc[0].binding, RHIFormat::kR32G32B32_SFLOAT,
	 offsetof(SVD, normal)},
	{2, svd_vert_bindings_desc[0].binding, RHIFormat::kR32G32_SFLOAT,
	 offsetof(SVD, uv)}};

RHIVertexInputAttributeDesc ue_complex_va_desc[] = {
	{0, ue_complex_vert_bindings_desc[0].binding, RHIFormat::kR32G32B32_SFLOAT,
	 offsetof(UEVertexComplex, Pos)},
	{1, ue_complex_vert_bindings_desc[0].binding, RHIFormat::kR32G32_SFLOAT,
	 offsetof(UEVertexComplex, TexCoord)}};

RHIVertexInputAttributeDesc ue_gouraud_va_desc[] = {
	{0, ue_gouraud_vert_bindings_desc[0].binding, RHIFormat::kR32G32B32_SFLOAT,
	 offsetof(UEVertexGouraud, Pos)},
	{1, ue_gouraud_vert_bindings_desc[0].binding, RHIFormat::kR32G32B32A32_SFLOAT,
	 offsetof(UEVertexGouraud, TexCoord)},
	{2, ue_gouraud_vert_bindings_desc[0].binding, RHIFormat::kB8G8R8A8_UNORM,
	 offsetof(UEVertexGouraud, Color)},
	{3, ue_gouraud_vert_bindings_desc[0].binding, RHIFormat::kB8G8R8A8_UNORM,
	 offsetof(UEVertexGouraud, FogColor)}};

// Create Test Vertex Buffer
SimpleVertex vb[] = {{{-0.55f, -0.55f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
					 {{-0.55f, 0.55f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
					 {{0.55f, -0.55f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
					 {{0.55f, 0.55f, 0.0f, 1.0f}, {0.45f, 0.45f, 0.45f, 0.0f}, {1.0f, 1.0f}}};

static int g_curFBIdx = 0;
static IRHIDevice* g_vulkan_device = 0;
static IRHICmdBuf* g_cmdbuf[kNumBufferedFrames] = { 0 };

IRHIBuffer* g_quad_gpu_vb = nullptr;
IRHIBuffer* g_quad_staging_vb = nullptr;
IRHIEvent* g_quad_vb_copy_event= nullptr;

struct SBuffer {
	enum BufType_t { kUnknown = 0, kIB = 1, kVB = 2, kUni = 3 };

	IRHIBuffer* device_buf_ = nullptr;
	IRHIBuffer* staging_buf_ = nullptr;
	IRHIEvent* copy_event_ = nullptr;
	BufType_t type_ = kUnknown;
	uint32_t size_ = 0;
	void* mapped_buf_= 0;

	void* getMappedPtr() const { return mapped_buf_; }

	static SBuffer* makeIB(IRHIDevice* dev, uint32_t size, const void* data) {
		SBuffer* b = make(dev, size, RHIBufferUsageFlagBits::kIndexBufferBit, data);
		b->type_ = kIB;
		return b;
	}

	static SBuffer* makeVB(IRHIDevice* dev, uint32_t size, const void* data) {
		SBuffer* b = make(dev, size, RHIBufferUsageFlagBits::kVertexBufferBit, data);
		b->type_ = kVB;
		return b;
	}

	static SBuffer* makeUB(IRHIDevice* dev, uint32_t size, const void* data) {
		SBuffer* b = make(dev, size, RHIBufferUsageFlagBits::kUniformBufferBit, data);
		b->type_ = kUni;
		return b;
	}

	static SBuffer* make(IRHIDevice* dev, uint32_t size, uint32_t usage, const void* data) {
		SBuffer* buf = new SBuffer();
		buf->size_ = size;

		buf->device_buf_ = dev->CreateBuffer(
			size, RHIBufferUsageFlagBits::kTransferDstBit | usage,
			RHIMemoryPropertyFlagBits::kDeviceLocal, RHISharingMode::kExclusive);
		assert(buf->device_buf_);

		buf->staging_buf_ = dev->CreateBuffer(size, RHIBufferUsageFlagBits::kTransferSrcBit,
							 RHIMemoryPropertyFlagBits::kHostVisible, RHISharingMode::kExclusive);
		assert(buf->staging_buf_);

		buf->mapped_buf_ = buf->staging_buf_->Map(dev, 0, size, 0);
		
		if (data) {
			buf->CopyToStaging(dev, 0, size, data);
		}

		buf->copy_event_ = dev->CreateEvent();

		return buf;
	}
	
	void CopyToStaging(IRHIDevice* dev, uint32_t offset, uint32_t size, const void* data) {
		assert(data);
		uint64_t size64 = (uint64_t)offset + (uint64_t)size;
		assert(size64 <= (uint64_t)size_);
		memcpy((uint8_t*)getMappedPtr() + offset, data, size);
	}

	void CopyToGPU(IRHIDevice* dev, IRHICmdBuf* cb) {
		//if (!copy_event_->IsSet(dev)) {
			cb->CopyBuffer(device_buf_, 0, staging_buf_, 0, staging_buf_->Size());
			RHIAccessFlags dst_acc_flags;
			RHIPipelineStageFlags::Value dst_pipe_stage;
			switch (type_) {
			case kIB: 
				dst_acc_flags = RHIAccessFlagBits::kIndexRead;
				dst_pipe_stage = RHIPipelineStageFlags::kVertexInput;
				break;
			case kVB: 
				dst_acc_flags = RHIAccessFlagBits::kVertexAttributeRead;
				dst_pipe_stage = RHIPipelineStageFlags::kVertexInput;
				break;
			case kUni: 
				dst_acc_flags = RHIAccessFlagBits::kShaderRead;
				// TODO: may pass exact flags in case of Uniform Buffer for now select earliest one
				dst_pipe_stage = RHIPipelineStageFlags::kVertexShader;
				break;
			default:
				assert(!"Wrong buffer type, TODO: add case for uniform buffer");
			};
			cb->BufferBarrier(device_buf_, (uint32_t)RHIAccessFlagBits::kTransferWrite,
							  RHIPipelineStageFlags::kTransfer, dst_acc_flags, dst_pipe_stage);
			cb->SetEvent(copy_event_, dst_pipe_stage);
		//}
	}

	bool IsReady(IRHIDevice* dev) const { return copy_event_->IsSet(dev); }

	void Destroy(IRHIDevice* dev) {
		staging_buf_->Unmap(dev);
		// destroy buffers...
	}
};


struct SShader {

	RHIShaderStage stages_[2];

	static SShader* load(IRHIDevice* dev, const char* vs, const char* fs) {
		size_t vs_size;
		const uint32_t* vs_data = (uint32_t*)filesystem::loadfile(vs, &vs_size);
		size_t ps_size;
		const uint32_t* ps_data = (uint32_t*)filesystem::loadfile(fs, &ps_size);

		SShader* sh = new SShader();

		sh->stages_[0].module = dev->CreateShader(RHIShaderStageFlagBits::kVertex, vs_data, vs_size);
		sh->stages_[0].pEntryPointName = "main";
		sh->stages_[0].stage = RHIShaderStageFlagBits::kVertex;
		sh->stages_[1].module = dev->CreateShader(RHIShaderStageFlagBits::kFragment, ps_data, ps_size);
		sh->stages_[1].pEntryPointName = "main";
		sh->stages_[1].stage = RHIShaderStageFlagBits::kFragment;

		delete[] vs_data;
		delete[] ps_data;

		return sh;
	}
};

IRHIRenderPass* g_main_pass = nullptr;
std::vector<IRHIFrameBuffer*> g_main_fb;
std::vector<IRHIImageView*> g_main_ds;

IRHIGraphicsPipeline *g_tri_pipeline = nullptr;
IRHIGraphicsPipeline *g_fs_clear_pipeline = nullptr;
IRHIGraphicsPipeline *g_quad_pipeline = nullptr;
IRHIGraphicsPipeline *g_world_model_pipeline = nullptr;

IRHIBuffer* g_uniform_buffer = 0;
IRHIBuffer* g_uniform_staging_buffer[kNumBufferedFrames] = {0};
IRHIEvent* g_uniform_buffer_copy_event = nullptr;	

IRHIDescriptorSetLayout* g_my_layout = 0;
IRHIDescriptorSet* g_my_ds_set0 = 0;
IRHIDescriptorSet* g_my_ds_set1 = 0;

IRHIImage* g_test_image = 0;
IRHIImageView* g_test_image_view = 0;
// temp buffer to store image data before copying to gpu mem
IRHIBuffer* g_img_staging_buf = 0;
IRHIEvent* g_img_copy_event = nullptr;

SBuffer* g_cube_ib = nullptr;
SBuffer* g_cube_vb = nullptr;
SShader* g_cube_shader = nullptr;

IRHISampler* g_test_sampler = nullptr;

enum PipelineBlend : uint8_t {
	kPipeBlendNo,
	kPipeBlendModulated,
	kPipeBlendTranslucent,
	kPipeBlendAlpha,
	kPipeBlendInvisible,
	kPipeBlendCount
};

enum SurfaceShader : uint8_t {
	kSurfaceShaderComplex,
	kSurfaceShaderGouraud,
	kSurfaceShaderClearDepth,
};

const uint32_t BLEND_MODE_MASK = 0x7;
const uint32_t DEPTH_MODE_MASK = 0x1;
const uint32_t ALPHA_TEST_MASK = 0x1;
const uint32_t DEPTH_CLEAR_MASK = 0x1;
const uint32_t SURFACE_SHADER_MASK = 0x3;

const uint32_t BLEND_MODE_OFFSET = 0;
const uint32_t DEPTH_MODE_OFFSET = 3;
const uint32_t ALPHA_TEST_OFFSET = 4;
const uint32_t DEPTH_CLEAR_OFFSET = 5;
const uint32_t SURFACE_SHADER_OFFSET = 7;

uint32_t make_key(SurfaceShader shader, PipelineBlend blend, bool depth_write, bool alpha_test) {
	uint32_t s = (uint32_t)shader;
	uint32_t b = (uint32_t)blend;
	uint32_t dw = !!depth_write;
	uint32_t at = !!alpha_test;
	//uint32_t clear = !!depth_clear;

	uint32_t rv = ((b & BLEND_MODE_MASK) << BLEND_MODE_OFFSET) |
				  ((dw & DEPTH_MODE_MASK) << DEPTH_MODE_OFFSET) |
				  ((at & ALPHA_TEST_MASK) << ALPHA_TEST_OFFSET) |
				  //((clear & DEPTH_CLEAR_MASK) << DEPTH_CLEAR_OFFSET) |
				  ((s & SURFACE_SHADER_MASK) << SURFACE_SHADER_OFFSET);

	return rv;
}

struct ComplexSurfaceDrawCall {
	uint32_t vb_offset;
	uint32_t num_vertices;
	uint32_t ib_offset;
	uint32_t num_indices;
	//TODO: just for check
	uint32_t vs_ub_idx;
	IRHIDescriptorSet* dset;
	const IRHIImageView* diffuse;
	const IRHIImageView* macro;
	const IRHIImageView* lightmap;
	const IRHIImageView* detail;
	PipelineBlend pipeline_blend;
	bool b_depth_write;
	bool b_alpha_test;
	SurfaceShader surface_shader;
	RHIViewport viewport;
};

const uint32_t gUENumVert = 256 * 1024;
const uint32_t gUENumIndices = 256 * 1024;
// expect no more than 1k draw calls per frame
// TODO: we will come up with reallocation of course,.. later
const uint32_t gUEDrawCalls = 1000;
// Complex IB
SBuffer* g_ue_complex_vb[kNumBufferedFrames] = { 0 };
uint32_t g_ue_complex_vb_size[kNumBufferedFrames] = { 0 };
// Complex VB
SBuffer* g_ue_complex_ib[kNumBufferedFrames] = { 0 };
uint32_t g_ue_complex_ib_size[kNumBufferedFrames] = { 0 };

// Gouraud IB
SBuffer* g_ue_gouraud_vb[kNumBufferedFrames] = { 0 };
uint32_t g_ue_gouraud_vb_size[kNumBufferedFrames] = { 0 };
// Gouraud VB
SBuffer* g_ue_gouraud_ib[kNumBufferedFrames] = { 0 };
uint32_t g_ue_gouraud_ib_size[kNumBufferedFrames] = { 0 };

// dynamic UB for VS per draw call data
template<typename T>
struct DynamicUB {
private:
	~DynamicUB() {}
public:
	typedef typename T El_t;
	SBuffer *buf[kNumBufferedFrames] = {0};
	uint32_t size[kNumBufferedFrames] = {0};
	IRHIDescriptorSet *dset[kNumBufferedFrames] = {0};
	uint32_t el_size = 0;
	uint32_t num_el = 0;
	// descriptor set (one per frame) designed to store per frame data (proj. matrix and stuff)
	// for now only stores VS data using dynamic UB (so no need to have ds per draw call)
	IRHIDescriptorSetLayout *ds_layout = 0;

	static DynamicUB<T>* make(uint32_t count, const IRHIDescriptorSetLayout* dsl, IRHIDevice* dev) {
		DynamicUB* ub = new DynamicUB<T>();
		ub->num_el = count;

		// offsetAlignment is also our element size (obviously)
		// there are restrictions on alignment of dynamic UB offsets
		uint32_t minUboAlignment = dev->GetProperties().minUniformBufferOffsetAlignment;
		uint32_t offsetAlignment = sizeof(typename T);
		if (minUboAlignment > 0) {
			// this alignment can only be >= sizeof(T) as it is also an offset :-)
			offsetAlignment = (offsetAlignment + minUboAlignment - 1) & ~(minUboAlignment - 1);
		}
		ub->el_size = offsetAlignment;
		// I just hope that UB alignment will be max possible alignment for any possible usage
		// (like our dynamic descriptor)
		for (int i = 0; i < kNumBufferedFrames; ++i) {
			ub->buf[i] = SBuffer::makeUB(dev, ub->num_el * ub->el_size, nullptr);
			ub->size[i] = 0;
			ub->dset[i] = dev->AllocateDescriptorSet(dsl);

			RHIDescriptorWriteDesc vs_ds_write_desc;
			RHIDescriptorWriteDescBuilder builder(&vs_ds_write_desc, 1);
			// VkDescriptorBufferInfo man page
			// For VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC and
			// VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC descriptor types, offset is the base offset
			// from which the dynamic offset is applied and range is the static size used for all
			// dynamic offsets.
			builder.add(ub->dset[i], 0, ub->buf[i]->device_buf_, 0, offsetAlignment);
			dev->UpdateDescriptorSet(&vs_ds_write_desc, builder.cur_index);

		}
		return ub;
	}

	void destroy(IRHIDevice* dev) {
		for (int i = 0; i < kNumBufferedFrames; ++i) {
			ub->buf->Destroy(dev);
		}
		// free dset
		// dev->ReleaseDescriptorSet(ub->dset);

		delete this;
	}
};

// dynamic UB for VS per draw call data
#if 0
SBuffer* g_ue_vs_ub[kNumBufferedFrames] = { 0 };
uint32_t g_ue_vs_ub_el_size = 0;
const uint32_t g_ue_vs_ub_num_el = gUEDrawCalls;
uint32_t g_ue_vs_ub_size[kNumBufferedFrames] = { 0 };
// descriptor set (one per frame) designed to store per frame data (proj. matrix and stuff)
// for now only stores VS data using dynamic UB (so no need to have ds per draw call)
IRHIDescriptorSet* g_ue_vs_ub_ds[kNumBufferedFrames] = {0};
#endif
IRHIDescriptorSetLayout* g_ue_vs_ub_dsl = 0;
DynamicUB<UEPerDrawCallComplexVsData>* g_ue_complex_vs_ub = nullptr;
DynamicUB<UEPerDrawCallGouraudVsData>* g_ue_gouraud_vs_ub = nullptr;

SShader* g_ue_complex_shader = nullptr;
SShader* g_ue_complex_shader_alpha_test = nullptr;
SShader* g_ue_gouraud_shader = nullptr;
SShader* g_ue_gouraud_shader_alpha_test = nullptr;

IRHIDescriptorSetLayout* g_ue_dsl_complex= 0;
IRHIDescriptorSetLayout* g_ue_dsl_gouraud = 0;
std::vector<IRHIDescriptorSet*> g_ue_complex_dsets[kNumBufferedFrames];
std::vector<IRHIDescriptorSet*> g_ue_gouraud_dsets[kNumBufferedFrames];
// NOTE: do not actually need to make this kNumBufferedFrames array
int g_ue_complex_dsets_reserved[kNumBufferedFrames] = { 0 };
int g_ue_gouraud_dsets_reserved[kNumBufferedFrames] = { 0 };
std::vector<ComplexSurfaceDrawCall> g_draw_calls;
//std::vector<GouraudSurfaceDrawCall> g_gouraud_draw_calls;

// UEPerDrawCallUniformBuf 
IRHIBuffer* g_ue_per_draw_call_uniforms[kNumBufferedFrames] = { 0 };
uint32_t g_ue_per_draw_call_uniforms_count[kNumBufferedFrames] = { 0 };
// UEPerFrameUniformBuf 
IRHIBuffer* g_ue_per_frame_uniforms[kNumBufferedFrames] = { 0 };
UEPerFrameUniformBuf* g_ue_per_frame_uniforms_ptr[kNumBufferedFrames] = { 0 };

std::vector<TextureUploadTask*> g_tex_upload_tasks;
std::vector<TextureUploadTask*> g_tex_upload_in_progress;
//std::vector<TextureUploadTask> g_tex_upload_done;

void update_uniform_staging_buf(int idx, IRHIDevice* dev) {
	assert(idx >= 0 && idx < kNumBufferedFrames);

	PerFrameUniforms* uniptr = (PerFrameUniforms*)g_uniform_staging_buffer[idx]->Map(dev, 0, 0xFFFFFFFF, 0);
	uniptr->stuff = vec4(1, -2, 3, -3.1415f);
	uniptr->fake_camera = mat4::rotationZ(45.0f * M_PI / 180.0f);
	static float angle = 30.0f * M_PI / 180.0f;
	angle += .2f * M_PI / 180.0f;
	uniptr->world = mat4::rotationY(angle) * mat4::rotationX(angle) *mat4::scale(vec3(0.5f));
	uniptr->view = lookAt(vec3(0, 0, -5), vec3(0, 0, 0));
	const float w = (float)dev->GetSwapChainImage(0)->Width();
	const float h = (float)dev->GetSwapChainImage(0)->Height();
	//uniptr->proj = frustumProjMatrix(-w / 2.0f, w / 2.0f, -h / 2.0f, h / 2.0f, 0.1f, 100.0f);
	uniptr->proj = g_current_projection;// perspectiveMatrixX(45.0f * M_PI / 180.0f, w, h, 1.0f, 100.0f, false);
	uniptr->vwp = uniptr->proj * uniptr->view * uniptr->world;
	mat4 vw = uniptr->view * uniptr->world;
	mat3 vw3(vw.getRow(0).xyz(), vw.getRow(1).xyz(), vw.getRow(2).xyz());
	uniptr->normal_tr.setRow(0, vec4(vw3.getRow(0), 0));
	uniptr->normal_tr.setRow(1, vec4(vw3.getRow(1), 0));
	uniptr->normal_tr.setRow(2, vec4(vw3.getRow(2), 0));
	uniptr->normal_tr.setRow(3, vec4(0,0,0,1));

	// no need to unmap in vulkan (but we flush in unmap, maybe move it to the separate function)
	g_uniform_staging_buffer[idx]->Unmap(dev);
}

void ue_update_per_frame_uniforms(int idx, IRHIDevice* dev, DWORD DetailTexColor) {
	assert(idx >= 0 && idx < kNumBufferedFrames);
	g_ue_per_frame_uniforms_ptr[idx]->proj = g_current_projection;
	vec4 det_color;
	det_color.x = (float)(DetailTexColor&0xFF);
	det_color.y = (float)((DetailTexColor>>8)&0xFF);
	det_color.z = (float)((DetailTexColor>>16)&0xFF);
	det_color.w = (float)((DetailTexColor>>24)&0xFF);
	det_color *= 1.0f/255.0f;
	det_color = clamp(det_color, 0,1);
	g_ue_per_frame_uniforms_ptr[idx]->DetailColor = det_color;
	g_ue_per_frame_uniforms[idx]->Flush(dev, 0, 0);
}

PipelineBlend select_blend(DWORD Flags) {
	PipelineBlend rv = kPipeBlendNo;
	if (Flags & PF_Invisible) {
		rv = kPipeBlendInvisible;
	} else if (Flags & PF_Translucent) {
		rv = kPipeBlendTranslucent;
	} else if (Flags & PF_Modulated) {
		rv = kPipeBlendModulated;
		// Rune specific, see PF_AlphaBlend
	} else if (Flags & PF_AlphaBlend) {
		rv = kPipeBlendAlpha;
	}

	return rv;
}

bool select_depth_write(DWORD Flags) {
	return (Flags & PF_Occlude);
}

IRHIImageView* create_depth(IRHIDevice* dev, int w, int h) {

	RHIImageDesc img_desc;
	img_desc.type = RHIImageType::k2D;
	img_desc.format = RHIFormat::kD32_SFLOAT;
    img_desc.width = w;
    img_desc.height = h;
    img_desc.depth = 1;
    img_desc.arraySize = 1;
    img_desc.numMips = 1;
    img_desc.numSamples = RHISampleCount::k1Bit;
	img_desc.tiling = RHIImageTiling::kOptimal;
	img_desc.usage = RHIImageUsageFlagBits::DepthStencilAttachmentBit;
	img_desc.sharingMode = RHISharingMode::kExclusive; // only in graphics queue

	IRHIImage* ds = dev->CreateImage(&img_desc, RHIImageLayout::kUndefined,
									   RHIMemoryPropertyFlagBits::kDeviceLocal);
	assert(ds);

	RHIImageViewDesc iv_desc;
	iv_desc.image = ds;
	iv_desc.viewType = RHIImageViewType::k2d;
    iv_desc.format = img_desc.format;
	iv_desc.subresourceRange.aspectMask = RHIImageAspectFlags::kDepth;
	iv_desc.subresourceRange.baseArrayLayer = 0;
	iv_desc.subresourceRange.baseMipLevel = 0;
	iv_desc.subresourceRange.layerCount = 1;
	iv_desc.subresourceRange.levelCount = 1;

	IRHIImageView* ds_view = dev->CreateImageView(&iv_desc);
	assert(ds_view);
	return ds_view;
}

RHIColorBlendAttachmentState create_blend_att_state(bool b_enable, RHIBlendFactor::Value src, RHIBlendFactor::Value dst) {

	RHIColorBlendAttachmentState blend_att_state;
	blend_att_state.alphaBlendOp = RHIBlendOp::kAdd;
	blend_att_state.colorBlendOp = RHIBlendOp::kAdd;
	blend_att_state.blendEnable = b_enable;
	blend_att_state.colorWriteMask =
		(uint32_t)RHIColorComponentFlags::kR | (uint32_t)RHIColorComponentFlags::kG |
		(uint32_t)RHIColorComponentFlags::kB | (uint32_t)RHIColorComponentFlags::kA;
	blend_att_state.srcColorBlendFactor = src;
	blend_att_state.dstColorBlendFactor = dst;
	blend_att_state.srcAlphaBlendFactor = RHIBlendFactor::One;
	blend_att_state.dstAlphaBlendFactor = RHIBlendFactor::Zero;
	return blend_att_state;
}

static const RHIColorBlendAttachmentState no_blend_att_state = create_blend_att_state(false, RHIBlendFactor::One, RHIBlendFactor::Zero);
static const RHIColorBlendAttachmentState modulated_blend_att_state = create_blend_att_state(true, RHIBlendFactor::DstColor, RHIBlendFactor::SrcColor);
static const RHIColorBlendAttachmentState translucent_blend_att_state = create_blend_att_state(true, RHIBlendFactor::One, RHIBlendFactor::OneMinusSrcColor);
static const RHIColorBlendAttachmentState alpha_blend_att_state = create_blend_att_state(true, RHIBlendFactor::SrcAlpha, RHIBlendFactor::OneMinusSrcAlpha);
// TODO: just disable color writes, okay...
static const RHIColorBlendAttachmentState indivisible_blend_att_state = create_blend_att_state(true, RHIBlendFactor::One, RHIBlendFactor::Zero);

// create_blend_states
static const RHIColorBlendState no_blend_state = { false, RHILogicOp::kCopy, 1, &no_blend_att_state, {0, 0, 0, 0}};
static const RHIColorBlendState modulated_blend_state = { false, RHILogicOp::kCopy, 1, &modulated_blend_att_state, {0, 0, 0, 0}};
static const RHIColorBlendState translucent_blend_state = { false, RHILogicOp::kCopy, 1, &translucent_blend_att_state, {0, 0, 0, 0}};
static const RHIColorBlendState alpha_blend_state = { false, RHILogicOp::kCopy, 1, &alpha_blend_att_state, {0, 0, 0, 0}};
static const RHIColorBlendState invisible_blend_state = { false, RHILogicOp::kCopy, 1, &indivisible_blend_att_state, {0, 0, 0, 0}};

static const RHIColorBlendState * const g_blend_states[kPipeBlendCount] = {&no_blend_state, &modulated_blend_state,
									  &translucent_blend_state, &alpha_blend_state,
									  &invisible_blend_state};

// Pipelines for different states
std::unordered_map<uint32_t, IRHIGraphicsPipeline*> g_ue_pipelines;

/**
Attempts to read a property from the game's config file; on failure, a default is written (so it can be changed by the user) and returned.
\param name A string identifying the config file options.
\param defaultVal The default value to write and return if the option is not found.
\param isBool Whether the parameter's a boolean or integer
\return The value for the property.
\note The default value is written so it can be user modified (either from the config or preferences window) from then on.
*/
int UVulkanRenderDevice::getOption(TCHAR* name,int defaultVal, bool isBool)
{
	TCHAR* Section = L"VulkanDrv.VulkanRenderDevice";
	int out;
	if(isBool)
	{
		if(!GConfig->GetBool( Section, name, (INT&) out))
		{
			GConfig->SetBool(Section,name,defaultVal);
			out = defaultVal;
		}
	}
	else
	{
		if(!GConfig->GetInt( Section, name, (INT&) out))
		{
			GConfig->SetInt(Section,name,defaultVal);
			out = defaultVal;
		}
	}
	return out;
}

UVulkanRenderDevice::UVulkanRenderDevice()
{
	log_info("constructing vulkan render device...\n");
}

/**
Constructor called by the game when the renderer is first created.
\note Required to compile for Unreal Tournament. 
\note Binding settings to the preferences window needs to done here instead of in init() or the game crashes when starting a map if the renderer's been restarted at least once.
*/
#pragma warning (push)
#pragma warning (disable: 4291)
void UVulkanRenderDevice::StaticConstructor()
{
	//Make the property appear in the preferences window; this will automatically pick up the current value and write back changes.	
	new(GetClass(), L"Precache", RF_Public) UBoolProperty(CPP_PROPERTY(options.precache), TEXT("Options"), CPF_Config);
	new(GetClass(), L"Antialiasing", RF_Public) UIntProperty(CPP_PROPERTY(VulkanOptions.samples), TEXT("Options"), CPF_Config);
	new(GetClass(), L"Anisotropy", RF_Public) UIntProperty(CPP_PROPERTY(VulkanOptions.aniso), TEXT("Options"), CPF_Config);
	new(GetClass(), L"VSync", RF_Public) UBoolProperty(CPP_PROPERTY(VulkanOptions.VSync), TEXT("Options"), CPF_Config);
	new(GetClass(), L"ParallaxOcclusionMapping", RF_Public) UBoolProperty(CPP_PROPERTY(VulkanOptions.POM), TEXT("Options"), CPF_Config);
	new(GetClass(), L"LODBias", RF_Public) UIntProperty(CPP_PROPERTY(VulkanOptions.LODBias), TEXT("Options"), CPF_Config);
	new(GetClass(), L"AlphaToCoverage", RF_Public) UBoolProperty(CPP_PROPERTY(VulkanOptions.alphaToCoverage), TEXT("Options"), CPF_Config);
	new(GetClass(), L"BumpMapping", RF_Public) UBoolProperty(CPP_PROPERTY(VulkanOptions.bumpMapping), TEXT("Options"), CPF_Config);
	new(GetClass(), L"ClassicLighting", RF_Public) UBoolProperty(CPP_PROPERTY(VulkanOptions.classicLighting), TEXT("Options"), CPF_Config);
	new(GetClass(), L"AutoFOV", RF_Public) UBoolProperty(CPP_PROPERTY(options.autoFOV), TEXT("Options"), CPF_Config);
	new(GetClass(), L"FPSLimit", RF_Public) UIntProperty(CPP_PROPERTY(options.FPSLimit), TEXT("Options"), CPF_Config);
	new(GetClass(), L"SimulateMultiPassTexturing", RF_Public) UBoolProperty(CPP_PROPERTY(VulkanOptions.simulateMultipassTexturing), TEXT("Options"), CPF_Config);
	new(GetClass(), L"UnlimitedViewDistance", RF_Public) UBoolProperty(CPP_PROPERTY(options.unlimitedViewDistance), TEXT("Options"), CPF_Config);


	new(GetClass(), L"ColorizeDetailTextures", RF_Public) UBoolProperty(CPP_PROPERTY(options.ColorizeDetailTextures), TEXT("Options"), CPF_Config);
	//SC_AddBoolConfigParam(8,  TEXT("ColorizeDetailTextures"), CPP_PROPERTY_LOCAL(options.ColorizeDetailTextures), 0);

	//Turn on parent class options by default. If done here (instead of in Init()), the ingame preferences still work
	getOption(L"Coronas",1,true);
	getOption(L"HighDetailActors",1,true);
	getOption(L"VolumetricLighting",1,true);
	getOption(L"ShinySurfaces",1,true);
	getOption(L"DetailTextures",1,true);

	//Create a console to print debug stuff to.
	#ifdef _DEBUG
	AllocConsole();
	//stdout->_file = _open_osfhandle((long)GetStdHandle(STD_OUTPUT_HANDLE),_O_TEXT);
	#endif
}
#pragma warning(pop)

/**
Initialization of renderer.
- Set parent class options. Some of these are settings for the renderer to heed, others control what the game does.
	- URenderDevice::SpanBased; Probably for software renderers.
	- URenderDevice::Fullscreen; Only for Voodoo cards.
	- URenderDevice::SupportsTC; Game sends compressed textures if present.
	- URenderDevice::SupportsDistanceFog; Distance fog. Don't know how this is supposed to be implemented.
	- URenderDevice::SupportsLazyTextures; Renderer loads and unloads texture info when needed (???).
	- URenderDevice::PrefersDeferredLoad; Renderer prefers not to cache textures in advance (???).
	- URenderDevice::ShinySurfaces; Renderer supports detail textures. The game sends them always, so it's meant as a detail setting for the renderer.
	- URenderDevice::Coronas; If enabled, the game draws light coronas.
	- URenderDevice::HighDetailActors; If enabled, game sends more detailed models (???).
	- URenderDevice::VolumetricLighting; If enabled, the game sets fog textures for surfaces if needed.
	- URenderDevice::PrecacheOnFlip; The game will call the PrecacheTexture() function to load textures in advance. Also see Flush().
	- URenderDevice::Viewport; Always set to InViewport.
- Initialize graphics api.
- Resize buffers (convenient to use SetRes() for this).

\param InViewport viewport parameters, can get the window handle.
\param NewX Viewport width.
\param NewY Viewport height.
\param NewColorBytes Color depth.
\param Fullscreen Whether fullscreen mode should be used.
\return 1 if init succesful. On 0, game errors out.

\note D3D10 renderer ignores color depth.
*/
UBOOL UVulkanRenderDevice::Init(UViewport* InViewport, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen)
{
	log_info("Initializing Vulkan render device.\n");

	//Set parent class params
	URenderDevice::SpanBased = 0;
	URenderDevice::FullscreenOnly = 0;
	URenderDevice::SupportsFogMaps = 1;
	URenderDevice::SupportsTC = 1;
	URenderDevice::SupportsDistanceFog = 0;
	URenderDevice::SupportsLazyTextures = 0;

	//Get/set config options.
	options.precache = getOption(L"Precache",0,true);
	VulkanOptions.samples = getOption(L"Antialiasing",4,false);
	VulkanOptions.aniso = getOption(L"Anisotropy",8,false);
	VulkanOptions.VSync = getOption(L"VSync",1,true);	
	VulkanOptions.POM = getOption(L"ParallaxOcclusionMapping",0,true);	
	VulkanOptions.LODBias = getOption(L"LODBias",0,false);
	VulkanOptions.bumpMapping = getOption(L"BumpMapping",0,true);	
	VulkanOptions.classicLighting = getOption(L"ClassicLighting",1,true);	
	VulkanOptions.alphaToCoverage = getOption(L"AlphaToCoverage",0,true);
	options.autoFOV = getOption(L"AutoFOV",1,true);
	options.FPSLimit = getOption(L"FPSLimit",100,false);
	VulkanOptions.simulateMultipassTexturing = getOption(L"simulateMultipassTexturing",1,true);
	options.unlimitedViewDistance = getOption(L"unlimitedViewDistance",0,true);

	if(options.unlimitedViewDistance)
		zFar = 65536.0f;
	else
		zFar = 32760.0f;

	g_texCache = TextureCache::makeCache();
	texture_upload_task_init();
	 
	//Set parent options
	URenderDevice::Viewport = InViewport;

	//Do some nice compatibility fixing: set processor affinity to single-cpu
	SetProcessAffinityMask(GetCurrentProcess(),0x1);
#if USE_GLAD_LOADER
	int glad_vk_version = gladLoaderLoadVulkan(NULL, NULL, NULL);
    if (!glad_vk_version) {
        log_error("gladLoad Failure: Unable to load Vulkan symbols!\n");
    }
	else
	{
		log_info("Init: glad vulkan load succeded, version: %d\n", glad_vk_version);
	}
#endif

	// TODO: pass VulkanOptions
	if(!vulkan_initialize((HWND)InViewport->GetWindow()))
	{
		GError->Log(L"Init: Initializing vulkan failed.");
		return 0;
	}

	g_vulkan_device = create_device();
	assert(g_vulkan_device);
	g_vulkan_device->SetOnSwapChainRecreatedCallback(UVulkanRenderDevice::OnSwapChainRecreated, this);

	IRHIDevice* device = g_vulkan_device;
	for (size_t i = 0; i < kNumBufferedFrames; ++i) {
		g_cmdbuf[i] = device->CreateCommandBuffer(RHIQueueType::kGraphics);
	}
	g_curFBIdx = -1;

	SVDAdapter<> cube_data;
	generate_cube(cube_data, vec3(1), vec3(0.0f));
	//g_cube_ib = SBuffer::makeIB(device, 36 * sizeof(uint16_t), 0);
	g_cube_vb = SBuffer::makeVB(device, 36 * sizeof(SVD), cube_data.vb_);
	g_cube_shader = SShader::load(device, "vulkandrv/spir-v-world-model.vert.spv.bin",
								  "vulkandrv/spir-v-world-model.frag.spv.bin");

	g_quad_staging_vb =
		device->CreateBuffer(sizeof(vb), RHIBufferUsageFlagBits::kTransferSrcBit,
							 RHIMemoryPropertyFlagBits::kHostVisible, RHISharingMode::kExclusive);

	g_quad_gpu_vb = device->CreateBuffer(
		sizeof(vb), RHIBufferUsageFlagBits::kVertexBufferBit | RHIBufferUsageFlagBits::kTransferDstBit,
		RHIMemoryPropertyFlagBits::kDeviceLocal, RHISharingMode::kExclusive);

	assert(g_quad_gpu_vb);
	assert(g_quad_staging_vb);
	uint8_t *memptr = (uint8_t *)g_quad_staging_vb->Map(device, 0, sizeof(vb), 0);
	memcpy(memptr, vb, sizeof(vb));
	g_quad_staging_vb->Unmap(device);

	g_quad_vb_copy_event = device->CreateEvent();

	RHIDescriptorSetLayoutDesc ue_dsl_complex_desc[] = {
		// diffuse
		{RHIDescriptorType::kCombinedImageSampler, RHIShaderStageFlagBits::kFragment, 1, 0},
		// lightmap
		{RHIDescriptorType::kCombinedImageSampler, RHIShaderStageFlagBits::kFragment, 1, 1},
		// detail/fog 
		{RHIDescriptorType::kCombinedImageSampler, RHIShaderStageFlagBits::kFragment, 1, 2},
		// macro 
		{RHIDescriptorType::kCombinedImageSampler, RHIShaderStageFlagBits::kFragment, 1, 3},
		// per frame
		{RHIDescriptorType::kUniformBuffer, RHIShaderStageFlagBits::kFragment | RHIShaderStageFlagBits::kVertex, 1, 4},
		// per draw call
		{RHIDescriptorType::kUniformBuffer, RHIShaderStageFlagBits::kFragment | RHIShaderStageFlagBits::kVertex, 1, 5},
	};

	RHIDescriptorSetLayoutDesc ue_dsl_gouraud_desc[] = {
		// diffuse
		{RHIDescriptorType::kCombinedImageSampler, RHIShaderStageFlagBits::kFragment, 1, 0},
		// per frame
		{RHIDescriptorType::kUniformBuffer, RHIShaderStageFlagBits::kFragment | RHIShaderStageFlagBits::kVertex, 1, 1},
		// per draw call
		{RHIDescriptorType::kUniformBuffer, RHIShaderStageFlagBits::kFragment | RHIShaderStageFlagBits::kVertex, 1, 2},
	};

	RHIDescriptorSetLayoutDesc ue_vs_dsl_desc[] = {
		{RHIDescriptorType::kUniformBufferDynamic, RHIShaderStageFlagBits::kVertex, 1, 0}
	};
	g_ue_dsl_complex = device->CreateDescriptorSetLayout(ue_dsl_complex_desc, countof(ue_dsl_complex_desc));
	g_ue_dsl_gouraud = device->CreateDescriptorSetLayout(ue_dsl_gouraud_desc, countof(ue_dsl_gouraud_desc));
	g_ue_vs_ub_dsl = device->CreateDescriptorSetLayout(ue_vs_dsl_desc, countof(ue_vs_dsl_desc));

	// create ue geometry buffers (one per swap chain len)
	for (int i = 0; i < kNumBufferedFrames; ++i) {
		g_ue_complex_vb[i] = SBuffer::makeVB(device, gUENumVert* sizeof(UEVertexComplex), nullptr);
		g_ue_complex_vb_size[i] = 0;
		g_ue_complex_ib[i] = SBuffer::makeIB(device, gUENumIndices* sizeof(uint32_t), nullptr);
		g_ue_complex_ib_size[i] = 0;

		g_ue_gouraud_vb[i] = SBuffer::makeVB(device, gUENumVert* sizeof(UEVertexGouraud), nullptr);
		g_ue_gouraud_vb_size[i] = 0;
		g_ue_gouraud_ib[i] = SBuffer::makeIB(device, gUENumIndices* sizeof(uint32_t), nullptr);
		g_ue_gouraud_ib_size[i] = 0;

		// TODO: check flags
		g_ue_per_draw_call_uniforms[i] = device->CreateBuffer(
			sizeof(UEPerDrawCallUniformBuf)*gUEDrawCalls, RHIBufferUsageFlagBits::kUniformBufferBit,
			RHIMemoryPropertyFlagBits::kHostVisible, RHISharingMode::kExclusive);
		g_ue_per_draw_call_uniforms_count[i] = 0;

		// TODO: check flags
		g_ue_per_frame_uniforms[i] = device->CreateBuffer(
			sizeof(UEPerFrameUniformBuf), RHIBufferUsageFlagBits::kUniformBufferBit,
			RHIMemoryPropertyFlagBits::kHostVisible, RHISharingMode::kExclusive);
		g_ue_per_frame_uniforms_ptr[i] =
			(UEPerFrameUniformBuf*)g_ue_per_frame_uniforms[i]->Map(device, 0, 0xFFFFFFFF, 0);
	}

	g_ue_complex_vs_ub = DynamicUB<UEPerDrawCallComplexVsData>::make(gUEDrawCalls, g_ue_vs_ub_dsl, device);
	g_ue_gouraud_vs_ub = DynamicUB<UEPerDrawCallGouraudVsData>::make(gUEDrawCalls*2, g_ue_vs_ub_dsl, device);

	g_ue_complex_shader = SShader::load(device, "vulkandrv/complex-surface.vert.spv.bin",
										"vulkandrv/complex-surface.frag.spv.bin");

	g_ue_complex_shader_alpha_test = SShader::load(device, "vulkandrv/complex-surface.vert.spv.bin",
										"vulkandrv/complex-surface-atest.frag.spv.bin");

	g_ue_gouraud_shader = SShader::load(device, "vulkandrv/gouraud-surface.vert.spv.bin",
										"vulkandrv/gouraud-surface.frag.spv.bin");
	g_ue_gouraud_shader_alpha_test = SShader::load(device, "vulkandrv/gouraud-surface.vert.spv.bin",
										"vulkandrv/gouraud-surface-atest.frag.spv.bin");

	RHIAttachmentDesc att_desc[2]; // color + depth
	att_desc[0].format = device->GetSwapChainFormat();
	att_desc[0].numSamples = 1;
	att_desc[0].loadOp = RHIAttachmentLoadOp::kClear;
	att_desc[0].storeOp = RHIAttachmentStoreOp::kStore;
	att_desc[0].stencilLoadOp = RHIAttachmentLoadOp::kDoNotCare;
	att_desc[0].stencilStoreOp = RHIAttachmentStoreOp::kDoNotCare;
	att_desc[0].initialLayout = RHIImageLayout::kUndefined;
	att_desc[0].finalLayout = RHIImageLayout::kPresent;

	att_desc[1].format = RHIFormat::kD32_SFLOAT;
	att_desc[1].numSamples = 1;
	att_desc[1].loadOp = RHIAttachmentLoadOp::kClear;
	att_desc[1].storeOp = RHIAttachmentStoreOp::kDoNotCare;
	att_desc[1].stencilLoadOp = RHIAttachmentLoadOp::kDoNotCare;
	att_desc[1].stencilStoreOp = RHIAttachmentStoreOp::kDoNotCare; 
	att_desc[1].initialLayout = RHIImageLayout::kUndefined;
	att_desc[1].finalLayout = RHIImageLayout::kPresent; // ?

	RHIAttachmentRef color_att_ref = {0, RHIImageLayout::kColorOptimal};
	RHIAttachmentRef depth_att_ref = {1, RHIImageLayout::kDepthStencilOptimal};

	RHISubpassDesc sp_desc;
	sp_desc.bindPoint = RHIPipelineBindPoint::kGraphics;
	sp_desc.colorAttachmentCount = 1;
	sp_desc.colorAttachments = &color_att_ref;
	sp_desc.depthStencilAttachment = &depth_att_ref;
	sp_desc.inputAttachmentCount = 0;
	sp_desc.inputAttachments = nullptr;
	sp_desc.preserveAttachmentCount = 0;
	sp_desc.preserveAttachments = nullptr;

	RHISubpassDependency sp_dep0;
	sp_dep0.srcSubpass = kSubpassExternal;
	sp_dep0.dstSubpass = 0;
	sp_dep0.srcStageMask = RHIPipelineStageFlags::kBottomOfPipe;
	sp_dep0.dstStageMask = RHIPipelineStageFlags::kColorAttachmentOutput;
	sp_dep0.srcAccessMask = RHIAccessFlagBits::kMemoryRead;
	sp_dep0.dstAccessMask = RHIAccessFlagBits::kColorAttachmentWrite;
	sp_dep0.dependencyFlags = (uint32_t)RHIDependencyFlags::kByRegion;

	RHISubpassDependency sp_dep1;
	sp_dep1.srcSubpass = 0;
	sp_dep1.dstSubpass = kSubpassExternal;
	sp_dep1.srcStageMask = RHIPipelineStageFlags::kColorAttachmentOutput;
	sp_dep1.dstStageMask = RHIPipelineStageFlags::kBottomOfPipe;
	sp_dep1.srcAccessMask = RHIAccessFlagBits::kColorAttachmentWrite;
	sp_dep1.dstAccessMask = RHIAccessFlagBits::kMemoryRead;
	sp_dep1.dependencyFlags = (uint32_t)RHIDependencyFlags::kByRegion;

	RHISubpassDependency sp_deps[] = { sp_dep0, sp_dep1 };

	RHIRenderPassDesc rp_desc;
	rp_desc.attachmentCount = countof(att_desc);
	rp_desc.attachmentDesc = att_desc;
	rp_desc.subpassCount = 1;
	rp_desc.subpassDesc = &sp_desc;
	rp_desc.dependencyCount = countof(sp_deps);
	rp_desc.dependencies = sp_deps;

	g_main_pass = device->CreateRenderPass(&rp_desc);

	g_main_fb.resize(device->GetSwapChainSize());
	g_main_ds.resize(device->GetSwapChainSize());
	for (size_t i = 0; i < g_main_fb.size(); ++i) {
		IRHIImageView *view = device->GetSwapChainImageView(i);
		// TODO: can get image from view
		const IRHIImage *image = device->GetSwapChainImage(i);
		IRHIImageView* ds_view = create_depth(device, image->Width(), image->Height());
		assert(ds_view->GetImage()->Format() == att_desc[1].format);
		g_main_ds[i] = ds_view;

		IRHIImageView* att_arr[] = { view, ds_view };

		RHIFrameBufferDesc fb_desc;
		fb_desc.attachmentCount = countof(att_arr);
		fb_desc.pAttachments = att_arr;
		fb_desc.width_ = image->Width();
		fb_desc.height_ = image->Height();
		fb_desc.layers_ = 1;
		g_main_fb[i] = device->CreateFrameBuffer(&fb_desc, g_main_pass);
	}

	// TODO: delete those after use
	SShader* tri_shader = SShader::load(device, "vulkandrv/spir-v.vert.spv.bin",
								  "vulkandrv/spir-v.frag.spv.bin");
	SShader* quad_shader = SShader::load(device, "vulkandrv/spir-v-model.vert.spv.bin",
								  "vulkandrv/spir-v-model.frag.spv.bin");

	SShader* fs_quad = SShader::load(device, "vulkandrv/fs-quad.vert.spv.bin",
								  "vulkandrv/fs-quad.frag.spv.bin");

	Image img;
	const char *image_path = "./data/ut.bmp";
	if (!img.loadFromFile(image_path)) {
		log_error("Init: failed to load; %s\n", image_path);
		return false;
	}

	FORMAT img_fmt = img.getFormat();
	if (img_fmt != FORMAT_RGB8 && img_fmt != FORMAT_RGBA8) {
		log_error(("Unsupported texture format when loading %s\n", image_path));
	}

	RHIImageDesc img_desc;
	img_desc.type = RHIImageType::k2D;
    img_desc.format = img_fmt == FORMAT_RGB8 ? RHIFormat::kR8G8B8_UNORM : RHIFormat::kR8G8B8A8_UNORM;
    img_desc.width = img.getWidth();
    img_desc.height = img.getHeight();
    img_desc.depth = 1;
    img_desc.arraySize = 1;
    img_desc.numMips = 1;
    img_desc.numSamples = RHISampleCount::k1Bit;
	img_desc.tiling = RHIImageTiling::kOptimal;
	img_desc.usage = RHIImageUsageFlagBits::SampledBit|RHIImageUsageFlagBits::TransferDstBit;
	img_desc.sharingMode = RHISharingMode::kExclusive; // only in graphics queue

	//TODO: why do we need this initial image layout of it only can be undefined or preinitialized?
	g_test_image = device->CreateImage(&img_desc, RHIImageLayout::kUndefined,
									   RHIMemoryPropertyFlagBits::kDeviceLocal);
	assert(g_test_image);

	g_img_staging_buf =
		device->CreateBuffer(1024*1024*4, RHIBufferUsageFlagBits::kTransferSrcBit,
							 RHIMemoryPropertyFlagBits::kHostVisible, RHISharingMode::kExclusive);
	assert(g_img_staging_buf );
	const uint32_t img_size = img.getWidth() * img.getHeight() * getBytesPerPixel(img.getFormat());
	uint8_t *img_memptr = (uint8_t *)g_img_staging_buf->Map(device, 0, img_size, 0);
	memcpy(img_memptr, img.getPixels(), img_size);
	g_img_staging_buf->Unmap(device);

	g_img_copy_event = device->CreateEvent();

	RHIImageViewDesc iv_desc;
	iv_desc.image = g_test_image;
	iv_desc.viewType = RHIImageViewType::k2d;
    iv_desc.format = img_desc.format;
	iv_desc.subresourceRange.aspectMask = RHIImageAspectFlags::kColor;
	iv_desc.subresourceRange.baseArrayLayer = 0;
	iv_desc.subresourceRange.baseMipLevel = 0;
	iv_desc.subresourceRange.layerCount = 1;
	iv_desc.subresourceRange.levelCount = 1;

	g_test_image_view = device->CreateImageView(&iv_desc);
	assert(g_test_image_view);

	RHIVertexInputState tri_vi_state;
	tri_vi_state.vertexBindingDescCount = 0;
	tri_vi_state.pVertexBindingDesc = nullptr;
	tri_vi_state.vertexAttributeDescCount = 0;
	tri_vi_state.pVertexAttributeDesc = nullptr;

	RHIVertexInputState quad_vi_state;
	quad_vi_state.vertexBindingDescCount = countof(vert_bindings_desc);
	quad_vi_state.pVertexBindingDesc = vert_bindings_desc;
	quad_vi_state.vertexAttributeDescCount = countof(va_desc);
	quad_vi_state.pVertexAttributeDesc = va_desc;

	RHIVertexInputState world_model_vi_state;
	world_model_vi_state.vertexBindingDescCount = countof(svd_vert_bindings_desc);
	world_model_vi_state.pVertexBindingDesc = svd_vert_bindings_desc;
	world_model_vi_state.vertexAttributeDescCount = countof(world_model_va_desc);
	world_model_vi_state.pVertexAttributeDesc = world_model_va_desc;

	RHIVertexInputState ue_vi_complex_state;
	ue_vi_complex_state.vertexBindingDescCount = countof(ue_complex_vert_bindings_desc);
	ue_vi_complex_state.pVertexBindingDesc = ue_complex_vert_bindings_desc;
	ue_vi_complex_state.vertexAttributeDescCount = countof(ue_complex_va_desc);
	ue_vi_complex_state.pVertexAttributeDesc = ue_complex_va_desc;

	RHIVertexInputState ue_vi_gouraud_state;
	ue_vi_gouraud_state.vertexBindingDescCount = countof(ue_gouraud_vert_bindings_desc);
	ue_vi_gouraud_state.pVertexBindingDesc = ue_gouraud_vert_bindings_desc;
	ue_vi_gouraud_state.vertexAttributeDescCount = countof(ue_gouraud_va_desc);
	ue_vi_gouraud_state.pVertexAttributeDesc = ue_gouraud_va_desc;

	RHIInputAssemblyState tri_ia_state;
	tri_ia_state.primitiveRestartEnable = false;
	tri_ia_state.topology = RHIPrimitiveTopology::kTriangleList;

	RHIInputAssemblyState quad_ia_state;
	quad_ia_state.primitiveRestartEnable = false;
	quad_ia_state.topology = RHIPrimitiveTopology::kTriangleStrip;

	RHIInputAssemblyState tris_ia_state;
	tris_ia_state.primitiveRestartEnable = false;
	tris_ia_state.topology = RHIPrimitiveTopology::kTriangleList;

	RHIInputAssemblyState ue_ia_state;
	ue_ia_state.primitiveRestartEnable = false;
	ue_ia_state.topology = RHIPrimitiveTopology::kTriangleFan;

	RHIScissor scissors;
	scissors.x = 0;
	scissors.y = 0;
	// just "disable" it
	scissors.width = (uint32_t)4096;
	scissors.height = (uint32_t)4096;

	RHIViewport viewport;
	viewport.x = viewport.y = 0;
	viewport.width = (float)NewX;
	viewport.height = (float)NewY;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	RHIViewportState viewport_state;
	viewport_state.pScissors = &scissors;
	viewport_state.pViewports = &viewport;
	viewport_state.scissorCount = 1;
	viewport_state.viewportCount = 1;

	RHIRasterizationState raster_state;
	raster_state.depthClampEnable = false;
	raster_state.cullMode = RHICullModeFlags::kBack;
	raster_state.depthBiasClamp = 0.0f;
	raster_state.depthBiasConstantFactor = 0.0f;
	raster_state.depthBiasEnable = false;
	raster_state.depthBiasSlopeFactor = 0.0f;
	raster_state.frontFace = RHIFrontFace::kCounterClockwise;
	raster_state.lineWidth = 1.0f;
	raster_state.polygonMode = RHIPolygonMode::kFill;
	raster_state.rasterizerDiscardEnable = false;

	RHIRasterizationState world_model_raster_state = raster_state;
	world_model_raster_state.frontFace = RHIFrontFace::kCounterClockwise;

	RHIRasterizationState ue_raster_state = raster_state;
	ue_raster_state.frontFace = RHIFrontFace::kClockwise;

	RHIMultisampleState ms_state;
	ms_state.alphaToCoverageEnable = false;
	ms_state.alphaToOneEnable = false;
	ms_state.minSampleShading = 0.0f;
	ms_state.pSampleMask = nullptr;
	ms_state.rasterizationSamples = 1;
	ms_state.sampleShadingEnable = false;

	RHIStencilOpState front_n_back;
	front_n_back.failOp = RHIStencilOp::kKeep;
	front_n_back.passOp = RHIStencilOp::kKeep;
	front_n_back.depthFailOp = RHIStencilOp::kKeep;
	front_n_back.compareOp = RHICompareOp::kAlways;
	front_n_back.compareMask = 0;
	front_n_back.writeMask = 0;
	front_n_back.reference = 0;


	RHIDepthStencilState ds_state;
	ds_state.depthTestEnable = true;
	ds_state.depthWriteEnable = true;
	ds_state.depthCompareOp = RHICompareOp::kGreaterOrEqual;
	ds_state.depthBoundsTestEnable = false;
	ds_state.stencilTestEnable = false;
	ds_state.front = front_n_back;
	ds_state.back = front_n_back;
	ds_state.minDepthBounds = 0;
	ds_state.maxDepthBounds = 1;

	const RHIDepthStencilState ds_write_state = ds_state;
	RHIDepthStencilState ds_no_write_state = ds_state;
	ds_no_write_state.depthWriteEnable = false;

	RHIDepthStencilState ds_write_always = ds_state;
	ds_write_always.depthCompareOp = RHICompareOp::kAlways;


	RHIColorBlendAttachmentState no_blend_no_color = create_blend_att_state(false, RHIBlendFactor::One, RHIBlendFactor::Zero);
	no_blend_no_color.colorWriteMask = 0;
	const RHIColorBlendState no_blend_no_color_state = { false, RHILogicOp::kCopy, 1, &no_blend_no_color, {0, 0, 0, 0}};

	RHIDynamicState::Value dyn_state[] = { RHIDynamicState::kViewport };

	RHIDescriptorSetLayoutDesc dsl_desc[] = {
		{RHIDescriptorType::kSampler, RHIShaderStageFlagBits::kFragment, 1, 0},
		{RHIDescriptorType::kCombinedImageSampler, RHIShaderStageFlagBits::kFragment, 1, 1},
		{RHIDescriptorType::kUniformBuffer, RHIShaderStageFlagBits::kFragment | RHIShaderStageFlagBits::kVertex, 1, 2}
	};


	RHISamplerDesc sampler_desc;
	sampler_desc.magFilter = RHIFilter::kLinear;
	sampler_desc.minFilter = RHIFilter::kLinear;
	sampler_desc.mipmapMode = RHISamplerMipmapMode::kLinear;
	sampler_desc.addressModeU = RHISamplerAddressMode::kRepeat;
	sampler_desc.addressModeV = RHISamplerAddressMode::kRepeat;
	sampler_desc.addressModeW = RHISamplerAddressMode::kRepeat;
	sampler_desc.mipLodBias = 0;
	sampler_desc.anisotropyEnable = false;
	sampler_desc.maxAnisotropy = 0.0f;
	sampler_desc.compareEnable = false;
	sampler_desc.compareOp = RHICompareOp::kAlways;
	sampler_desc.minLod = 0;
	sampler_desc.maxLod = 10;
	sampler_desc.unnormalizedCoordinates = false;
	g_test_sampler = device->CreateSampler(sampler_desc);
	IRHISampler* test_sampler2 = device->CreateSampler(sampler_desc);

	g_uniform_buffer = device->CreateBuffer(
		sizeof(PerFrameUniforms),
		RHIBufferUsageFlagBits::kUniformBufferBit | RHIBufferUsageFlagBits::kTransferDstBit,
							 RHIMemoryPropertyFlagBits::kDeviceLocal, RHISharingMode::kExclusive);

	for (int i = 0; i < countof(g_uniform_staging_buffer); ++i) {
		g_uniform_staging_buffer[i] = device->CreateBuffer(
			sizeof(PerFrameUniforms),
			RHIBufferUsageFlagBits::kTransferSrcBit,
			RHIMemoryPropertyFlagBits::kHostVisible, RHISharingMode::kExclusive);
			update_uniform_staging_buf(i, device);
	}
	g_uniform_buffer_copy_event = device->CreateEvent();


	g_my_layout = device->CreateDescriptorSetLayout(dsl_desc, countof(dsl_desc));
	g_my_ds_set0 = device->AllocateDescriptorSet(g_my_layout);
	g_my_ds_set1 = device->AllocateDescriptorSet(g_my_layout);

	RHIDescriptorWriteDesc desc_write_desc[4];
	RHIDescriptorWriteDescBuilder builder(desc_write_desc, countof(desc_write_desc));
	builder.add(g_my_ds_set0, 0, g_test_sampler)
		.add(g_my_ds_set0, 1, g_test_sampler, RHIImageLayout::kShaderReadOnlyOptimal, g_test_image_view)
		.add(g_my_ds_set0, 2, g_uniform_buffer, 0, sizeof(PerFrameUniforms));
		//.add(g_my_ds_set1, 0, g_test_sampler2);// .add(g_my_ds_set1, 2, g_uniform_buffer, 0, sizeof(PerFrameUniforms))
		;
	device->UpdateDescriptorSet(desc_write_desc, builder.cur_index);



	const IRHIDescriptorSetLayout* pipe_layout_desc[] = { g_my_layout, g_my_layout };
	IRHIPipelineLayout *pipeline_layout = device->CreatePipelineLayout(pipe_layout_desc, countof(pipe_layout_desc));
	IRHIPipelineLayout *pipeline_layout_empty = device->CreatePipelineLayout(nullptr, 0);

	const IRHIDescriptorSetLayout* ue_complex_pipe_layout_desc[] = { g_ue_dsl_complex, g_ue_vs_ub_dsl};
	IRHIPipelineLayout *ue_complex_pipeline_layout =
		device->CreatePipelineLayout(ue_complex_pipe_layout_desc, countof(ue_complex_pipe_layout_desc));

	const IRHIDescriptorSetLayout* ue_gouraud_pipe_layout_desc[] = { g_ue_dsl_gouraud, g_ue_vs_ub_dsl };
	IRHIPipelineLayout *ue_gouraud_pipeline_layout =
		device->CreatePipelineLayout(ue_gouraud_pipe_layout_desc, countof(ue_gouraud_pipe_layout_desc));

	g_tri_pipeline = device->CreateGraphicsPipeline(
		tri_shader->stages_, countof(tri_shader->stages_), &tri_vi_state, &tri_ia_state,
		&viewport_state, &raster_state, &ms_state, &ds_state, &no_blend_state, pipeline_layout,
		dyn_state, countof(dyn_state), g_main_pass);

	g_fs_clear_pipeline = device->CreateGraphicsPipeline(
		fs_quad->stages_, countof(fs_quad->stages_), &tri_vi_state, &tri_ia_state,
		&viewport_state, &raster_state, &ms_state, &ds_write_always, &no_blend_no_color_state, pipeline_layout_empty,
		dyn_state, countof(dyn_state), g_main_pass);

	g_quad_pipeline = device->CreateGraphicsPipeline(
		quad_shader->stages_, countof(quad_shader->stages_), &quad_vi_state, &quad_ia_state,
		&viewport_state, &raster_state, &ms_state, &ds_state, &no_blend_state, pipeline_layout,
		dyn_state, countof(dyn_state), g_main_pass);

	g_world_model_pipeline = device->CreateGraphicsPipeline(
		g_cube_shader->stages_, countof(g_cube_shader->stages_), &world_model_vi_state,
		&tris_ia_state, &viewport_state, &world_model_raster_state, &ms_state, &ds_state,
		&no_blend_state, pipeline_layout, dyn_state, countof(dyn_state), g_main_pass);

	for (uint8_t j = 0; j < 2; j++) {
		const RHIDepthStencilState* depth_state = j ? &ds_write_state : &ds_no_write_state;
		for (uint8_t i = 0; i < kPipeBlendCount; i++) {
			IRHIGraphicsPipeline* pipeline = device->CreateGraphicsPipeline(
				g_ue_complex_shader->stages_, countof(g_ue_complex_shader->stages_), &ue_vi_complex_state,
				&ue_ia_state, &viewport_state, &ue_raster_state, &ms_state, depth_state, g_blend_states[i],
				ue_complex_pipeline_layout, dyn_state, countof(dyn_state), g_main_pass);

			uint32_t key = make_key(kSurfaceShaderComplex, (PipelineBlend)i, j!=0, !"ALPHA_TEST");
			assert(g_ue_pipelines.count(key) == 0);
			g_ue_pipelines.insert(std::make_pair(key, pipeline));
		}
	}
	// clear Z
	{
		// yeah, we actually do depth write, but we need to have a separate flag for depth clear
		// (which is a different pipeline meaning we overwrite everyting)
		uint32_t clear_z_key = make_key(kSurfaceShaderClearDepth, kPipeBlendNo, !"DEPTH_WRITE", !"ALPHA_TEST");
		assert(g_ue_pipelines.count(clear_z_key) == 0);
		g_ue_pipelines.insert(std::make_pair(clear_z_key, g_fs_clear_pipeline));
	}

	// alpha test
	// dim1
	const SurfaceShader surface[] = { kSurfaceShaderComplex, kSurfaceShaderGouraud };
	const SShader* const shaders[] = { g_ue_complex_shader_alpha_test, g_ue_gouraud_shader_alpha_test };
	const RHIVertexInputState* const vi_states[] = { &ue_vi_complex_state, &ue_vi_gouraud_state};
	const IRHIPipelineLayout * const layouts[] = { ue_complex_pipeline_layout, ue_gouraud_pipeline_layout };
	// dim2
	const PipelineBlend blends[] = { kPipeBlendNo, kPipeBlendTranslucent };
	// dim3
	const RHIDepthStencilState* depth_states[] = { &ds_write_state, &ds_no_write_state };
	for (int d = 0; d < countof(depth_states); ++d) {
		for (int i = 0; i < countof(surface); ++i) {
			for (int b = 0; b < countof(blends); ++b) {
				IRHIGraphicsPipeline *pipeline = device->CreateGraphicsPipeline(
					shaders[i]->stages_, countof(shaders[i]->stages_), vi_states[i], &ue_ia_state,
					&viewport_state, &ue_raster_state, &ms_state, depth_states[d],
					g_blend_states[blends[b]], layouts[i], dyn_state, countof(dyn_state),
					g_main_pass);

				uint32_t alpha_test_key =
					make_key(surface[i], blends[b], "DEPTH_WRITE" && d == 0, "ALPHA_TEST");
				assert(g_ue_pipelines.count(alpha_test_key) == 0);
				g_ue_pipelines.insert(std::make_pair(alpha_test_key, pipeline));
			}
		}
	}

	// create pipelines for gouraud surfaces
	for (uint8_t j = 0; j < 2; j++) {
		const RHIDepthStencilState *depth_state = j ? &ds_write_state : &ds_no_write_state;
		for (uint8_t i = 0; i < kPipeBlendCount; i++) {
			IRHIGraphicsPipeline *pipeline = device->CreateGraphicsPipeline(
				g_ue_gouraud_shader->stages_, countof(g_ue_gouraud_shader->stages_),
				&ue_vi_gouraud_state, &ue_ia_state, &viewport_state, &ue_raster_state, &ms_state,
				depth_state, g_blend_states[i], ue_gouraud_pipeline_layout,
				dyn_state, countof(dyn_state), g_main_pass);

			uint32_t key = make_key(kSurfaceShaderGouraud, (PipelineBlend)i, j!=0, !"ALPHA TEST");
			assert(g_ue_pipelines.count(key) == 0);
			g_ue_pipelines.insert(std::make_pair(key, pipeline));
		}
	}

	

	if (!UVulkanRenderDevice::SetRes(NewX, NewY, NewColorBytes, Fullscreen)) {
		GError->Log(L"Init: SetRes failed.");
		return 0;
	}

#if 0
	//Brightness
	float brightness;
	GConfig->GetFloat(L"WinDrv.WindowsClient",L"Brightness",brightness);
	D3D::setBrightness(brightness);
#endif

	URenderDevice::PrecacheOnFlip = 1; //Turned on to immediately recache on init (prevents lack of textures after fullscreen switch)

	QueryPerformanceFrequency(&perfCounterFreq); //Init performance counter frequency.

	m_useZRangeHack = false;
	m_nearZRangeHackProjectionActive = false;

	return 1;
}

UBOOL UVulkanRenderDevice::SetRes(INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen)
{
	log_info("SetRes: %d x %d\n", NewX, NewY, Fullscreen);

	//Without BLIT_Direct3D major flickering occurs when switching from fullscreen to windowed.
	UBOOL Result = URenderDevice::Viewport->ResizeViewport(
		Fullscreen ? (BLIT_Fullscreen | BLIT_Direct3D) : (BLIT_HardwarePaint | BLIT_Direct3D), NewX,
		NewY, NewColorBytes);
	if (!Result) 
	{
		GError->Log(L"SetRes: Error resizing viewport.");
		return 0;
	}	

	if(!g_vulkan_device->OnWindowSizeChanged(NewX, NewY, Fullscreen!=0))
	//if(!D3D::resize(NewX,NewY,(Fullscreen!=0)))
	{
		GError->Log(L"SetRes: D3D::Resize failed.");
		return 0;
	}
	
	//Calculate new FOV. Is set, if needed, at frame start as game resets FOV on level load.
	int defaultFOV;
	#if(RUNE||DEUSEX)
		defaultFOV=75;
	#endif
	#if(UNREALGOLD||UNREALTOURNAMENT||NERF)
		defaultFOV=90;
	#endif
	customFOV = Misc::getFov(defaultFOV,Viewport->SizeX,Viewport->SizeY);

	return 1;
}

void UVulkanRenderDevice::Exit()
{
	for (int i = 0; i < kNumBufferedFrames; ++i) {
		g_ue_complex_dsets[i].clear();
		g_ue_complex_dsets_reserved[i] = 0;
		g_ue_gouraud_dsets[i].clear();
		g_ue_gouraud_dsets_reserved[i] = 0;
	};

	g_tex_upload_tasks.clear();
	g_tex_upload_in_progress.clear();
	texture_upload_task_fini();

	TextureCache::destroy(g_texCache);
	delete g_vulkan_device;
	g_ue_pipelines.clear();
	assert(g_draw_calls.size() == 0);
	//assert(g_gouraud_draw_calls.size() == 0);
	vulkan_finalize();
}


#if UNREALGOLD
void UVulkanRenderDevice::Flush()
{
}
#else
void UVulkanRenderDevice::Flush(UBOOL AllowPrecache)
{
	#if (!UNREALGOLD)
	if(AllowPrecache && options.precache)
		URenderDevice::PrecacheOnFlip = 1;
	#endif

	for (auto i = g_tex_upload_tasks.begin(); i < g_tex_upload_tasks.end(); ++i)
	{
		(*i)->release();
	}

	g_tex_upload_tasks.clear();
	g_tex_upload_in_progress.clear();
	TextureCache::destroy(g_texCache);
	g_texCache = TextureCache::makeCache();
}
#endif

/**
Clear screen and depth buffer, prepare buffers to receive data.
\param FlashScale To do with flash effects, see notes.
\param FlashFog To do with flash effects, see notes.
\param ScreenClear The color with which to clear the screen. Used for Rune fog.
\param RenderLockFlags Signify whether the screen should be cleared. Depth buffer should always be cleared.
\param InHitData Something to do with clipping planes; safe to ignore.
\param InHitSize Something to do with clipping planes; safe to ignore.

\note 'Flash' effects are fullscreen colorization, for example when the player is underwater (blue) or being hit (red).
Depending on the values of the related parameters (see source code) this should be drawn; the games don't always send a blank flash when none should be drawn.
EndFlash() ends this, but other renderers actually save the parameters and start drawing it there so it gets blended with the final scene.
\note RenderLockFlags aren't always properly set, this results in for example glitching in the Unreal castle flyover, in the wall of the tower with the Nali on it.
*/
int g_idx = 0;
int sanity_lock_cnt = 0;
void UVulkanRenderDevice::Lock(FPlane FlashScale, FPlane FlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* HitData, INT* HitSize)
{
	g_idx = 0;
	assert(0 == sanity_lock_cnt);

	log_info("Lock\n");

	float deltaTime;
	static LARGE_INTEGER oldTime;
	LARGE_INTEGER time;
	if(oldTime.QuadPart==0)
		QueryPerformanceCounter(&oldTime); //Initial time

	
	QueryPerformanceCounter(&time);
	deltaTime  =  (time.QuadPart-oldTime.QuadPart) / (float)perfCounterFreq.QuadPart;
	
	//FPS limiter
	if(options.FPSLimit > 0)
	{		
		while(deltaTime<(float)1/options.FPSLimit) //Busy wait for max accuracy
		{
			QueryPerformanceCounter(&time);
			deltaTime  =  (time.QuadPart-oldTime.QuadPart) / (float)perfCounterFreq.QuadPart;
		}		
	}
	oldTime.QuadPart = time.QuadPart;	
	

	//If needed, set new field of view; the game resets this on level switches etc. Can't be done in config as Unreal doesn't support this.
	if(options.autoFOV && Viewport->Actor->DefaultFOV!=customFOV)
	{		
		TCHAR buf[8]=L"fov ";
		_itow_s(customFOV,&buf[4],4,10);
		Viewport->Actor->DefaultFOV=(float)customFOV; //Do this so the value is set even if FOV settings don't take effect (multiplayer mode) 
		URenderDevice::Viewport->Exec(buf,*GLog); //And this so the FOV change actually happens				
	}

	if (options.ColorizeDetailTextures) {
		m_detailTextureColor4ub = 0x00408040;
	}
	else {
		m_detailTextureColor4ub = 0x00808080;
	}


	
	IRHIDevice* dev = g_vulkan_device;
	if (!dev->BeginFrame())
	{
		log_error("BeginFrame failed\n");
	}

	g_curFBIdx = (int)dev->GetCurrentSwapChainImageIndex();

	IRHICmdBuf* cb = g_cmdbuf[g_curFBIdx];

	cb->Begin();

	static float sec = 0.0f;
	sec += deltaTime;
	static vec4 color = vec4(1, 0, 0, 0);
	color.x = 0.5f*sinf(2 * 3.1415f*sec) + 0.5f;

	{
		if (!g_quad_vb_copy_event->IsSet(dev)) {
			cb->CopyBuffer(g_quad_gpu_vb, 0, g_quad_staging_vb, 0, g_quad_staging_vb->Size());
			cb->BufferBarrier(
				g_quad_gpu_vb, RHIAccessFlagBits::kTransferWrite, RHIPipelineStageFlags::kTransfer,
				RHIAccessFlagBits::kVertexAttributeRead, RHIPipelineStageFlags::kVertexInput);
			cb->SetEvent(g_quad_vb_copy_event, RHIPipelineStageFlags::kVertexInput);
		}

		if (!g_img_copy_event->IsSet(dev)) {
			cb->Barrier_UndefinedToTransfer(g_test_image);
			cb->CopyBufferToImage2D(g_test_image, g_img_staging_buf);
			cb->Barrier_TransferToShaderRead(g_test_image);
			cb->SetEvent(g_img_copy_event, RHIPipelineStageFlags::kFragmentShader);
		}

		static bool b_cube_copied = false;
		if (!b_cube_copied) {
			g_cube_vb->CopyToGPU(dev, cb);
			b_cube_copied = true;
		}

		update_uniform_staging_buf(g_curFBIdx, dev);

		// update uniforms device buffer (using g_curFBIdx for indexing, assume they are
		// synchronized and there are same numbers of both)
		cb->CopyBuffer(g_uniform_buffer, 0, g_uniform_staging_buffer[g_curFBIdx], 0, sizeof(PerFrameUniforms));
		cb->BufferBarrier(g_uniform_buffer, (uint32_t)RHIAccessFlagBits::kTransferWrite,
						  RHIPipelineStageFlags::kTransfer, (uint32_t)RHIAccessFlagBits::kUniformRead,
						  RHIPipelineStageFlags::kVertexShader);
		cb->SetEvent(g_uniform_buffer_copy_event, RHIPipelineStageFlags::kVertexShader);

	}


	sanity_lock_cnt++;
}

void UVulkanRenderDevice::Unlock(UBOOL Blit)
{
	IRHIDevice* dev = g_vulkan_device;
	assert(1 == sanity_lock_cnt);

	const uint32_t cur_swap_chain_img_idx = dev->GetCurrentSwapChainImageIndex();
	assert(cur_swap_chain_img_idx <= g_main_fb.size());
	IRHIFrameBuffer *cur_fb = g_main_fb[cur_swap_chain_img_idx];
	IRHIImageView* cur_ds = g_main_ds[cur_swap_chain_img_idx];

	IRHICmdBuf* cb = g_cmdbuf[g_curFBIdx];

	// fb_image should be same as corresponding colour attachment of framebuffer
	// GetSwapChainWidthHeight() ?
	const IRHIImage* fb_image = dev->GetCurrentSwapChainImage();

	for (int i = 0; i < g_tex_upload_tasks.size(); ++i) {
		TextureUploadTask* t = g_tex_upload_tasks[i];
		if (t->is_update)
			cb->Barrier_ShaderReadToTransfer(t->image);
		else
			cb->Barrier_UndefinedToTransfer(t->image);
		cb->CopyBufferToImage2D(t->image, t->img_staging_buf);
		cb->Barrier_TransferToShaderRead(t->image);
		cb->SetEvent(t->img_copy_event, RHIPipelineStageFlags::kFragmentShader);

		assert(g_tex_upload_in_progress.end() ==
			   std::find(g_tex_upload_in_progress.begin(), g_tex_upload_in_progress.end(), t));
		g_tex_upload_in_progress.push_back(t);
	}
	g_tex_upload_tasks.clear();


	const auto b = g_tex_upload_in_progress.begin();
	const auto e = g_tex_upload_in_progress.end();
	for (auto it = b; it != e; ++it) {
		if ((*it)->img_copy_event->IsSet(dev)) {
			(*it)->release();
			*it = nullptr;
		}
	}
	auto it = std::remove_if(b, e, [dev](const TextureUploadTask* t) { return t == nullptr; });
	g_tex_upload_in_progress.erase(it, e);

	if (!g_draw_calls.empty() /*|| !g_gouraud_draw_calls.empty()*/) {
		ue_update_per_frame_uniforms(g_curFBIdx, dev, m_detailTextureColor4ub);
	}

	if (!g_draw_calls.empty()) {
		// TODO: do not copy whole array! only actual filled frame data 
		g_ue_complex_vb[g_curFBIdx]->CopyToGPU(dev, cb);
		g_ue_complex_ib[g_curFBIdx]->CopyToGPU(dev, cb);
		g_ue_complex_vs_ub->buf[g_curFBIdx]->CopyToGPU(dev, cb);
	}

	//if (!g_gouraud_draw_calls.empty()) {
	if(g_ue_gouraud_ib_size[g_curFBIdx])
	{
		// TODO: do not copy whole array! only actual filled frame data 
		g_ue_gouraud_vb[g_curFBIdx]->CopyToGPU(dev, cb);
		g_ue_gouraud_ib[g_curFBIdx]->CopyToGPU(dev, cb);
		g_ue_gouraud_vs_ub->buf[g_curFBIdx]->CopyToGPU(dev, cb);
	}

	//cb->Barrier_PresentToClear(fb_image);
	//cb->Barrier_PresentToClear(cur_ds->GetImage());
	//vec4 color = vec4(1, 0, 0, 0);
	//cb->Clear(fb_image, color, (uint32_t)RHIImageAspectFlags::kColor, cur_ds->GetImage(), 0.0f, 0,
	//		  (uint32_t)RHIImageAspectFlags::kDepth);

	ivec4 render_area(0, 0, fb_image->Width(), fb_image->Height());
	RHIClearValue clear_values[] = { {vec4(0, 255, 0, 0), 0.0f, 0}, {vec4(0, 0, 0, 0), 0.0f, 0} };
	// it is important we pass fb corresponding to current swapchain index as we will be waiting on it in Present()
	cb->BeginRenderPass(g_main_pass, cur_fb, &render_area, clear_values, (uint32_t)countof(clear_values));

	RHIViewport viewport;
	viewport.x = FrameXB;
	viewport.y = FrameYB;
	viewport.width = FrameX;
	viewport.height = FrameY;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	const IRHIDescriptorSet* sets[] = { g_my_ds_set0, g_my_ds_set1 };
	if (0)
	{
		cb->BindDescriptorSets(RHIPipelineBindPoint::kGraphics, g_tri_pipeline->Layout(), sets,
			countof(sets), 0, nullptr);
		cb->BindPipeline(RHIPipelineBindPoint::kGraphics, g_tri_pipeline);
		cb->SetViewport(&viewport, 1);
		cb->Draw(3, 1, 0, 0);
	}

	// I think there is no need to wait as we schedule draw in the queue and all operations are
	// sequential there only necessary if we want to do something on a host, but let it be here as
	// an example
	// if (g_quad_vb_copy_event->IsSet(dev) && g_img_copy_event->IsSet(dev) &&
	// g_uniform_buffer_copy_event->IsSet(dev))
	if(0)
	{
		cb->BindDescriptorSets(RHIPipelineBindPoint::kGraphics, g_quad_pipeline->Layout(), sets,
							   countof(sets), 0, nullptr);
		cb->BindPipeline(RHIPipelineBindPoint::kGraphics, g_quad_pipeline);
		cb->SetViewport(&viewport, 1);
		cb->BindVertexBuffers(&g_quad_gpu_vb, 0, 1);
		cb->Draw(4, 1, 0, 0);
	}
	if(0)
	{
		cb->BindDescriptorSets(RHIPipelineBindPoint::kGraphics, g_world_model_pipeline->Layout(),
							   sets, countof(sets), 0, nullptr);
		cb->BindPipeline(RHIPipelineBindPoint::kGraphics, g_world_model_pipeline);
		cb->SetViewport(&viewport, 1);
		// cb->BindIndexBuffer(g_cube_ib->device_buf_, 0, RHIIndexType::kUint32);
		cb->BindVertexBuffers(&g_cube_vb->device_buf_, 0, 1);
		// cb->DrawIndexed(g_cube_ib->device_buf_->Size()/sizeof(uint32_t), 1, 0, 0, 0);
		cb->Draw(g_cube_vb->device_buf_->Size() / sizeof(SVD), 1, 0, 0);
	}

	if (!g_draw_calls.empty()) {


		const int num_draw_calls = (int)g_draw_calls.size();
		for (int i = 0; i < num_draw_calls; i++) {
			const ComplexSurfaceDrawCall& dc = g_draw_calls[i];

			//TODO: make this key where we fill drawcall struct?
			uint32_t key = make_key(dc.surface_shader, dc.pipeline_blend, dc.b_depth_write, dc.b_alpha_test);
			assert(g_ue_pipelines.count(key));
			IRHIGraphicsPipeline* pipeline = g_ue_pipelines[key];
			cb->BindPipeline(RHIPipelineBindPoint::kGraphics, pipeline);
			cb->SetViewport(&dc.viewport, 1);

			if (dc.dset) {

				const bool is_complex = dc.surface_shader == kSurfaceShaderComplex;

				if(is_complex) {
					cb->BindIndexBuffer(g_ue_complex_ib[g_curFBIdx]->device_buf_, 0, RHIIndexType::kUint32);
					cb->BindVertexBuffers(&g_ue_complex_vb[g_curFBIdx]->device_buf_, 0, 1);
				} else {
					cb->BindIndexBuffer(g_ue_gouraud_ib[g_curFBIdx]->device_buf_, 0, RHIIndexType::kUint32);
					cb->BindVertexBuffers(&g_ue_gouraud_vb[g_curFBIdx]->device_buf_, 0, 1);
				}

				RHIDescriptorWriteDesc desc_write_desc[5];
				RHIDescriptorWriteDescBuilder builder(desc_write_desc, countof(desc_write_desc));
				// TODO: move g_ue_per_frame_uniforms out of "for" loop
				builder.add(g_draw_calls[i].dset, 0, g_test_sampler,
							RHIImageLayout::kShaderReadOnlyOptimal, dc.diffuse);
				if (dc.lightmap) {
					builder.add(g_draw_calls[i].dset, 1, g_test_sampler,
								RHIImageLayout::kShaderReadOnlyOptimal, dc.lightmap);
				} 
				if (dc.detail) {
					builder.add(g_draw_calls[i].dset, 2, g_test_sampler,
								RHIImageLayout::kShaderReadOnlyOptimal, dc.detail);
				} 
				if (dc.macro) {
					builder.add(g_draw_calls[i].dset, 3, g_test_sampler,
								RHIImageLayout::kShaderReadOnlyOptimal, dc.detail);
				} 

				// TODO: have a separate set for this to not setup per draw call
				builder.add(g_draw_calls[i].dset, is_complex ? 4 : 1, g_ue_per_frame_uniforms[g_curFBIdx], 0,
							sizeof(UEPerFrameUniformBuf));
				dev->UpdateDescriptorSet(desc_write_desc, builder.cur_index);

				if (is_complex) {
					const IRHIDescriptorSet *sets[] = {dc.dset, g_ue_complex_vs_ub->dset[g_curFBIdx]};
					uint32_t dyn_offsets[] = {dc.vs_ub_idx * g_ue_complex_vs_ub->el_size};
					cb->BindDescriptorSets(RHIPipelineBindPoint::kGraphics, pipeline->Layout(),
										   sets, countof(sets), countof(dyn_offsets), dyn_offsets);
				} else {
					const IRHIDescriptorSet *sets[] = {dc.dset, g_ue_gouraud_vs_ub->dset[g_curFBIdx]};
					uint32_t dyn_offsets[] = {dc.vs_ub_idx * g_ue_gouraud_vs_ub->el_size};
					cb->BindDescriptorSets(RHIPipelineBindPoint::kGraphics, pipeline->Layout(),
										   sets, countof(sets), countof(dyn_offsets), dyn_offsets);
				}
			}

			if (dc.num_indices) {
				cb->DrawIndexed(dc.num_indices, 1, dc.ib_offset, 0, 0);
			} else {
				cb->Draw(dc.num_vertices, 1, dc.ib_offset, 0);
			}
		}
	}
#if 0
	if (!g_gouraud_draw_calls.empty()) {
		cb->BindIndexBuffer(g_ue_gouraud_ib[g_curFBIdx]->device_buf_, 0, RHIIndexType::kUint32);
		cb->BindVertexBuffers(&g_ue_gouraud_vb[g_curFBIdx]->device_buf_, 0, 1);

		const int num_draw_calls = (int)g_gouraud_draw_calls.size();
		for (int i = 0; i < num_draw_calls; i++) {
			const GouraudSurfaceDrawCall& dc = g_gouraud_draw_calls[i];

			//TODO: make this key where we fill drawcall struct?
			// TODO: only one pipeline for gouraud, maybe not store it in a map (even if we will have wireframe)
			uint32_t key = make_key(kSurfaceShaderGouraud, kPipeBlendNo, true, false, false);
			assert(g_ue_pipelines.count(key));
			IRHIGraphicsPipeline* pipeline = g_ue_pipelines[key];
			cb->BindPipeline(RHIPipelineBindPoint::kGraphics, pipeline);
			cb->SetViewport(&viewport, 1);

			if (dc.dset) {
				RHIDescriptorWriteDesc desc_write_desc[4];
				RHIDescriptorWriteDescBuilder builder(desc_write_desc, countof(desc_write_desc));
				// TODO: move g_ue_per_frame_uniforms out of "for" loop
				builder.add(g_gouraud_draw_calls[i].dset, 0, g_test_sampler,
							RHIImageLayout::kShaderReadOnlyOptimal, dc.diffuse);
				builder.add(g_gouraud_draw_calls[i].dset, 1, g_ue_per_frame_uniforms[g_curFBIdx], 0,
							sizeof(UEPerFrameUniformBuf));
				dev->UpdateDescriptorSet(desc_write_desc, builder.cur_index);

				cb->BindDescriptorSets(RHIPipelineBindPoint::kGraphics, pipeline->Layout(), &dc.dset,
									   1, 0, nullptr);
			}

			if (dc.num_indices) {
				cb->DrawIndexed(dc.num_indices, 1, dc.ib_offset, 0, 0);
			} else {
				cb->Draw(dc.num_vertices, 1, dc.ib_offset, 0);
			}
		}
	}
#endif

	cb->EndRenderPass(g_main_pass, cur_fb);
	cb->End();
	dev->Submit(cb, RHIQueueType::kGraphics);

	if (!g_vulkan_device->Present())
	{
		log_error("Present failed\n");
	}

	if (!g_vulkan_device->EndFrame())
	{
		log_error("EndFrame failed\n");
	}

	g_ue_complex_vb_size[g_curFBIdx] = 0;
	g_ue_complex_ib_size[g_curFBIdx] = 0;
	g_ue_complex_dsets_reserved[g_curFBIdx] = 0;
	g_ue_complex_vs_ub->size[g_curFBIdx] = 0;

	g_ue_gouraud_vb_size[g_curFBIdx] = 0;
	g_ue_gouraud_ib_size[g_curFBIdx] = 0;
	g_ue_gouraud_dsets_reserved[g_curFBIdx] = 0;
	g_ue_gouraud_vs_ub->size[g_curFBIdx] = 0;

	g_draw_calls.resize(0);

	sanity_lock_cnt--;
}


const IRHIImageView* GetCachedTexture(struct FTextureInfo *Texture, unsigned long PolyFlags, class IRHIDevice *dev, bool b_detail) {

	const IRHIImageView* rhi_texture = nullptr;
	if (!g_texCache->isCached(Texture->CacheID)) {
		TextureUploadTask* t;
		g_texCache->cache(Texture, PolyFlags, dev, &t);
		g_tex_upload_tasks.push_back(t);
		rhi_texture = t->img_view;
	} else {
		if (Texture->bRealtimeChanged) {
			TextureUploadTask* t;
			g_texCache->update(Texture, PolyFlags, dev, &t);
			g_tex_upload_tasks.push_back(t);
		}
		//Mask bit changed. Static texture, so must be deleted and recreated.
		else if (!b_detail && (PolyFlags & PF_Masked) != 0 && !g_texCache->isMasked(Texture->CacheID))
		{
			//assert(!"Handle this");
			//delete & recache
		}
			
		rhi_texture = g_texCache->get(Texture->CacheID);
	}
	
	//if (Texture->bRealtimeChanged) {
	//	assert(Texture->bRealtime);
	//}

	return rhi_texture;
}

/**
Complex surfaces are used for map geometry. They consist of facets which in turn consist of polys (triangle fans).
\param Frame The scene. See SetSceneNode().
\param Surface Holds information on the various texture passes and the surface's PolyFlags.
	- PolyFlags contains the correct flags for this surface. See polyflags.h
	- Texture is the diffuse texture.
	- DetailTexture is the nice close-up detail that's modulated with the diffuse texture for walls. It's up to the renderer to only draw these on near surfaces.
	- LightMap is the precalculated map lighting. Should be drawn with a -.5 pan offset.
	- FogMap is precalculated fog. Should be drawn with a -.5 pan offset. Should be added, not modulated. Flags determine if it should be applied, see polyflags.h.
	- MacroTexture is similar to a detail texture but for far away surfaces. Rarely used.
\param Facet Contains coordinates and polygons.
	- MapCoords are used to calculate texture coordinates. Involved. See code.
	- Polys is a linked list of triangle fan arrays; each element is similar to the models used in DrawGouraudPolygon().
	
\note DetailTexture and FogMap are mutually exclusive; D3D10 renderer just uses seperate binds for them anyway.
\note D3D10 renderer handles DetailTexture range in shader.
\note Check if submitted polygons are valid (3 or more points).
*/
void UVulkanRenderDevice::DrawComplexSurface(FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet)
{
	assert(1 == sanity_lock_cnt);
	check(Surface.Texture);

	const IRHIImageView *rhi_macro = nullptr;
	const IRHIImageView *rhi_lightmap = nullptr;
	const IRHIImageView *rhi_detail = nullptr;
	const IRHIImageView *rhi_fog = nullptr;
	const IRHIImageView *rhi_diffuse =
		GetCachedTexture(Surface.Texture, Surface.PolyFlags, g_vulkan_device, false);

	if (Surface.MacroTexture) {
		rhi_macro = GetCachedTexture(Surface.MacroTexture, Surface.PolyFlags, g_vulkan_device, false);
		check(rhi_macro);
	}

	if (Surface.LightMap) {
		rhi_lightmap = GetCachedTexture(Surface.LightMap, Surface.PolyFlags, g_vulkan_device, false);
		check(rhi_lightmap);
	}

	if (Surface.FogMap) {
		rhi_fog = GetCachedTexture(Surface.FogMap, Surface.PolyFlags, g_vulkan_device, false);
		check(rhi_fog);
	}

	//See if detail texture should be drawn
	//FogMap and DetailTexture are mutually exclusive effects
	// TODO: can add early rejection of detail texture if all vertices in draw call are too far, but probably not necessary
	bool drawDetailTexture = false;
	if ((DetailTextures != 0) && Surface.DetailTexture && !Surface.FogMap) {
		drawDetailTexture = true;
	}

	if (drawDetailTexture && Surface.DetailTexture) {
		rhi_detail = GetCachedTexture(Surface.DetailTexture, Surface.PolyFlags, g_vulkan_device, true);
		check(rhi_detail);
	}

	//Calculate UDot and VDot intermediates for complex surface
	FLOAT UDot = Facet.MapCoords.XAxis | Facet.MapCoords.Origin;
	FLOAT VDot = Facet.MapCoords.YAxis | Facet.MapCoords.Origin;

	const float UMult = 1.0f / (Surface.Texture->UScale * Surface.Texture->USize);
	const float VMult = 1.0f / (Surface.Texture->VScale * Surface.Texture->VSize);
	float LM_UMult = 0, LM_VMult = 0;
	float Det_UMult = 0, Det_VMult = 0;
	float Macro_UMult = 0, Macro_VMult = 0;

	uint32_t Flags = Surface.PolyFlags;

	uint32_t& cur_vb_idx = g_ue_complex_vb_size[g_curFBIdx];
	uint32_t& cur_ib_idx = g_ue_complex_ib_size[g_curFBIdx];

	
	uint32_t& cur_vs_data_idx = g_ue_complex_vs_ub->size[g_curFBIdx];
	BufferView<UEPerDrawCallComplexVsData> vs_uniforms(
		g_ue_complex_vs_ub->buf[g_curFBIdx]->getMappedPtr(), g_ue_complex_vs_ub->el_size,
		g_ue_complex_vs_ub->num_el);

	vs_uniforms[cur_vs_data_idx].XAxis_UDot = vec4(*(vec3 *)&Facet.MapCoords.XAxis.X, UDot);
	vs_uniforms[cur_vs_data_idx].YAxis_VDot = vec4(*(vec3 *)&Facet.MapCoords.YAxis.X, VDot);
	vs_uniforms[cur_vs_data_idx].Diffuse_PanXY_UVMult =
		vec4(Surface.Texture->Pan.X, Surface.Texture->Pan.Y, UMult, VMult);

	if (rhi_macro) {
		float UScale = Surface.MacroTexture->UScale;
		float VScale = Surface.MacroTexture->VScale;
		float USize = Surface.MacroTexture->USize;
		float VSize = Surface.MacroTexture->VSize;
		Macro_UMult = 1.0f / (UScale * USize);
		Macro_VMult = 1.0f / (VScale * VSize);
		vs_uniforms[cur_vs_data_idx].Macro_PanXY_UVMult =
			vec4(Surface.MacroTexture->Pan.X, Surface.MacroTexture->Pan.Y, Macro_UMult, Macro_VMult);
		vs_uniforms[cur_vs_data_idx].HasMacro_UVScale = vec4(1, UScale, VScale, 0);
	} else {
		vs_uniforms[cur_vs_data_idx].HasMacro_UVScale.x = 0;
	}

	if (rhi_lightmap) {
		float UScale = Surface.LightMap->UScale;
		float VScale = Surface.LightMap->VScale;
		float USize = Surface.LightMap->USize;
		float VSize = Surface.LightMap->VSize;
		LM_UMult = 1.0f / (UScale * USize);
		LM_VMult = 1.0f / (VScale * VSize);
		vs_uniforms[cur_vs_data_idx].Lightmap_PanXY_UVMult =
			vec4(Surface.LightMap->Pan.X, Surface.LightMap->Pan.Y, LM_UMult, LM_VMult);
		vs_uniforms[cur_vs_data_idx].HasLightmap_UVScale = vec4(1, UScale, VScale, 0);
	} else {
		vs_uniforms[cur_vs_data_idx].HasLightmap_UVScale.x = 0;
	}

	if (rhi_detail || rhi_fog) {
		FTextureInfo* Tex = rhi_fog ? Surface.FogMap : Surface.DetailTexture;
		float UScale = Tex->UScale;
		float VScale = Tex->VScale;
		float USize = Tex->USize;
		float VSize = Tex->VSize;
		Det_UMult = 1.0f / (UScale * USize);
		Det_VMult = 1.0f / (VScale * VSize);
		vs_uniforms[cur_vs_data_idx].Detail_PanXY_UVMult =
			vec4(Tex->Pan.X, Tex->Pan.Y, Det_UMult, Det_VMult);
		vs_uniforms[cur_vs_data_idx].HasDetail_UVScale = vec4(1, UScale, VScale, rhi_fog?1:0);
	} else {
		vs_uniforms[cur_vs_data_idx].HasDetail_UVScale.x = 0;
	}

	cur_vs_data_idx++;

	//Draw each polygon
	for(FSavedPoly* Poly=Facet.Polys; Poly; Poly=Poly->Next )
	{
		const uint32_t vb_offset = cur_vb_idx;
		const uint32_t ib_offset = cur_ib_idx;
		
		if(Poly->NumPts < 3) {
			log_info("Invalid polygon");
			continue;
		}

		UEVertexComplex* VB = (UEVertexComplex*)g_ue_complex_vb[g_curFBIdx]->getMappedPtr();
		uint32_t* IB = (uint32_t*)g_ue_complex_ib[g_curFBIdx]->getMappedPtr();

		const int32_t num_verts = Poly->NumPts;
		const int32_t num_indices_for_poly_fan = (num_verts - 2) * 3;

		assert(cur_ib_idx + num_indices_for_poly_fan < gUENumIndices);
		assert(cur_vb_idx + Poly->NumPts < gUENumVert);

		// Generate fan indices
		for (int i = 1; i < num_verts - 1; i++) {
			IB[cur_ib_idx++] = cur_vb_idx ; // Center point
			IB[cur_ib_idx++] = cur_vb_idx + i;
			IB[cur_ib_idx++] = cur_vb_idx + i + 1;
		}

		// Generate fan vertices 
		for (INT i = 0; i < num_verts; i++)
		{
			UEVertexComplex* v = VB + cur_vb_idx++;
			
			// TODO: this can be doen in VS!
			FLOAT U = Facet.MapCoords.XAxis | Poly->Pts[i]->Point;
			FLOAT V = Facet.MapCoords.YAxis | Poly->Pts[i]->Point;
			FLOAT UCoord = U-UDot;
			FLOAT VCoord = V-VDot;

			//v->Flags = Flags;
			v->Pos = *(vec3*)&Poly->Pts[i]->Point.X; //Position
		}

		if(!(Flags & (PF_Translucent|PF_Modulated))) //If none of these flags, occlude (opengl renderer)
		{
			Flags |= PF_Occlude;
		}
		
		// TODO: take into account if necessary
		// setup proper samplers for those
		//desiredDynamicTexBits = (PolyFlags & PF_NoSmooth) ? DT_NO_SMOOTH_BIT : 0;
		//UBOOL SkipMipmaps = (Info.NumMips == 1);
		

		// TODO: forget about tri fans, move to triangles and have one draw call per Facet! and not per poly
		ComplexSurfaceDrawCall dc;
		dc.pipeline_blend = select_blend(Flags);
		dc.b_depth_write = select_depth_write(Flags);
		//dc.b_depth_clear = 0;
		// TODO: if masked we should use DepthEqual because triangles are drawn on top of something
		// which has been already drawn (see D3D9 renderer)
		dc.b_alpha_test = Flags & PF_Masked;

		// either alpha test or blend
		// apparently alpha + blend is also ok (happend on dm-barricade)
		//assert((!dc.b_alpha_test && kPipeBlendNo == dc.pipeline_blend) ||
		//	   (dc.b_alpha_test ^ (!!dc.pipeline_blend)));

		if(dc.b_alpha_test && kPipeBlendNo != dc.pipeline_blend) {
			log_info("alpha test + alpha blend");
		}
		dc.surface_shader = kSurfaceShaderComplex;
		dc.vb_offset = vb_offset;
		dc.num_vertices = cur_vb_idx - vb_offset;
		dc.ib_offset = ib_offset;
		dc.num_indices = cur_ib_idx - ib_offset;
		// should always equal to dc index in g_draw_calls array
		// (however we have ClearZ which also adds draw call, so indices may be shifted, so let's
		// have it for now)
		dc.vs_ub_idx = cur_vs_data_idx-1;
		dc.diffuse = rhi_diffuse;
		dc.lightmap = rhi_lightmap;
		assert((0==rhi_detail && 0==rhi_fog) || (!!rhi_fog ^ !!rhi_detail));
		dc.detail = rhi_detail ? rhi_detail : rhi_fog;
		dc.macro = rhi_macro;
		dc.viewport = g_current_viewport;
		// TODO: rework this to a simple free list of dsets
		if (g_ue_complex_dsets[g_curFBIdx].size() == (size_t)g_ue_complex_dsets_reserved[g_curFBIdx]) {
			g_ue_complex_dsets[g_curFBIdx].push_back(
				g_vulkan_device->AllocateDescriptorSet(g_ue_dsl_complex));
		}
		dc.dset = g_ue_complex_dsets[g_curFBIdx][g_ue_complex_dsets_reserved[g_curFBIdx]];
		g_ue_complex_dsets_reserved[g_curFBIdx]++;
		g_draw_calls.emplace_back(dc);

	//	log_info("i: %d flags: %x depth write: %d\n", idx, Flags, dc.b_depth_write);
		//log_info("Complex\n");
	}
}

/**
Gouraud shaded polygons are used for 3D models and surprisingly decals and shadows. 
They are sent with a call of this function per triangle fan, worldview transformed and lit. They do have normals and texture coordinates (no panning).
\param Frame The scene. See SetSceneNode().
\param Info The texture for the model. Models only come with diffuse textures.
\param Pts A triangle fan stored as an array. Each element has a normal, light (i.e. color) and fog (color due to being in fog).
\param NumPts Number of verts in fan.
\param PolyFlags Contains the correct flags for this model. See polyflags.h
\param Span Probably for software renderers.

\note Modulated models (i.e. shadows) shouldn't have a color, and fog should only be applied to models with the correct flags for that. The D3D10 renderer handles this in the shader.
\note Check if submitted polygons are valid (3 or more points).
*/

void UVulkanRenderDevice::DrawGouraudPolygon(FSceneNode *Frame, FTextureInfo &Info,
											 FTransTexture **Pts, int NumPts, DWORD PolyFlags,
											 FSpanBuffer *Span) {

	if (NumPts < 3) {
		log_info("Invalid polygon");
		return;
	}

	const IRHIImageView *rhi_diffuse = GetCachedTexture(&Info, PolyFlags, g_vulkan_device, false);

	const BYTE NoFlags = 0;
	const BYTE ColorFlags = 1;
	const BYTE FogFlags = 2;
	BYTE requestedColorFlags = NoFlags;

	// Check if should render fog and if vertex specular is supported
	// Also set other color flags
	if (PolyFlags & PF_Modulated) {
		requestedColorFlags = 0;
	} else {
		requestedColorFlags = ColorFlags;
#ifdef UTGLR_RUNE_BUILD
			if (((PolyFlags & (PF_RenderFog | PF_Translucent | PF_Modulated | PF_AlphaBlend)) == PF_RenderFog)/* && UseVertexSpecular*/) {
#else
			if (((PolyFlags & (PF_RenderFog | PF_Translucent | PF_Modulated)) == PF_RenderFog) /*&& UseVertexSpecular*/) {
#endif
				requestedColorFlags = ColorFlags | FogFlags;
			}
	}

	// If not drawing fog, disable the PF_RenderFog flag
	if (!(requestedColorFlags & FogFlags)) {
		PolyFlags &= ~PF_RenderFog;
	}

	if (!(PolyFlags & (PF_Translucent | PF_Modulated |
					   PF_Highlighted))) // If none of these flags, occlude (opengl renderer)
	{
		PolyFlags |= PF_Occlude;
	} else if (PolyFlags & PF_Translucent) {
		PolyFlags &= ~PF_Masked;
	}


	const float UMult = 1.0f / (Info.UScale * Info.USize);
	const float VMult = 1.0f / (Info.VScale * Info.VSize);

	uint32_t& cur_vs_data_idx = g_ue_gouraud_vs_ub->size[g_curFBIdx];
	BufferView<UEPerDrawCallGouraudVsData> vs_uniforms(
		g_ue_gouraud_vs_ub->buf[g_curFBIdx]->getMappedPtr(), g_ue_gouraud_vs_ub->el_size,
		g_ue_gouraud_vs_ub->num_el);
	vs_uniforms[cur_vs_data_idx++].proj = g_current_projection;

	uint32_t& cur_vb_idx = g_ue_gouraud_vb_size[g_curFBIdx];
	uint32_t& cur_ib_idx = g_ue_gouraud_ib_size[g_curFBIdx];
	const uint32_t vb_offset = cur_vb_idx;
	const uint32_t ib_offset = cur_ib_idx;

	UEVertexGouraud* VB = (UEVertexGouraud*)g_ue_gouraud_vb[g_curFBIdx]->getMappedPtr();
	uint32_t* IB = (uint32_t*)g_ue_gouraud_ib[g_curFBIdx]->getMappedPtr();

	const int32_t num_verts = NumPts;
	const int32_t num_indices_for_poly_fan = (num_verts - 2) * 3;

	assert(cur_ib_idx + num_indices_for_poly_fan < gUENumIndices);
	assert(cur_vb_idx + NumPts < gUENumVert);

	// Generate fan indices
	for (int i = 1; i < num_verts - 1; i++) {
		IB[cur_ib_idx++] = cur_vb_idx; // Center point
		IB[cur_ib_idx++] = cur_vb_idx + i;
		IB[cur_ib_idx++] = cur_vb_idx + i + 1;
	}

	// Generate fan vertices
	for (INT i = 0; i < num_verts; i++) {
		UEVertexGouraud* v = VB + cur_vb_idx++;
		v->Pos = *(vec3 *)&Pts[i]->Point.X; // Position
		v->TexCoord.x = Pts[i]->U * UMult;
		v->TexCoord.y = Pts[i]->V * VMult;
		v->TexCoord.z = (float)g_idx;
		
		if(requestedColorFlags & FogFlags) {
			FLOAT f255_Times_One_Minus_FogW = 255.0f * (1.0f - Pts[i]->Fog.W);
			v->Color = FPlaneTo_BGRScaled_A255(&Pts[i]->Light, f255_Times_One_Minus_FogW);
			v->FogColor = FPlaneTo_BGR_A0(&Pts[i]->Fog);
		} else if(requestedColorFlags & ColorFlags) {
			v->Color = FPlaneTo_BGR_A0(&Pts[i]->Light);
			v->FogColor = 0;
		} else {
			v->Color = 0xFFFFFFFF;
			v->FogColor = 0;
		}
	}

	ComplexSurfaceDrawCall dc;
	dc.pipeline_blend = select_blend(PolyFlags);
	dc.b_depth_write = select_depth_write(PolyFlags);
	//dc.b_depth_clear = 0;
	// TODO: if masked we should use DepthEqual because triangles are drawn on top of something
	// which has been already drawn (see D3D9 renderer)
	dc.b_alpha_test = PolyFlags & PF_Masked;

	// either alpha test or blend
	assert((!dc.b_alpha_test && kPipeBlendNo == dc.pipeline_blend) ||
		   (dc.b_alpha_test ^ (!!dc.pipeline_blend)));

	dc.surface_shader = kSurfaceShaderGouraud;
	dc.vb_offset = vb_offset;
	dc.num_vertices = cur_vb_idx - vb_offset;
	dc.ib_offset = ib_offset;
	dc.num_indices = cur_ib_idx - ib_offset;
	dc.diffuse = rhi_diffuse;
	dc.detail = nullptr;
	dc.lightmap = nullptr;
	dc.macro = nullptr;
	dc.vs_ub_idx = cur_vs_data_idx-1;
	dc.viewport = g_current_viewport;
	// TODO: rework this to a simple free list of dsets
	if (g_ue_gouraud_dsets[g_curFBIdx].size() == (size_t)g_ue_gouraud_dsets_reserved[g_curFBIdx]) {
		g_ue_gouraud_dsets[g_curFBIdx].push_back(
			g_vulkan_device->AllocateDescriptorSet(g_ue_dsl_gouraud));
	}
	dc.dset = g_ue_gouraud_dsets[g_curFBIdx][g_ue_gouraud_dsets_reserved[g_curFBIdx]];
	g_ue_gouraud_dsets_reserved[g_curFBIdx]++;
	//g_gouraud_draw_calls.emplace_back(dc);
	g_draw_calls.emplace_back(dc);

	//log_info("i: %d flags: %x depth write: %d pipe_bled: %d \n", g_idx, PolyFlags, dc.b_depth_write, dc.pipeline_blend);
	g_idx++;

}
void UVulkanRenderDevice::DrawTile(FSceneNode *Frame, FTextureInfo &Info, FLOAT X, FLOAT Y,
								   FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL,
								   class FSpanBuffer *Span, FLOAT Z, FPlane Color, FPlane Fog,
								   DWORD PolyFlags) {
	FLOAT PX1 = X - Frame->FX2;
	FLOAT PX2 = PX1 + XL;
	FLOAT PY1 = Y - Frame->FY2;
	FLOAT PY2 = PY1 + YL;

	//FLOAT One_Over_FX = 1.0f / Frame->FX;
	float RFX2 = m_RFX2;
	float RFY2 = m_RFY2;

	FLOAT RPX1 = RFX2 * PX1;
	FLOAT RPX2 = RFX2 * PX2;
	FLOAT RPY1 = RFY2 * PY1;
	FLOAT RPY2 = RFY2 * PY2;
	if (!Frame->Viewport->IsOrtho()) {
		RPX1 *= Z;
		RPX2 *= Z;
		RPY1 *= Z;
		RPY2 *= Z;
	}

	const IRHIImageView *rhi_diffuse = GetCachedTexture(&Info, PolyFlags, g_vulkan_device, false);

#ifdef UTGLR_RUNE_BUILD
	if (Info.Palette && Info.Palette[128].A != 255 &&
		!(PolyFlags & (PF_Translucent | PF_AlphaBlend))) {
#else
	if (Info.Palette && Info.Palette[128].A != 255 && !(PolyFlags & PF_Translucent)) {
#endif
		PolyFlags |= PF_Highlighted | PF_Occlude;
	}

	DWORD tileColor;
	tileColor = 0xFFFFFFFF;

	if (!(PolyFlags & PF_Modulated)) {
#ifdef UTGLR_RUNE_BUILD
		if (PolyFlags & PF_AlphaBlend) {
			Color.W = Info.Texture->Alpha;
			tileColor = FPlaneTo_BGRAClamped(&Color);
		} else {
			tileColor = FPlaneTo_BGRClamped_A255(&Color);
		}
#else
		tileColor = FPlaneTo_BGRClamped_A255(&Color);
#endif
	}

	FLOAT TexInfoUMult = 1.0f / (Info.UScale * Info.USize);
	FLOAT TexInfoVMult = 1.0f / (Info.VScale * Info.VSize);

	FLOAT SU1 = (U) * TexInfoUMult;
	FLOAT SU2 = (U + UL) * TexInfoUMult;
	FLOAT SV1 = (V) * TexInfoVMult;
	FLOAT SV2 = (V + VL) * TexInfoVMult;

	uint32_t& cur_vs_data_idx = g_ue_gouraud_vs_ub->size[g_curFBIdx];
	BufferView<UEPerDrawCallGouraudVsData> vs_uniforms(
		g_ue_gouraud_vs_ub->buf[g_curFBIdx]->getMappedPtr(), g_ue_gouraud_vs_ub->el_size,
		g_ue_gouraud_vs_ub->num_el);
	vs_uniforms[cur_vs_data_idx++].proj = g_current_projection;

	uint32_t& cur_vb_idx = g_ue_gouraud_vb_size[g_curFBIdx];
	uint32_t& cur_ib_idx = g_ue_gouraud_ib_size[g_curFBIdx];
	const uint32_t vb_offset = cur_vb_idx;
	const uint32_t ib_offset = cur_ib_idx;

	// TODO: can use special quad shader which will calculate texcoords from vertex_id % 4
	UEVertexGouraud* VB = (UEVertexGouraud*)g_ue_gouraud_vb[g_curFBIdx]->getMappedPtr();
	uint32_t* IB = (uint32_t*)g_ue_gouraud_ib[g_curFBIdx]->getMappedPtr();

	const int32_t num_verts = 4;
	const int32_t num_indices_for_poly_fan = (num_verts - 2) * 3;

	assert(cur_ib_idx + num_indices_for_poly_fan < gUENumIndices);
	assert(cur_vb_idx + num_verts < gUENumVert);

	// Generate fan indices
	for (int i = 1; i < num_verts - 1; i++) {
		IB[cur_ib_idx++] = cur_vb_idx; // Center point
		IB[cur_ib_idx++] = cur_vb_idx + i;
		IB[cur_ib_idx++] = cur_vb_idx + i + 1;
	}

	// Generate fan vertices
	{
		UEVertexGouraud* v0 = VB + cur_vb_idx++;
		UEVertexGouraud* v1 = VB + cur_vb_idx++;
		UEVertexGouraud* v2 = VB + cur_vb_idx++;
		UEVertexGouraud* v3 = VB + cur_vb_idx++;

		v0->TexCoord.x = SU1;
		v0->TexCoord.y = SV1;

		v1->TexCoord.x = SU2;
		v1->TexCoord.y = SV1;

		v2->TexCoord.x = SU2;
		v2->TexCoord.y = SV2;

		v3->TexCoord.x = SU1;
		v3->TexCoord.y = SV2;

		v0->Pos.x = RPX1;
		v0->Pos.y = RPY1;
		v0->Pos.z = Z;
		v0->Color = tileColor;
		v0->FogColor = 0;

		v1->Pos.x = RPX2;
		v1->Pos.y = RPY1;
		v1->Pos.z = Z;
		v1->Color = tileColor;
		v1->FogColor = 0;

		v2->Pos.x = RPX2;
		v2->Pos.y = RPY2;
		v2->Pos.z = Z;
		v2->Color = tileColor;
		v2->FogColor = 0;

		v3->Pos.x = RPX1;
		v3->Pos.y = RPY2;
		v3->Pos.z = Z;
		v3->Color = tileColor;
		v3->FogColor = 0;
	}

	ComplexSurfaceDrawCall dc;
	dc.pipeline_blend = select_blend(PolyFlags);
	dc.b_depth_write = select_depth_write(PolyFlags);
	//dc.b_depth_clear = 0;
	// TODO: if masked we should use DepthEqual because triangles are drawn on top of something
	// which has been already drawn (see D3D9 renderer)
	dc.b_alpha_test = PolyFlags & PF_Masked;
	dc.surface_shader = kSurfaceShaderGouraud;
	dc.vb_offset = vb_offset;
	dc.num_vertices = cur_vb_idx - vb_offset;
	dc.ib_offset = ib_offset;
	dc.num_indices = cur_ib_idx - ib_offset;
	dc.diffuse = rhi_diffuse;
	dc.detail = nullptr;
	dc.lightmap = nullptr;
	dc.macro = nullptr;
	dc.vs_ub_idx = cur_vs_data_idx-1;
	dc.viewport = g_current_viewport;
	// TODO: rework this to a simple free list of dsets
	if (g_ue_gouraud_dsets[g_curFBIdx].size() == (size_t)g_ue_gouraud_dsets_reserved[g_curFBIdx]) {
		g_ue_gouraud_dsets[g_curFBIdx].push_back(
			g_vulkan_device->AllocateDescriptorSet(g_ue_dsl_gouraud));
	}
	dc.dset = g_ue_gouraud_dsets[g_curFBIdx][g_ue_gouraud_dsets_reserved[g_curFBIdx]];
	g_ue_gouraud_dsets_reserved[g_curFBIdx]++;
	//g_gouraud_draw_calls.emplace_back(dc);
	g_draw_calls.emplace_back(dc);

	g_idx++;

	//log_info("DrawTile");

}
void UVulkanRenderDevice::Draw2DLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2)
{
}
void UVulkanRenderDevice::Draw2DPoint(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2, FLOAT Z)
{
}
void UVulkanRenderDevice::ClearZ(FSceneNode* Frame)
{
	ComplexSurfaceDrawCall dc;
	dc.pipeline_blend = kPipeBlendNo;
	dc.b_depth_write = 0;
	//dc.b_depth_clear = true;
	dc.b_alpha_test = false;
	dc.surface_shader = kSurfaceShaderClearDepth;

	dc.viewport = g_current_viewport;
	dc.vb_offset = 0;
	dc.num_vertices = 6;
	dc.ib_offset = 0;
	dc.num_indices = 0;
	dc.diffuse = nullptr;
	dc.lightmap = nullptr;
	dc.detail = nullptr;
	dc.macro = nullptr;
	dc.dset = nullptr;
	g_draw_calls.emplace_back(dc);

	log_info("ClearZ");
}
void UVulkanRenderDevice::PushHit(const BYTE* Data, INT Count)
{
}
void UVulkanRenderDevice::PopHit(INT Count, UBOOL bForce)
{
}
void UVulkanRenderDevice::GetStats(TCHAR* Result)
{
}
void UVulkanRenderDevice::ReadPixels(FColor* Pixels)
{
}

/* Optional but implemented */

UBOOL UVulkanRenderDevice::Exec(const TCHAR* Cmd, FOutputDevice& Ar)
{
		//First try parent
	//wchar_t* ptr;
	#if (!UNREALGOLD && !NERF)
	if(URenderDevice::Exec(Cmd,Ar))
	{
		return 1;
	}
	else
	#endif
	if(ParseCommand(&Cmd,L"GetRes"))
	{
		log_info("Getting modelist...\n");
		TCHAR* resolutions = TEXT("ASFAS");// D3D::getModes();
		Ar.Log(resolutions);
		//delete [] resolutions;
		log_info("Done.\n");
		return 1;
	}	
	/*else if((ptr=(wchar_t*)wcswcs(Cmd,L"Brightness"))) //Brightness is sent as "brightness [val]".
	{
		UD3D10RenderDevice::debugs("Setting brightness.");
		if((ptr=wcschr(ptr,' ')))//Search for space after 'brightness'
		{
			float b;
			b=_wtof(ptr); //Get brightness value;
			D3D::setBrightness(b);
		}
	}*/
	return 0;
}

/**
This optional function can be used to set the frustum and viewport parameters per scene change instead of per drawXXXX() call.
\param Frame Contains various information with which to build frustum and viewport.
\note Standard Z parameters: near 1, far 32760.
*/
void UVulkanRenderDevice::SetSceneNode(FSceneNode* Frame)
{
	//Calculate projection parameters 
	FLOAT One_Over_FX = 1.0f / Frame->FX;
	m_Aspect = Frame->FY*One_Over_FX;
	m_Fov = Viewport->Actor->FovAngle * PI / 180.0f;
	m_RProjZ = appTan(m_Fov/2.0f);

	FrameX = (float)Frame->X;
	FrameY = (float)Frame->Y;
	FrameFX = (float)Frame->FX;
	FrameFY = (float)Frame->FY;
	FrameXB = (float)Frame->XB;
	FrameYB = (float)Frame->YB;
	m_RFX2 = 2.0f * m_RProjZ * One_Over_FX;
	m_RFY2 = 2.0f * m_RProjZ * One_Over_FX;

	log_info("SetSceneNode: FX:%f FY:%f X:%d Y:%d XB:%d YB:%d zNear: %f zFar: %f Fov: %f\n", Frame->FX, Frame->FY,
			 Frame->X, Frame->Y, Frame->XB, Frame->YB, zNear, zFar, Viewport->Actor->FovAngle);
#if 1
	if (Frame->Viewport->IsOrtho()) {
		SetOrthoProjection();
	} else {
		SetProjection(false);
	}
	#endif

	g_current_viewport.x =  Frame->XB;
	g_current_viewport.y =  Frame->YB;
	g_current_viewport.width = Frame->X;
	g_current_viewport.height = Frame->Y;
	g_current_viewport.minDepth = 0.0f;
	g_current_viewport.maxDepth = 1.0f;

	// Viewport is set here as it changes during gameplay. For example in DX conversations
 	//D3D::setViewPort(Frame->X,Frame->Y,Frame->XB,Frame->YB); 

	//shader_GouraudPolygon->setViewportSize(Frame->X,Frame->Y);	//Shared by all Unreal shaders
	//shader_GouraudPolygon->setProjection(aspect,RProjZ,zNear,zFar);	//Shared by all Unreal shaders
}

/**
Store a texture in the renderer-kept texture cache. Only called by the game if URenderDevice::PrecacheOnFlip is 1.
\param Info Texture (meta)data. Includes a CacheID with which to index.
\param PolyFlags Contains the correct flags for this texture. See polyflags.h

\note Already cached textures are skipped, unless it's a dynamic texture, in which case it is updated.
\note Extra care is taken to recache textures that aren't saved as masked, but now have flags indicating they should be (masking is not always properly set).
	as this couldn't be anticipated in advance, the texture needs to be deleted and recreated.
*/
void UVulkanRenderDevice::PrecacheTexture(FTextureInfo& Info, DWORD PolyFlags)
{
}
void UVulkanRenderDevice::EndFlash()
{
}

#if (RUNE)
void UVulkanRenderDevice::DrawFogSurface(FSceneNode* Frame, FFogSurf &FogSurf)
{
}
void UVulkanRenderDevice::PreDrawGouraud(FSceneNode *Frame, FLOAT FogDistance, FPlane FogColor)
{
}
void UVulkanRenderDevice::PostDrawGouraud(FLOAT FogDistance)
{
}
#endif 

void UVulkanRenderDevice::OnSwapChainRecreated(void* user_ptr) {
	UVulkanRenderDevice* vrd = (UVulkanRenderDevice*)(user_ptr);
	(void)vrd;
	IRHIDevice* dev = g_vulkan_device;

	log_info("recreating frame buffers, because swap chain was recreated\n");
	for (size_t i = 0; i < g_main_fb.size(); ++i) {
		g_main_fb[i]->Destroy(dev);
		g_main_fb[i] = nullptr;

		g_main_ds[i]->GetImage()->Destroy(dev);
		g_main_ds[i]->Destroy(dev);
	}

	for (size_t i = 0; i < g_main_fb.size(); ++i) {
		IRHIImageView *view = dev->GetSwapChainImageView(i);
		const IRHIImage *image = dev->GetSwapChainImage(i);
		IRHIImageView* ds_view = create_depth(dev, image->Width(), image->Height());
		g_main_ds[i] = ds_view;

		IRHIImageView* att_arr[] = { view, ds_view };

		RHIFrameBufferDesc fb_desc;
		fb_desc.attachmentCount = countof(att_arr);
		fb_desc.pAttachments = att_arr;
		fb_desc.width_ = image->Width();
		fb_desc.height_ = image->Height();
		fb_desc.layers_ = 1;
		g_main_fb[i] = dev->CreateFrameBuffer(&fb_desc, g_main_pass);
	}
}

void UVulkanRenderDevice::SetProjection(bool requestNearZRangeHackProjection) {
	float left, right, bottom, top, zNear, zFar;
	float invRightMinusLeft, invTopMinusBottom, invNearMinusFar;

	//Save new Z range hack projection state
	m_nearZRangeHackProjectionActive = requestNearZRangeHackProjection;

	//Set default zNearVal
	FLOAT zNearVal = 0.5f;

	FLOAT zScaleVal = 1.0f;
	if (requestNearZRangeHackProjection) {
		zScaleVal = 0.125f;
		zNearVal = 0.5;
	}
	else {
		if (m_useZRangeHack) {
			zNearVal = 4.0f;
		}
	}

	left = -m_RProjZ * zNearVal;
	right = +m_RProjZ * zNearVal;
	bottom = -m_Aspect*m_RProjZ * zNearVal;
	top = +m_Aspect*m_RProjZ * zNearVal;
	zNear = 1.0f * zNearVal;
	zFar = 32768.0f;
	if (requestNearZRangeHackProjection) {
		zFar *= zScaleVal;
	}

	invRightMinusLeft = 1.0f / (right - left);
	invTopMinusBottom = 1.0f / (top - bottom);
	invNearMinusFar = 1.0f / (zNear - zFar);

	mat4 d3dProj;
	d3dProj.elem[0][0] = 2.0f * zNear * invRightMinusLeft;
	d3dProj.elem[0][1] = 0.0f;
	d3dProj.elem[0][2] = 0.0f;
	d3dProj.elem[0][3] = 0.0f;

	d3dProj.elem[1][0] = 0.0f;
	d3dProj.elem[1][1] = 2.0f * zNear * invTopMinusBottom;
	d3dProj.elem[1][2] = 0.0f;
	d3dProj.elem[1][3] = 0.0f;

	d3dProj.elem[2][0] = 0*1.0f / (FLOAT)FrameX;
	d3dProj.elem[2][1] = 0*-1.0f / (FLOAT)FrameY;
	d3dProj.elem[2][2] = -zScaleVal * (zFar * invNearMinusFar);
	d3dProj.elem[2][3] = 1.0f;

	d3dProj.elem[3][0] = 0.0f;
	d3dProj.elem[3][1] = 0.0f;
	d3dProj.elem[3][2] = zScaleVal * zScaleVal * (zNear * zFar * invNearMinusFar);
	d3dProj.elem[3][3] = 0.0f;

	// should equal
	//g_current_projection = transpose(d3dProj);
	//g_current_projection = perspectiveMatrixX(m_Fov, FrameFX, FrameFY, zNear, zFar, false);
	g_current_projection = perspectiveMatrixXReverseZ(m_Fov, FrameFX, FrameFY, zNear, zFar);

	return;
}

void UVulkanRenderDevice::SetOrthoProjection(void) {
	float left, right, bottom, top, zNear, zFar;
	float invRightMinusLeft, invTopMinusBottom, invNearMinusFar;

	left = -m_RProjZ * 0.5f;
	right = +m_RProjZ * 0.5f;
	bottom = -m_Aspect*m_RProjZ * 0.5f;
	top = +m_Aspect*m_RProjZ * 0.5f;
	zNear = 1.0f * 0.5f;
	zFar = 32768.0f;

	invRightMinusLeft = 1.0f / (right - left);
	invTopMinusBottom = 1.0f / (top - bottom);
	invNearMinusFar = 1.0f / (zNear - zFar);

	mat4 d3dProj;
	d3dProj.elem[0][0] = 2.0f * invRightMinusLeft;
	d3dProj.elem[0][1] = 0.0f;
	d3dProj.elem[0][2] = 0.0f;
	d3dProj.elem[0][3] = 0.0f;

	d3dProj.elem[1][0] = 0.0f;
	d3dProj.elem[1][1] = -2.0f * invTopMinusBottom;
	d3dProj.elem[1][2] = 0.0f;
	d3dProj.elem[1][3] = 0.0f;

	d3dProj.elem[2][0] = 0.0f;
	d3dProj.elem[2][1] = 0.0f;
	d3dProj.elem[2][2] = -1.0f * invNearMinusFar;
	d3dProj.elem[2][3] = 0.0f;

	d3dProj.elem[3][0] = -1.0f / (FLOAT)FrameX;
	d3dProj.elem[3][1] = 1.0f / (FLOAT)FrameY;
	d3dProj.elem[3][2] = zNear * invNearMinusFar;
	d3dProj.elem[3][3] = 1.0f;

	g_current_projection = d3dProj;
}
