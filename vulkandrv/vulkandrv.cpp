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

//#include "resource.h"
#include "vulkandrv.h"
#include "utils/logging.h"
#include "utils/macros.h"
#include "utils/file_utils.h"
#include "utils/Image.h"
//#include "texconverter.h"
//#include "customflags.h"
#include "misc.h"
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

struct SimpleVertex {
    vec4 pos;
    vec4 color;
    vec2 uv;
};
struct PerFrameUniforms {
	mat4 camera;
};

RHIVertexInputBindingDesc vert_bindings_desc[] = {
	{0, sizeof(SimpleVertex), RHIVertexInputRate::kVertex}};

RHIVertexInputAttributeDesc va_desc[] = {
	{0, vert_bindings_desc[0].binding, RHIFormat::kR32G32B32A32_SFLOAT,
	 offsetof(SimpleVertex, pos)},
	{1, vert_bindings_desc[0].binding, RHIFormat::kR32G32B32A32_SFLOAT,
	 offsetof(SimpleVertex, color)},
	{2, vert_bindings_desc[0].binding, RHIFormat::kR32G32_SFLOAT,
	 offsetof(SimpleVertex, uv)}};

// Create Test Vertex Buffer
SimpleVertex vb[] = {{{-0.55f, -0.55f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
					 {{-0.55f, 0.55f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
					 {{0.55f, -0.55f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
					 {{0.55f, 0.55f, 0.0f, 1.0f}, {0.45f, 0.45f, 0.45f, 0.0f}, {1.0f, 1.0f}}};

static int g_curCBIdx = 0;
static int g_curFBIdx = 0;
static IRHIDevice* g_vulkan_device = 0;
static IRHICmdBuf* g_cmdbuf[kNumBufferedFrames] = { 0 };

IRHIBuffer* g_quad_gpu_vb = nullptr;
IRHIBuffer* g_quad_staging_vb = nullptr;
IRHIEvent* g_quad_vb_copy_event= nullptr;	
IRHIRenderPass* g_main_pass = nullptr;
std::vector<IRHIFrameBuffer*> g_main_fb;

IRHIGraphicsPipeline *g_tri_pipeline = nullptr;
IRHIGraphicsPipeline *g_quad_pipeline = nullptr;

IRHIBuffer* g_uniform_buffer = 0;
IRHIDescriptorSetLayout* g_my_layout = 0;
IRHIDescriptorSet* g_my_ds_set0 = 0;
IRHIDescriptorSet* g_my_ds_set1 = 0;

IRHIImage* g_test_image = 0;
IRHIImageView* g_test_image_view = 0;
// temp buffer to store image data before copying to gpu mem
IRHIBuffer* g_img_staging_buf = 0;
IRHIEvent* g_img_copy_event= nullptr;	


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
	g_curCBIdx = 0;

	g_quad_staging_vb =
		device->CreateBuffer(sizeof(vb), RHIBufferUsageFlags::kTransferSrcBit,
							 RHIMemoryPropertyFlagBits::kHostVisible, RHISharingMode::kExclusive);

	g_quad_gpu_vb = device->CreateBuffer(
		sizeof(vb), RHIBufferUsageFlags::kVertexBufferBit | RHIBufferUsageFlags::kTransferDstBit,
		RHIMemoryPropertyFlagBits::kDeviceLocal, RHISharingMode::kExclusive);

	assert(g_quad_gpu_vb);
	assert(g_quad_staging_vb);
	uint8_t *memptr = (uint8_t *)g_quad_staging_vb->Map(device, 0, sizeof(vb), 0);
	memcpy(memptr, vb, sizeof(vb));
	g_quad_staging_vb->Unmap(device);

	g_quad_vb_copy_event = device->CreateEvent();

	RHIAttachmentDesc att_desc;
	att_desc.format = device->GetSwapChainFormat();
	att_desc.numSamples = 1;
	att_desc.loadOp = RHIAttachmentLoadOp::kClear;
	att_desc.storeOp = RHIAttachmentStoreOp::kStore;
	att_desc.stencilLoadOp = RHIAttachmentLoadOp::kDoNotCare;
	att_desc.stencilStoreOp = RHIAttachmentStoreOp::kDoNotCare;
	att_desc.initialLayout = RHIImageLayout::kUndefined;
	att_desc.finalLayout = RHIImageLayout::kPresent;

	RHIAttachmentRef color_att_ref = {0, RHIImageLayout::kColorOptimal};

	RHISubpassDesc sp_desc;
	sp_desc.bindPoint = RHIPipelineBindPoint::kGraphics;
	sp_desc.colorAttachmentCount = 1;
	sp_desc.colorAttachments = &color_att_ref;
	sp_desc.depthStencilAttachment = nullptr;
	sp_desc.inputAttachmentCount = 0;
	sp_desc.inputAttachments = nullptr;
	sp_desc.preserveAttachmentCount = 0;
	sp_desc.preserveAttachments = nullptr;

	RHISubpassDependency sp_dep0;
	sp_dep0.srcSubpass = kSubpassExternal;
	sp_dep0.dstSubpass = 0;
	sp_dep0.srcStageMask = RHIPipelineStageFlags::kBottomOfPipe;
	sp_dep0.dstStageMask = RHIPipelineStageFlags::kColorAttachmentOutput;
	sp_dep0.srcAccessMask = RHIAccessFlags::kMemoryRead;
	sp_dep0.dstAccessMask = RHIAccessFlags::kColorAttachmentWrite;
	sp_dep0.dependencyFlags = (uint32_t)RHIDependencyFlags::kByRegion;

	RHISubpassDependency sp_dep1;
	sp_dep1.srcSubpass = 0;
	sp_dep1.dstSubpass = kSubpassExternal;
	sp_dep1.srcStageMask = RHIPipelineStageFlags::kColorAttachmentOutput;
	sp_dep1.dstStageMask = RHIPipelineStageFlags::kBottomOfPipe;
	sp_dep1.srcAccessMask = RHIAccessFlags::kColorAttachmentWrite;
	sp_dep1.dstAccessMask = RHIAccessFlags::kMemoryRead;
	sp_dep1.dependencyFlags = (uint32_t)RHIDependencyFlags::kByRegion;

	RHISubpassDependency sp_deps[] = { sp_dep0, sp_dep1 };

	RHIRenderPassDesc rp_desc;
	rp_desc.attachmentCount = 1;
	rp_desc.attachmentDesc = &att_desc;
	rp_desc.subpassCount = 1;
	rp_desc.subpassDesc = &sp_desc;
	rp_desc.dependencyCount = countof(sp_deps);
	rp_desc.dependencies = sp_deps;

	g_main_pass = device->CreateRenderPass(&rp_desc);

	g_main_fb.resize(device->GetSwapChainSize());
	for (size_t i = 0; i < g_main_fb.size(); ++i) {
		IRHIImageView *view = device->GetSwapChainImageView(i);
		const IRHIImage *image = device->GetSwapChainImage(i);

		RHIFrameBufferDesc fb_desc;
		fb_desc.attachmentCount = 1;
		fb_desc.pAttachments = &view;
		fb_desc.width_ = image->Width();
		fb_desc.height_ = image->Height();
		fb_desc.layers_ = 1;
		g_main_fb[i] = device->CreateFrameBuffer(&fb_desc, g_main_pass);
	}

	size_t vs_size;
	const uint32_t *vs =
		(uint32_t *)filesystem::loadfile("vulkandrv/spir-v.vert.spv.bin", &vs_size);
	size_t ps_size;
	const uint32_t *ps =
		(uint32_t *)filesystem::loadfile("vulkandrv/spir-v.frag.spv.bin", &ps_size);
	RHIShaderStage tri_shader_stage[2];
	tri_shader_stage[0].module = device->CreateShader(RHIShaderStageFlagBits::kVertex, vs, vs_size);
	tri_shader_stage[0].pEntryPointName = "main";
	tri_shader_stage[0].stage = RHIShaderStageFlagBits::kVertex;
	tri_shader_stage[1].module = device->CreateShader(RHIShaderStageFlagBits::kFragment, ps, ps_size);
	tri_shader_stage[1].pEntryPointName = "main";
	tri_shader_stage[1].stage = RHIShaderStageFlagBits::kFragment;

	vs = (uint32_t *)filesystem::loadfile("vulkandrv/spir-v-model.vert.spv.bin",
										  &vs_size);
	ps = (uint32_t *)filesystem::loadfile("vulkandrv/spir-v-model.frag.spv.bin",
										  &ps_size);
	RHIShaderStage quad_shader_stage[2];
	quad_shader_stage[0].module = device->CreateShader(RHIShaderStageFlagBits::kVertex, vs, vs_size);
	quad_shader_stage[0].pEntryPointName = "main";
	quad_shader_stage[0].stage = RHIShaderStageFlagBits::kVertex;
	quad_shader_stage[1].module = device->CreateShader(RHIShaderStageFlagBits::kFragment, ps, ps_size);
	quad_shader_stage[1].pEntryPointName = "main";
	quad_shader_stage[1].stage = RHIShaderStageFlagBits::kFragment;

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

	g_test_image = device->CreateImage(&img_desc, RHIImageLayout::kTransferDstOptimal,
									   RHIMemoryPropertyFlagBits::kDeviceLocal);
	assert(g_test_image);

	g_img_staging_buf =
		device->CreateBuffer(1024*1024*4, RHIBufferUsageFlags::kTransferSrcBit,
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

	RHIInputAssemblyState tri_ia_state;
	tri_ia_state.primitiveRestartEnable = false;
	tri_ia_state.topology = RHIPrimitiveTopology::kTriangleList;

	RHIInputAssemblyState quad_ia_state;
	quad_ia_state.primitiveRestartEnable = false;
	quad_ia_state.topology = RHIPrimitiveTopology::kTriangleStrip;

	RHIScissor scissors;
	scissors.x = 0;
	scissors.y = 0;
	scissors.width = (uint32_t)NewX;
	scissors.height = (uint32_t)NewY;

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

	RHIMultisampleState ms_state;
	ms_state.alphaToCoverageEnable = false;
	ms_state.alphaToOneEnable = false;
	ms_state.minSampleShading = 0.0f;
	ms_state.pSampleMask = nullptr;
	ms_state.rasterizationSamples = 1;
	ms_state.sampleShadingEnable = false;

	RHIColorBlendAttachmentState blend_att_state;
	blend_att_state.alphaBlendOp = RHIBlendOp::kAdd;
	blend_att_state.colorBlendOp = RHIBlendOp::kAdd;
	blend_att_state.blendEnable = false;
	blend_att_state.colorWriteMask =
		(uint32_t)RHIColorComponentFlags::kR | (uint32_t)RHIColorComponentFlags::kG |
		(uint32_t)RHIColorComponentFlags::kB | (uint32_t)RHIColorComponentFlags::kA;
	blend_att_state.srcColorBlendFactor = RHIBlendFactor::One;
	blend_att_state.dstColorBlendFactor = RHIBlendFactor::Zero;
	blend_att_state.srcAlphaBlendFactor = RHIBlendFactor::One;
	blend_att_state.dstAlphaBlendFactor = RHIBlendFactor::Zero;

	RHIColorBlendState blend_state = {false, RHILogicOp::kCopy, 1, &blend_att_state, {0, 0, 0, 0}};

	RHIDescriptorSetLayoutDesc dsl_desc[] = {
		{RHIDescriptorType::kSampler, RHIShaderStageFlagBits::kFragment, 1, 0},
		{RHIDescriptorType::kCombinedImageSampler, RHIShaderStageFlagBits::kFragment, 1, 1},
		//{RHIDescriptorType::kUniformBuffer, RHIShaderStageFlagBits::kFragment|RHIShaderStageFlagBits::kVertex, 1, 2}
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
	IRHISampler* test_sampler = device->CreateSampler(sampler_desc);
	IRHISampler* test_sampler2 = device->CreateSampler(sampler_desc);

	g_uniform_buffer =
		device->CreateBuffer(sizeof(PerFrameUniforms), RHIBufferUsageFlags::kUniformBufferBit,
							 RHIMemoryPropertyFlagBits::kDeviceLocal, RHISharingMode::kExclusive);

	g_my_layout = device->CreateDescriptorSetLayout(dsl_desc, countof(dsl_desc));
	g_my_ds_set0 = device->AllocateDescriptorSet(g_my_layout);
	g_my_ds_set1 = device->AllocateDescriptorSet(g_my_layout);

	RHIDescriptorWriteDesc desc_write_desc[4];
	RHIDescriptorWriteDescBuilder builder(desc_write_desc, countof(desc_write_desc));
	builder.add(g_my_ds_set0, 0, test_sampler)
		.add(g_my_ds_set0, 1, test_sampler, RHIImageLayout::kShaderReadOnlyOptimal, g_test_image_view)
		//.add(g_my_ds_set0, 2, g_uniform_buffer, 0, sizeof(PerFrameUniforms));
		//.add(g_my_ds_set1, 0, test_sampler2);// .add(g_my_ds_set1, 2, g_uniform_buffer, 0, sizeof(PerFrameUniforms))
		;
	device->UpdateDescriptorSet(desc_write_desc, builder.cur_index);

	const IRHIDescriptorSetLayout* pipe_layout_desc[] = { g_my_layout, g_my_layout };
	IRHIPipelineLayout *pipeline_layout = device->CreatePipelineLayout(pipe_layout_desc, countof(pipe_layout_desc));

	g_tri_pipeline = device->CreateGraphicsPipeline(
		&tri_shader_stage[0], countof(tri_shader_stage), &tri_vi_state, &tri_ia_state,
		&viewport_state, &raster_state, &ms_state, &blend_state, pipeline_layout, g_main_pass);

	g_quad_pipeline = device->CreateGraphicsPipeline(
		&quad_shader_stage[0], countof(quad_shader_stage), &quad_vi_state, &quad_ia_state,
		&viewport_state, &raster_state, &ms_state, &blend_state, pipeline_layout, g_main_pass);

	if (!UVulkanRenderDevice::SetRes(NewX, NewY, NewColorBytes, Fullscreen)) {
		GError->Log(L"Init: SetRes failed.");
		return 0;
	}

#if 0	
	textureCache= new (std::nothrow) TextureCache(D3D::getDevice());
	if(!textureCache)
	{
		GError->Log(L"Error allocating texture cache.");
		return 0;
	}

	texConverter = new (std::nothrow) TexConverter(textureCache);
	if(!texConverter)
	{
		GError->Log(L"Error allocating texture converter.");
		return 0;
	}

	shader_GouraudPolygon = static_cast<Shader_GouraudPolygon*>(D3D::getShader(D3D::SHADER_GOURAUDPOLYGON));
	shader_Tile = static_cast<Shader_Tile*>(D3D::getShader(D3D::SHADER_TILE));
	shader_ComplexSurface = static_cast<Shader_ComplexSurface*>(D3D::getShader(D3D::SHADER_COMPLEXSURFACE));
	shader_FogSurface = static_cast<Shader_FogSurface*>(D3D::getShader(D3D::SHADER_FOGSURFACE));

	//Brightness
	float brightness;
	GConfig->GetFloat(L"WinDrv.WindowsClient",L"Brightness",brightness);
	D3D::setBrightness(brightness);

#endif
	URenderDevice::PrecacheOnFlip = 1; //Turned on to immediately recache on init (prevents lack of textures after fullscreen switch)

	QueryPerformanceFrequency(&perfCounterFreq); //Init performance counter frequency.
	
	return 1;
}

UBOOL UVulkanRenderDevice::SetRes(INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen)
{
	log_info("SetRes: %d x %d\n", NewX, NewY, Fullscreen);

	//Without BLIT_Direct3D major flickering occurs when switching from fullscreen to windowed.
	UBOOL Result = URenderDevice::Viewport->ResizeViewport(Fullscreen ? (BLIT_Fullscreen|BLIT_Direct3D) : (BLIT_HardwarePaint|BLIT_Direct3D), NewX, NewY, NewColorBytes);
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
	delete g_vulkan_device;
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

int sanity_lock_cnt = 0;
void UVulkanRenderDevice::Lock(FPlane FlashScale, FPlane FlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* HitData, INT* HitSize)
{
	assert(0 == sanity_lock_cnt);

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
	
	IRHIDevice* dev = g_vulkan_device;
	if (!dev->BeginFrame())
	{
		log_error("BeginFrame failed\n");
	}

	IRHIImage* fb_image = dev->GetCurrentSwapChainImage();
	IRHICmdBuf* cb = g_cmdbuf[g_curCBIdx];
	IRHIFrameBuffer *cur_fb = g_main_fb[g_curFBIdx];

	cb->Begin();

	static float sec = 0.0f;
	sec += deltaTime;
	static vec4 color = vec4(1, 0, 0, 0);
	color.x = 0.5f*sinf(2 * 3.1415f*sec) + 0.5f;

	bool bClearScreen = false;
	if (bClearScreen)
	{
		cb->Barrier_PresentToClear(fb_image);
		cb->Clear(fb_image, color, (uint32_t)RHIImageAspectFlags::kColor);
		cb->Barrier_ClearToPresent(fb_image);
	}
	else
	{
		if (!g_quad_vb_copy_event->IsSet(dev)) {
			static bool once = true;
			assert(once);
			cb->CopyBuffer(g_quad_gpu_vb, 0, g_quad_staging_vb, 0, g_quad_staging_vb->Size());
			cb->BufferBarrier(
				g_quad_gpu_vb, RHIAccessFlags::kTransferWrite, RHIPipelineStageFlags::kTransfer,
				RHIAccessFlags::kVertexAttributeRead, RHIPipelineStageFlags::kVertexInput);
			cb->SetEvent(g_quad_vb_copy_event, RHIPipelineStageFlags::kVertexInput);
			once = !once;
		}

		if (!g_img_copy_event->IsSet(dev)) {
			static bool once = true;
			assert(once);
			cb->Barrier_UndefinedToTransfer(g_test_image);
			cb->CopyBufferToImage2D(g_test_image, g_img_staging_buf);
			cb->Barrier_TransferToShaderRead(g_test_image);
			cb->SetEvent(g_img_copy_event, RHIPipelineStageFlags::kFragmentShader);
			once = !once;
		}

		ivec4 render_area(0, 0, fb_image->Width(), fb_image->Height());
		RHIClearValue clear_value = { vec4(0,1,0, 0), 0.0f, 0 };
		cb->BeginRenderPass(g_main_pass, cur_fb, &render_area, &clear_value, 1);

		const IRHIDescriptorSet* sets[] = { g_my_ds_set0, g_my_ds_set1 };
		cb->BindDescriptorSets(RHIPipelineBindPoint::kGraphics, g_tri_pipeline->Layout(), sets, countof(sets));
		cb->BindPipeline(RHIPipelineBindPoint::kGraphics, g_tri_pipeline);
		cb->Draw(3, 1, 0, 0);

		// I think there is no need to wait as we schedule draw in the queue and all operations are sequential there
		// only necessary if we want to do something on a host, but let it be here as an example
		if (g_quad_vb_copy_event->IsSet(dev) && g_img_copy_event->IsSet(dev)) {

			const IRHIDescriptorSet* sets[] = { g_my_ds_set0, g_my_ds_set1 };
			cb->BindDescriptorSets(RHIPipelineBindPoint::kGraphics, g_quad_pipeline->Layout(), sets, countof(sets));
			cb->BindPipeline(RHIPipelineBindPoint::kGraphics, g_quad_pipeline);
			cb->BindVertexBuffers(&g_quad_gpu_vb, 0, 1);
			cb->Draw(4, 1, 0, 0);
		}
		else {
			log_debug("Waiting for copy to finish\n");
		}
		cb->EndRenderPass(g_main_pass, cur_fb);
	}


	cb->End();
	dev->Submit(cb, RHIQueueType::kGraphics);

	sanity_lock_cnt++;
}

void UVulkanRenderDevice::Unlock(UBOOL Blit)
{
	IRHIDevice* dev = g_vulkan_device;
	assert(1 == sanity_lock_cnt);

	if (!g_vulkan_device->Present())
	{
		log_error("Present failed\n");
	}

	if (!g_vulkan_device->EndFrame())
	{
		log_error("EndFrame failed\n");
	}

	g_curCBIdx = (g_curCBIdx  + 1) % ((int)dev->GetNumBufferedFrames());
	g_curFBIdx = (g_curFBIdx  + 1) % ((int)g_main_fb.size());

	sanity_lock_cnt--;
}
void UVulkanRenderDevice::DrawComplexSurface(FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet)
{
}
void UVulkanRenderDevice::DrawGouraudPolygon(FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, int NumPts, DWORD PolyFlags, FSpanBuffer* Span)
{
}
void UVulkanRenderDevice::DrawTile(FSceneNode* Frame, FTextureInfo& Info, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, class FSpanBuffer* Span, FLOAT Z, FPlane Color, FPlane Fog, DWORD PolyFlags)
{
}
void UVulkanRenderDevice::Draw2DLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2)
{
}
void UVulkanRenderDevice::Draw2DPoint(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2, FLOAT Z)
{
}
void UVulkanRenderDevice::ClearZ(FSceneNode* Frame)
{
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

void UVulkanRenderDevice::SetSceneNode(FSceneNode* Frame)
{
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
	}

	for (size_t i = 0; i < g_main_fb.size(); ++i) {
		IRHIImageView *view = dev->GetSwapChainImageView(i);
		const IRHIImage *image = dev->GetSwapChainImage(i);

		RHIFrameBufferDesc fb_desc;
		fb_desc.attachmentCount = 1;
		fb_desc.pAttachments = &view;
		fb_desc.width_ = image->Width();
		fb_desc.height_ = image->Height();
		fb_desc.layers_ = 1;
		g_main_fb[i] = dev->CreateFrameBuffer(&fb_desc, g_main_pass);
	}

}
