#pragma once

struct TextureUploadTask {
	enum: unsigned char { kPending = 0, kDone };
	class IRHIImage* image;
	class IRHIImageView* img_view;
	class IRHIBuffer* img_staging_buf;
	class IRHIEvent* img_copy_event;
	bool is_update;
	unsigned char state;
};

typedef unsigned __int64 CacheKey_t;

class TextureCache {
	TextureCache() = default;
	~TextureCache() = default;

	struct CacheImpl* tc;
public:
	bool isCached(CacheKey_t id) const;
	const class IRHIImageView* get(CacheKey_t id) const;
	static TextureCache* makeCache();
	static void destroy(TextureCache* );
	bool cache(/*const*/ struct FTextureInfo* tex_info, unsigned long PolyFlags, class IRHIDevice* dev, struct TextureUploadTask* task);
	bool update(const struct FTextureInfo* tex_info, unsigned long PolyFlags, class IRHIDevice* dev, struct TextureUploadTask* task);
};
