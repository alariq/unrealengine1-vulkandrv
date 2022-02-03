#include "texture_cache.h"
#include "rhi.h"
#include "utils/logging.h"

#pragma pack(push, 4)
#include "Engine.h"
#pragma pack(pop)

#include <unordered_map>

struct TextureFormat {
	bool b_is_supported; /**< Is format supported by us */
	INT blocksize;		 /**< Block size (in one dimension) in bytes for compressed textures */
	char pixelsPerBlock; /**< Pixels each block of a compressed texture encodes */
	bool directAssign;	 /**< No conversion and temporary storage needed */
	RHIFormat RHIFormat; /**< format to use when creating texture */
	void (*conversionFunc)(const FTextureInfo *, DWORD flags, DWORD *dst, uint32_t dst_size,
						   int level); /**< Conversion function to use if no direct assignment possible */
};

struct TextureMetaData {
	// UINT height;
	// UINT width;

	/** Precalculated parameters with which to normalize texture coordinates */
	FLOAT multU;
	FLOAT multV;
	bool masked; /**< Tracked to fix masking issues, see UD3D10RenderDevice::PrecacheTexture */
	// bool externalTextures[DUMMY_NUM_EXTERNAL_TEXTURES]; /**< Which extra texture slots are used
	// */
	DWORD customPolyFlags; /**< To allow override textures to have their own polyflags set in a file
							*/
};

void Paletted2RGBA8(const FTextureInfo *, DWORD flags, DWORD* dst, uint32_t dst_size, int level);

// Unreal to vulkan
const static TextureFormat g_format_reg[] = {
	{true, 0, 0, false, RHIFormat::kR8G8B8A8_SRGB, &Paletted2RGBA8}, /**< TEXF_P8 = 0x00 */
	{true, 0, 0, true, RHIFormat::kR8G8B8A8_SRGB, nullptr},		  /**< TEXF_RGBA7	= 0x01 */
	{false, 0, 0, true, RHIFormat::kR8G8B8A8_SRGB, nullptr},		  /**< TEXF_RGB16	= 0x02 */
#if 0
	{true,4,8,true,DXGI_FORMAT_BC1_UNORM,nullptr},									/**< TEXF_DXT1 = 0x03 */
	{false,0,0,true,DXGI_FORMAT_UNKNOWN,nullptr},									/**< TEXF_RGB8 = 0x04 */
	{true,0,0,true,DXGI_FORMAT_R8G8B8A8_UNORM,nullptr},								/**< TEXF_RGBA8	= 0x05 */
#endif
};

struct MipInfo {
	uint32_t SysMemPitch;
	DWORD* pSysMem;
	uint32_t size;
	bool b_owns_memory;
};

struct CachedTexture {
	TextureMetaData metadata;
	IRHIImage* image;
	IRHIImageView* view;
};

struct CacheImpl {
	std::unordered_map<unsigned __int64, CachedTexture> thash;
};

int getTextureSize(RHIFormat fmt, int w, int h) {
	switch (fmt) {
	case RHIFormat::kB8G8R8A8_UNORM:
	case RHIFormat::kB8G8R8A8_SRGB:
	case RHIFormat::kR8G8B8A8_UNORM:
	case RHIFormat::kR8G8B8A8_SRGB:
		return w * h * 4;
	default:
		assert(!"Not implemented");
		return -1;
	}
}

