#pragma once 

#include "vulkan_common.h"

#include "vulkan_rhi.h"
#include <stdint.h> // mbstowcs_s
#include <cassert>
#include <vector>
#include <unordered_map>

class RHIImageVk;
class RHIImageViewVk;


struct QueueFamilies {
	uint32_t graphics_;// = 0xffffffff;
	uint32_t compute_;// = 0xffffffff;
	uint32_t transfer_;// = 0xffffffff;
	uint32_t family_bits_;// = 0;

	uint32_t present_;// = 0xffffffff;

	QueueFamilies() :graphics_(0xffffffff), compute_(0xffffffff), transfer_(0xffffffff), family_bits_(0), present_(0xffffffff) {}
	bool has_graphics() { return 0 != (family_bits_ & VK_QUEUE_GRAPHICS_BIT); }
	bool has_present() { return 0xffffffff != present_; }

};

struct Image {
	VkImage handle_;// = VK_NULL_HANDLE;
	VkImageView view_;// = VK_NULL_HANDLE;
	VkImageLayout layout_;// = VK_IMAGE_LAYOUT_UNDEFINED ;
	VkAccessFlags access_flags_;// = VK_ACCESS_FLAG_BITS_MAX_ENUM;
	VkFormat format_;// = VK_FORMAT_UNDEFINED;
};

struct SwapChainData {
	VkSurfaceCapabilitiesKHR capabilities_;
	std::vector<VkSurfaceFormatKHR> formats_;
	std::vector<VkPresentModeKHR> present_modes_;
};

struct SwapChain {
	VkSwapchainKHR swap_chain_;// = VK_NULL_HANDLE;
	VkFormat format_;// = VK_FORMAT_UNDEFINED;
	VkExtent2D extent_;// {0, 0};
	std::vector<class ::RHIImageVk*> images_;
	std::vector<class ::RHIImageViewVk*> views_;
};

struct VulkanDevice {
	VkInstance instance_;
	VkDebugUtilsMessengerEXT debug_messenger_;
	VkPhysicalDevice phys_device_;// = VK_NULL_HANDLE;
	SwapChainData swap_chain_data_;
	SwapChain swap_chain_;
	QueueFamilies queue_families_;
	VkDevice device_;// = VK_NULL_HANDLE;
	VkQueue graphics_queue_;// = VK_NULL_HANDLE;
	//VkQueue compute_queue_;// = VK_NULL_HANDLE;
	VkQueue present_queue_;// = VK_NULL_HANDLE;
	VkSurfaceKHR surface_;
	// TODO: this belongs to renderer
	VkSemaphore img_avail_sem_[kNumBufferedFrames];
	VkSemaphore rendering_finished_sem_[kNumBufferedFrames];
	VkFence frame_fence_[kNumBufferedFrames];

	VkAllocationCallbacks* pallocator_;// = nullptr;

	bool is_initialized_;// = false;

	VulkanDevice() {};// = default;
private:
	VulkanDevice(VulkanDevice&&);// = delete;
	VulkanDevice(const VulkanDevice&);// = delete;
};


////////////////////////////////////////////////////////////////////////////////
class RHIImageVk: public IRHIImage {

	VkImage handle_ = VK_NULL_HANDLE;
	VkMemoryPropertyFlags mem_prop_flags_;
	~RHIImageVk() = default;
public:
	//VkFormat vk_format_;
	VkAccessFlags vk_access_flags_ = VK_ACCESS_FLAG_BITS_MAX_ENUM;
	VkImageLayout vk_layout_;

	void Destroy(IRHIDevice *device);
	RHIImageVk(VkImage image, const RHIImageDesc &desc, VkMemoryPropertyFlags mem_prop_flags,
			   VkImageLayout layout)
		: IRHIImage(desc), handle_(image), mem_prop_flags_(mem_prop_flags), vk_layout_(layout) {} 
	VkImage Handle() const { return handle_; }
	//VkFormat Format() const { return vk_format_; }
};


////////////////////////////////////////////////////////////////////////////////
class RHIImageViewVk: public IRHIImageView  {
	VkImageView handle_;
	RHIImageVk* image_;
	~RHIImageViewVk() = default;

  public:
	RHIImageViewVk(VkImageView iv, RHIImageVk *image) : handle_(iv) { image_ = image; }
    const IRHIImage* GetImage() const { assert(image_); return image_; }
    IRHIImage* GetImage() { assert(image_); return image_; }
	void Destroy(IRHIDevice *);
	VkImageView Handle() const { return handle_; }
};

