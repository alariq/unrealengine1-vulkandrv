#pragma once

struct TextureUploadTask {
	enum : unsigned char { kPending = 0, kDone, kInvalid };
	class IRHIImage *image;
	class IRHIImageView *img_view;
	class IRHIBuffer *img_staging_buf;
	class IRHIEvent *img_copy_event;
	bool is_update;
	unsigned char state;
	int size;

	static TextureUploadTask *make(class IRHIImage *image, class IRHIImageView *img_view,
								   bool is_update, int size,
								   const unsigned long*data,
								   class IRHIDevice *dev);
	void release();
	void destroy();

  private:
	~TextureUploadTask();
	TextureUploadTask() {}
	TextureUploadTask(const TextureUploadTask&);
	TextureUploadTask& operator=(const TextureUploadTask&);
};

typedef unsigned __int64 CacheKey_t;

class TextureCache {
	TextureCache() = default;
	~TextureCache() = default;

	struct CacheImpl *tc;

  public:
	bool isCached(CacheKey_t id) const;
	const class IRHIImageView *get(CacheKey_t id) const;
	static TextureCache *makeCache();
	static void destroy(TextureCache *);
	bool cache(/*const*/ struct FTextureInfo *tex_info, unsigned long PolyFlags,
			   class IRHIDevice *dev, struct TextureUploadTask **task);
	bool update(const struct FTextureInfo *tex_info, unsigned long PolyFlags, class IRHIDevice *dev,
				struct TextureUploadTask **task);
};


void texture_upload_task_init();
void texture_upload_task_fini();