MipInfo convertMip(const FTextureInfo *TexInfo, const TextureFormat &format, DWORD PolyFlags,
				   int mipLevel) {
	MipInfo mi;
	// Set stride
	if (format.blocksize > 0) {
		// Max() as each mip is at least block sized in each direction
		mi.SysMemPitch = max(TexInfo->Mips[mipLevel]->USize, format.blocksize) *
						 format.pixelsPerBlock / format.blocksize;
	} else {
		// Pitch is set so garbage data outside of UClamp is skipped
		mi.SysMemPitch = TexInfo->Mips[mipLevel]->USize * sizeof(DWORD);
	}

	// Assign or convert
	if (format.directAssign) {
		// Direct assignment from Unreal to our texture is possible
		mi.pSysMem = (DWORD*)TexInfo->Mips[mipLevel]->DataPtr;
		mi.b_owns_memory = false;
		mi.size = getTextureSize(format.RHIFormat, TexInfo->Mips[mipLevel]->USize,
								 TexInfo->Mips[mipLevel]->VSize);
		if (mi.size == 512 && TexInfo->USize==128) {
			int asdfa = 0;
		}
	} else {
		// Texture needs to be converted via temporary data; allocate it
		// max(...) as otherwise USize*0 can occur
		// ???
		uint32_t dw_size = TexInfo->Mips[mipLevel]->USize * max((TexInfo->VClamp >> mipLevel), 1);
		//uint32_t dw_size = TexInfo->Mips[mipLevel]->USize * TexInfo->Mips[mipLevel]->VSize;
		mi.pSysMem = new (std::nothrow) DWORD[dw_size];
		mi.size = dw_size * sizeof(DWORD);
		if (mi.pSysMem == nullptr) {
			log_error("Convert: Error allocating texture initial data memory.");
			return mi;
		}				
		mi.b_owns_memory = true;

		if (mi.size != 4*TexInfo->Mips[mipLevel]->USize * TexInfo->Mips[mipLevel]->VSize) {
			int asdfasdf = 0;
		}
		if (mi.size == 524288) {
			int asdf = 124;
		}

		//Convert
		format.conversionFunc(TexInfo, PolyFlags, mi.pSysMem, mi.size, mipLevel);
	}

	return mi;
}

// Convert from palleted 8bpp to r8g8b8a8.
void Paletted2RGBA8(const FTextureInfo *TexInfo, DWORD PolyFlags, DWORD* dst, uint32_t dst_size, int mipLevel) {

	// If texture is masked with palette index 0 = transparent; make that index black w. alpha 0
	// (black looks best for the border that gets left after masking)
	if (PolyFlags & PF_Masked) {
		*(DWORD *)(&(TexInfo->Palette->R)) = (DWORD)0;
	}

	const BYTE* const start = (BYTE*)dst;
	BYTE *src = (BYTE *)TexInfo->Mips[mipLevel]->DataPtr;
	BYTE *srcEnd = src + TexInfo->Mips[mipLevel]->USize * TexInfo->Mips[mipLevel]->VSize;
	while (src < srcEnd) {
		*dst = *(DWORD *)&(TexInfo->Palette[*src]);
		src++;
		dst++;
	}
	assert((BYTE*)dst - start == dst_size);
}

static TextureMetaData buildMetaData(const FTextureInfo *TexInfo, DWORD PolyFlags,
									 DWORD customPolyFlags) {
	TextureMetaData metadata;
	metadata.multU = 1.0 / (TexInfo->UScale * TexInfo->UClamp);
	metadata.multV = 1.0 / (TexInfo->VScale * TexInfo->VClamp);
	metadata.masked = (PolyFlags & PF_Masked) != 0;
	metadata.customPolyFlags = customPolyFlags;
#if 0
	for(int i=0;i<TextureCache::DUMMY_NUM_EXTERNAL_TEXTURES;i++)
	{
		metadata.externalTextures[i]=nullptr;
	}
#endif
	return metadata;
}

bool TextureCache::isCached(CacheKey_t id) const {
	return tc->thash.count(id) != 0;
}

const IRHIImageView* TextureCache::get(CacheKey_t id) const {
	assert(isCached(id));
	return tc->thash[id].view;
}

bool TextureCache::isMasked(CacheKey_t id) const {
	assert(isCached(id));
	return tc->thash[id].metadata.masked;
}