////////////////////////////////////////////////////////////////////////////////
class RHISamplerVk: public IRHISampler {
	VkSampler handle_;
	RHISamplerDesc desc_;
	~RHISamplerVk() = default;

  public:
	RHISamplerVk(VkSampler sampler, const RHISamplerDesc &desc) : handle_(sampler), desc_(desc) {}
	void Destroy(IRHIDevice *);
	VkSampler Handle() const { return handle_; }
};

////////////////////////////////////////////////////////////////////////////////
class RHIDescriptorSetLayoutVk : public IRHIDescriptorSetLayout {
	VkDescriptorSetLayout handle_;
	~RHIDescriptorSetLayoutVk() = default;

public:
	std::vector<VkDescriptorSetLayoutBinding> vk_bindings_;
	std::vector<RHIDescriptorSetLayoutDesc> bindings_;

	RHIDescriptorSetLayoutVk(VkDescriptorSetLayout dsl, const RHIDescriptorSetLayoutDesc* desc,
		int count);
	void Destroy(IRHIDevice*);
	VkDescriptorSetLayout Handle() const { return handle_; }
};

// it is not an architecture I am just practicing typing

class RHIDescriptorSetVk : public IRHIDescriptorSet {
	VkDescriptorSet handle_;
	~RHIDescriptorSetVk() = default;
public:
	const RHIDescriptorSetLayoutVk* const layout_;
	RHIDescriptorSetVk(VkDescriptorSet ds, RHIDescriptorSetLayoutVk* layout) :handle_(ds), layout_(layout) {}
	VkDescriptorSet Handle() const { return handle_; }
};


////////////////////////////////////////////////////////////////////////////////
class RHIShaderVk: public IRHIShader {
	VkShaderModule shader_module_;
	std::vector<uint32_t> code_;//do we need this?
	RHIShaderStageFlagBits::Value stage_;
	VkShaderStageFlags vk_stage_;
	~RHIShaderVk() = default;
public:
	void Destroy(IRHIDevice* device);
	static RHIShaderVk* Create(IRHIDevice* device, const uint32_t* pdata, uint32_t size, RHIShaderStageFlagBits::Value stage);
	//const unsigned char* code() const { return code_.get(); }
	VkShaderModule Handle() const { return shader_module_; }
};

////////////////////////////////////////////////////////////////////////////////
class RHIPipelineLayoutVk: public IRHIPipelineLayout {
	VkPipelineLayout handle_;
public:
	void Destroy(IRHIDevice* device);
	static RHIPipelineLayoutVk* Create(IRHIDevice* device, IRHIDescriptorSetLayout* desc_set_layout);
	VkPipelineLayout Handle() const { return handle_; }
};

////////////////////////////////////////////////////////////////////////////////
class RHIGraphicsPipelineVk: public IRHIGraphicsPipeline {
	VkPipeline handle_;
public:
	void Destroy(IRHIDevice *device);
	static RHIGraphicsPipelineVk *
	Create(IRHIDevice *device, const RHIShaderStage *shader_stage, uint32_t shader_stage_count,
		   const RHIVertexInputState *vertex_input_state,
		   const RHIInputAssemblyState *input_assembly_state,
		   const RHIViewportState *viewport_state, const RHIRasterizationState *raster_state,
		   const RHIMultisampleState *multisample_state,
		   const RHIColorBlendState *color_blend_state, const IRHIPipelineLayout *pipleline_layout,
		   const IRHIRenderPass *render_pass);

	VkPipeline Handle() const { return handle_; }
};

////////////////////////////////////////////////////////////////////////////////
class RHIBufferVk : public IRHIBuffer {
    VkBuffer handle_;
    VkDeviceMemory backing_mem_;
    uint32_t buf_size_;
    uint32_t usage_flags_;
    uint32_t mem_flags_;

    bool is_mapped_;
    uint32_t mapped_offset_;
    uint32_t mapped_size_;
    uint32_t mapped_flags_;

public:
	static RHIBufferVk* Create(IRHIDevice* device, uint32_t size, uint32_t usage, uint32_t memprop, RHISharingMode::Value sharing);
	void Destroy(IRHIDevice *device);

    void* Map(IRHIDevice* device, uint32_t offset, uint32_t size, uint32_t map_flags);
    void Unmap(IRHIDevice* device);

    uint32_t Size() const { return buf_size_; }
	VkBuffer Handle() const { return handle_; }
};

////////////////////////////////////////////////////////////////////////////////
class RHIFenceVk: public IRHIFence {
        VkFence handle_;

        //TODO: add variables to cache the state

		RHIFenceVk() = default;
        ~RHIFenceVk() = default;
    public:
        virtual void Reset(IRHIDevice *device) ;
        virtual void Wait(IRHIDevice *device, uint64_t timeout) ;
        virtual bool IsSignalled(IRHIDevice *device) ;
        virtual void Destroy(IRHIDevice *device) ;

        VkFence Handle() const { return handle_; }

        static RHIFenceVk* Create(IRHIDevice *device, bool create_signalled);
};

////////////////////////////////////////////////////////////////////////////////
class RHIEventVk: public IRHIEvent {
        VkEvent handle_;
    public:
        // immediate set/reset by host
        virtual void Set(IRHIDevice *device) ;
        virtual void Reset(IRHIDevice *device) ;
        virtual bool IsSet(IRHIDevice *device) ;
        virtual void Destroy(IRHIDevice *device) ;

        VkEvent Handle() const { return handle_; }

        static RHIEventVk* Create(IRHIDevice *device);
};

////////////////////////////////////////////////////////////////////////////////
class RHICmdBufVk: public IRHICmdBuf {
	VkCommandBuffer cb_;
	bool is_recording_;// = false;
public:
	RHICmdBufVk(VkCommandBuffer cb/*, uint32_t qfi, VkCommandPool cmd_pool*/) :
		cb_(cb) {}
	VkCommandBuffer Handle() const { return cb_; }

	virtual void Barrier_ClearToPresent(IRHIImage* image) ;
	virtual void Barrier_PresentToClear(IRHIImage* image) ;
	virtual void Barrier_PresentToDraw(IRHIImage* image) ;
	virtual void Barrier_DrawToPresent(IRHIImage* image) ;

	virtual void BufferBarrier(IRHIBuffer *i_buffer, RHIAccessFlags::Value src_acc_flags,
							   RHIPipelineStageFlags::Value src_stage, RHIAccessFlags::Value dst_acc_fags,
							   RHIPipelineStageFlags::Value dst_stage) ;

	virtual bool Begin() ;
	virtual bool BeginRenderPass(IRHIRenderPass *i_rp, IRHIFrameBuffer *i_fb, const ivec4 *render_area,
					   const RHIClearValue *clear_values, uint32_t count) ;
	virtual void Draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex,
					  uint32_t first_instance) ;
    virtual void BindVertexBuffers(IRHIBuffer** i_vb, uint32_t first_binding, uint32_t count) ;

	virtual void CopyBuffer(class IRHIBuffer *dst, uint32_t dst_offset, class IRHIBuffer *src,
							uint32_t src_offset, uint32_t size) ;

    virtual void SetEvent(IRHIEvent* event, RHIPipelineStageFlags::Value stage) ;
    virtual void ResetEvent(IRHIEvent* event, RHIPipelineStageFlags::Value stage) ;

	virtual bool End() ;
	virtual void EndRenderPass(const IRHIRenderPass *i_rp, IRHIFrameBuffer *i_fb) ;
	virtual void Clear(IRHIImage* image_in, const vec4& color, uint32_t img_aspect_bits) ;
	virtual void BindPipeline(RHIPipelineBindPoint::Value bind_point, IRHIGraphicsPipeline* pipeline) ;

};

////////////////////////////////////////////////////////////////////////////////

class RHIFrameBufferVk: public IRHIFrameBuffer {
	VkFramebuffer handle_;
    std::vector<RHIImageViewVk*> attachments_;
	virtual ~RHIFrameBufferVk() = default;
public:
  RHIFrameBufferVk(VkFramebuffer fb, std::vector<RHIImageViewVk *> attachments)
	  : handle_(fb), attachments_(attachments) {}
  virtual void Destroy(IRHIDevice *) override;
  VkFramebuffer Handle() const { return handle_; }
  const std::vector<RHIImageViewVk*> GetAttachments() const {
      return attachments_;
  }
};

class RHIRenderPassVk: public IRHIRenderPass {
	VkRenderPass handle_;
	std::vector<RHIImageLayout::Value> att_final_layouts_;
	virtual ~RHIRenderPassVk() =default;

  public:
	RHIRenderPassVk(VkRenderPass rp, std::vector<RHIImageLayout::Value> att_final_layouts)
		: handle_(rp), att_final_layouts_(att_final_layouts) {}
	void Destroy(IRHIDevice *device);
	VkRenderPass Handle() const { return handle_; }

	RHIImageLayout::Value GetFinalLayout(uint32_t i) const {
		assert(att_final_layouts_.size() > i);
		return att_final_layouts_[i];
	}
};

class RHIDeviceVk: public IRHIDevice {
	VulkanDevice& dev_;