bool TextureCache::cache(FTextureInfo *TexInfo, DWORD PolyFlags, IRHIDevice *dev, TextureUploadTask** task) {

	// TODO: can't this just be an assert?
	if (TexInfo->Format > TEXF_RGBA8) {
		log_error("TextureCache: Unknown texture format TEXF_*: %d!\n", TexInfo->Format);
		return false;
	}

	const TextureFormat &format = g_format_reg[(int)TexInfo->Format];
	if((int)TexInfo->Format > TEXF_RGBA7) {
		int asdf =0;
	}
	if (format.b_is_supported == false) {
		log_error("Texture type <%d> is unsupported (yet?).\n", TexInfo->Format);
		return false;
	}

	// Unreal 1 S3TC texture fix: if texture info size doesn't match mip size (happens for some
	// textures for some reason), scale up clamp (which is what we use for the size)
	if (TexInfo->USize != TexInfo->Mips[0]->USize) {
		float scale = (float)TexInfo->Mips[0]->USize / TexInfo->USize;
		TexInfo->USize = TexInfo->Mips[0]->USize; // dont use this but just to be sure
		TexInfo->UClamp *= scale;
		TexInfo->UScale /= scale;
	}

	if (TexInfo->VSize != TexInfo->Mips[0]->VSize) {
		float scale = (float)TexInfo->Mips[0]->VSize / TexInfo->VSize;
		TexInfo->VSize = TexInfo->Mips[0]->VSize;
		TexInfo->VClamp *= scale;
		TexInfo->VScale /= scale;
	}

	// Set texture info. These parameters are the same for each usage of the texture.
	TextureMetaData metadata = buildMetaData(TexInfo, PolyFlags, 0);
	// Mult is a multiplier (so division is only done once here instead of when texture is applied)
	// to normalize texture coordinates. metadata.width = Info.USize; metadata.height = Info.VSize;
	// metadata.multU = 1.0 / (Info.UScale * Info.USize);
	// metadata.multV = 1.0 / (Info.VScale * Info.VSize);

	// Some third party s3tc textures report more mips than the info structure fits
	TexInfo->NumMips = clamp(TexInfo->NumMips, 0, MAX_MIPS);

	// convert only top mip for now
	MipInfo mip_data = convertMip(TexInfo, format, PolyFlags, 0);

	RHIImageDesc img_desc;
	img_desc.type = RHIImageType::k2D;
	img_desc.format = format.RHIFormat;
	img_desc.width = TexInfo->Mips[0]->USize;
	img_desc.height = TexInfo->Mips[0]->VSize;
	img_desc.depth = 1;
	img_desc.arraySize = 1;
	img_desc.numMips = 1;
	img_desc.numSamples = RHISampleCount::k1Bit;
	img_desc.tiling = RHIImageTiling::kOptimal;
	img_desc.usage = RHIImageUsageFlagBits::SampledBit | RHIImageUsageFlagBits::TransferDstBit;
	img_desc.sharingMode = RHISharingMode::kExclusive; // only in graphics queue

	// TODO: why do we need this initial image layout of it only can be undefined or preinitialized?
	IRHIImage *image = dev->CreateImage(&img_desc, RHIImageLayout::kUndefined,
										RHIMemoryPropertyFlagBits::kDeviceLocal);
	assert(image);
	//log_error("Failed to create GPU texture for: %s\n", TexInfo->Texture->GetFullName());

	RHIImageViewDesc iv_desc;
	iv_desc.image = image;
	iv_desc.viewType = RHIImageViewType::k2d;
    iv_desc.format = img_desc.format;
	iv_desc.subresourceRange.aspectMask = RHIImageAspectFlags::kColor;
	iv_desc.subresourceRange.baseArrayLayer = 0;
	iv_desc.subresourceRange.baseMipLevel = 0;
	iv_desc.subresourceRange.layerCount = 1;
	iv_desc.subresourceRange.levelCount = 1;

	IRHIImageView* view = dev->CreateImageView(&iv_desc);
	assert(view);

	tc->thash.insert(std::make_pair(TexInfo->CacheID, CachedTexture{ metadata, image, view}));

	*task = TextureUploadTask::make(image, view, false, mip_data.size, mip_data.pSysMem, dev);

	if (mip_data.b_owns_memory) {
		delete[] mip_data.pSysMem;
	}

	return true;
}