	// queue family index -> pool
	std::unordered_map<uint32_t, VkCommandPool> cmd_pools_;
	// queue family index -> cb
	std::unordered_map<uint32_t, VkCommandBuffer> cmd_buffers_;

	std::unordered_map<RHIDescriptorSetLayoutVk*, struct DescPoolInfo*> desc_pools_;

	// 
	int32_t prev_frame_;// = -1;
	int32_t cur_frame_;// = -1;
	uint32_t cur_swap_chain_img_idx_;// = 0xffffffff;
	bool between_begin_frame;// = false;

	fpOnSwapChainRecreated fp_swap_chain_recreated_;
	void* user_ptr_;

public:
	explicit RHIDeviceVk(VulkanDevice &device)
		: dev_(device), prev_frame_(-1), cur_frame_(-1), cur_swap_chain_img_idx_(0xffffffff),
		  between_begin_frame(false), fp_swap_chain_recreated_(nullptr), user_ptr_(nullptr) {}

	// interface implementation
	virtual ~RHIDeviceVk() {};
	virtual IRHICmdBuf* CreateCommandBuffer(RHIQueueType::Value queue_type) ;
	virtual IRHIRenderPass* CreateRenderPass(const RHIRenderPassDesc* desc) ;
	virtual IRHIFrameBuffer* CreateFrameBuffer(RHIFrameBufferDesc* desc, const IRHIRenderPass* rp_in) ;
	virtual IRHIImage* CreateImage(const RHIImageDesc* desc, RHIImageLayout::Value initial_layout, RHIMemoryPropertyFlags mem_prop) ;
	virtual IRHIImageView* CreateImageView(const RHIImageViewDesc* desc) ;
	virtual IRHISampler *CreateSampler(const RHISamplerDesc *desc);
	virtual IRHIBuffer* CreateBuffer(uint32_t size, uint32_t usage, uint32_t memprop, RHISharingMode::Value sharing) ;
	virtual IRHIDescriptorSetLayout* CreateDescriptorSetLayout(const RHIDescriptorSetLayoutDesc* desc, int count);

	virtual IRHIDescriptorSet* AllocateDescriptorSet(IRHIDescriptorSetLayout* layout);

    virtual IRHIFence* CreateFence(bool create_signalled) ;
    virtual IRHIEvent* CreateEvent() ;

    virtual IRHIGraphicsPipeline *CreateGraphicsPipeline(
            const RHIShaderStage *shader_stage, uint32_t shader_stage_count,
            const RHIVertexInputState *vertex_input_state,
            const RHIInputAssemblyState *input_assembly_state, const RHIViewportState *viewport_state,
            const RHIRasterizationState *raster_state, const RHIMultisampleState *multisample_state,
            const RHIColorBlendState *color_blend_state, const IRHIPipelineLayout *i_pipleline_layout,
            const IRHIRenderPass *i_render_pass) ;

    virtual IRHIPipelineLayout* CreatePipelineLayout(IRHIDescriptorSetLayout* desc_set_layout) ;
    virtual IRHIShader* CreateShader(RHIShaderStageFlagBits::Value stage, const uint32_t *pdata, uint32_t size) ;

	virtual RHIFormat GetSwapChainFormat() ;
	virtual uint32_t GetSwapChainSize()  { return (uint32_t)dev_.swap_chain_.images_.size(); }
	virtual class IRHIImageView* GetSwapChainImageView(uint32_t index) ;
	virtual class IRHIImage* GetSwapChainImage(uint32_t index) ;
	virtual IRHIImage* GetCurrentSwapChainImage() ;

    // TODO: this should belong to the renderer
    virtual uint32_t GetNumBufferedFrames()  { return kNumBufferedFrames; }
    virtual uint32_t GetCurrentFrame() { return cur_frame_; }

	virtual bool Submit(IRHICmdBuf* cb_in, RHIQueueType::Value queue_type) ;
	virtual bool BeginFrame() ;
	virtual bool Present() ;
	virtual bool EndFrame() ;

	VkDevice Handle() const { return dev_.device_; }
	VkPhysicalDevice PhysDeviceHandle() const { return dev_.phys_device_; }
	VkAllocationCallbacks* Allocator() const { return dev_.pallocator_; }

	virtual bool OnWindowSizeChanged(uint32_t width, uint32_t height, bool fullscreen) override;
	virtual void SetOnSwapChainRecreatedCallback(fpOnSwapChainRecreated callback, void* user_ptr) override {
		fp_swap_chain_recreated_ = callback;
		user_ptr_ = user_ptr;
	}
};