bool TextureCache::update(const struct FTextureInfo* TexInfo, unsigned long PolyFlags,
						  class IRHIDevice *dev, struct TextureUploadTask **task) {

	check(TexInfo->Format <= TEXF_RGBA8);
	const TextureFormat &format = g_format_reg[(int)TexInfo->Format];
	check(format.b_is_supported == true);

	assert(tc->thash.count(TexInfo->CacheID));
	CachedTexture ct = tc->thash[TexInfo->CacheID];

	TextureMetaData metadata = buildMetaData(TexInfo, PolyFlags, 0);
	MipInfo mip_data = convertMip(TexInfo, format, PolyFlags, 0);
	assert(memcmp(&metadata, &ct.metadata, sizeof(TextureMetaData)) == 0);

	*task = TextureUploadTask::make(ct.image, ct.view, true, mip_data.size, mip_data.pSysMem, dev);

	if (mip_data.b_owns_memory) {
		delete[] mip_data.pSysMem;
	}

	return true;
}


TextureCache *TextureCache::makeCache() {
	TextureCache* tc = new TextureCache();
	tc->tc = new CacheImpl;
	return tc;
}

void TextureCache::destroy(TextureCache *tc) {
	//...
	delete tc;
}
////////////////////////////////////////////////////////////////////////////////
// TextureUploadTask 
////////////////////////////////////////////////////////////////////////////////

// NOTE: maybe just have a single array of tasks and mark them as busy/free and instead return task
// index drawback is that then search will be linear and not binary as we will not be able to
// shuffle array to sort it to not invalidate indices
 
// TODO: vector is not the best adt for this
// Invariant: should be always sorted
static std::vector<TextureUploadTask*> g_taskCache;

void texture_upload_task_init() {
}
void texture_upload_task_fini() {
	g_taskCache.clear();
}

TextureUploadTask *TextureUploadTask::make(class IRHIImage *image, class IRHIImageView *img_view,
										   bool is_update, int size, const DWORD*data,
										   IRHIDevice *dev) {
	TextureUploadTask* task = nullptr;
	for (int i = 0; i < (int)g_taskCache.size(); ++i)
	{
		if (g_taskCache[i]->img_staging_buf->Size() >= size) {
			task = g_taskCache[i];
			g_taskCache.erase(g_taskCache.begin() + i);
			break;
		}
	}

	if (!task) {
		task = new TextureUploadTask;
		task->img_copy_event = dev->CreateEvent();;
		task->img_staging_buf =
			dev->CreateBuffer(size, RHIBufferUsageFlagBits::kTransferSrcBit,
							  RHIMemoryPropertyFlagBits::kHostVisible, RHISharingMode::kExclusive);
		task->img_staging_buf->Map(dev, 0, size, 0);
	}

	// initialize

	task->image = image;
	task->img_view = img_view;
	task->size = size;
	task->is_update = is_update;
	task->state = kPending;

	//TODO: convert right into this staging buf
	assert(task->img_staging_buf->Size() >= size);
	memcpy((uint8_t*)task->img_staging_buf->MappedPtr(), data, size);
	// never unmap
	//img_staging_buf->Unmap(dev);

	return task;
}

void TextureUploadTask::release() {
	assert(g_taskCache.end() == std::find(g_taskCache.begin(), g_taskCache.end(), this));

	img_view = nullptr;
	image = nullptr;
	state = kInvalid;
	size = -1;
	is_update = false;

	g_taskCache.push_back(this);
}

// supposed to be called only at the game end / device recreation
// TODO: release memory, otherwise will leak at device recreation
void TextureUploadTask::destroy() {
	state = kInvalid;
	delete this;
}

TextureUploadTask::~TextureUploadTask() {
	//g_taskCache.push_back(task);
}


