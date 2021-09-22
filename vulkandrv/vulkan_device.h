#pragma once 

#include "rhi.h"
#include "SDL2/SDL_vulkan.h"
#include "vulkan/vulkan.h"
//#include "vulkan_rhi.h"
#include <stdint.h> // mbstowcs_s
#include <cassert>
#include <vector>
#include <unordered_map>

class RHIImageVk;
class RHIImageViewVk;

namespace rhi_vulkan {

struct QueueFamilies {
	uint32_t graphics_ = 0xffffffff;
	uint32_t compute_ = 0xffffffff;
	uint32_t transfer_ = 0xffffffff;
	uint32_t family_bits_ = 0;

	uint32_t present_ = 0xffffffff;

	bool has_graphics() { return 0 != (family_bits_ & VK_QUEUE_GRAPHICS_BIT); }
	bool has_present() { return 0xffffffff != present_; }

};

struct Image {
	VkImage handle_ = VK_NULL_HANDLE;
	VkImageView view_ = VK_NULL_HANDLE;
	VkImageLayout layout_ = VK_IMAGE_LAYOUT_UNDEFINED ;
	VkAccessFlags access_flags_ = VK_ACCESS_FLAG_BITS_MAX_ENUM;
	VkFormat format_ = VK_FORMAT_UNDEFINED;
};

struct SwapChainData {
	VkSurfaceCapabilitiesKHR capabilities_;
	std::vector<VkSurfaceFormatKHR> formats_;
	std::vector<VkPresentModeKHR> present_modes_;
};

struct SwapChain {
	VkSwapchainKHR swap_chain_ = VK_NULL_HANDLE;
	VkFormat format_ = VK_FORMAT_UNDEFINED;
	VkExtent2D extent_{0, 0};
	std::vector<class ::RHIImageVk*> images_;
	std::vector<class ::RHIImageViewVk*> views_;
};

} // namespace rhi_vulkan

using SwapChainData = rhi_vulkan::SwapChainData;
using SwapChain = rhi_vulkan::SwapChain;
using QueueFamilies = rhi_vulkan::QueueFamilies;

struct VulkanDevice {
	VkInstance instance_;
	VkDebugUtilsMessengerEXT debug_messenger_;
	VkPhysicalDevice phys_device_ = VK_NULL_HANDLE;
	SwapChainData swap_chain_data_;
	SwapChain swap_chain_;
	QueueFamilies queue_families_;
	VkDevice device_ = VK_NULL_HANDLE;
	VkQueue graphics_queue_ = VK_NULL_HANDLE;
	//VkQueue compute_queue_ = VK_NULL_HANDLE;
	VkQueue present_queue_ = VK_NULL_HANDLE;
	VkSurfaceKHR surface_;
    // TODO: this belongs to renderer
	VkSemaphore img_avail_sem_[rhi_vulkan::kNumBufferedFrames];
	VkSemaphore rendering_finished_sem_[rhi_vulkan::kNumBufferedFrames];
    VkFence frame_fence_[rhi_vulkan::kNumBufferedFrames];

	VkAllocationCallbacks *pallocator_ = nullptr;

	bool is_initialized_ = false;

	VulkanDevice() = default;
	VulkanDevice(VulkanDevice&&) = delete;
	VulkanDevice(const VulkanDevice&) = delete;
};

////////////////////////////////////////////////////////////////////////////////
class RHIImageVk : public IRHIImage {
	//rhi_vulkan::Image image_;
	// should those be native Vk* members only?
	RHIFormat format_;
	RHIAccessFlags access_flags_;
	RHIImageLayout layout_;

	VkFormat vk_format_;
	VkImage handle_ = VK_NULL_HANDLE;
	~RHIImageVk() = default;
public:
	VkAccessFlags vk_access_flags_;
	VkImageLayout vk_layout_;

	void Destroy(IRHIDevice* device);
	RHIImageVk(VkImage image, uint32_t width, uint32_t height) : handle_(image) {
		width_ = width;
		height_ = height;
	}
	//const rhi_vulkan::Image& GetImage() const { return image_; }
	//rhi_vulkan::Image& GetImage() { return image_; }
	//void SetImage(const rhi_vulkan::Image& image);
	VkImage Handle() const { return handle_; }
	VkFormat Format() const { return vk_format_; }

};

////////////////////////////////////////////////////////////////////////////////
class RHIImageViewVk : public IRHIImageView {
	VkImageView handle_;
	~RHIImageViewVk() = default;

  public:
	RHIImageViewVk(VkImageView iv, RHIImageVk *image) : handle_(iv) { image_ = image; }
	void Destroy(IRHIDevice *);
	VkImageView Handle() const { return handle_; }
};

////////////////////////////////////////////////////////////////////////////////
class RHIShaderVk: public IRHIShader{
	VkShaderModule shader_module_;
	std::vector<uint32_t> code_;//do we need this?
	RHIShaderStageFlags stage_;
	VkShaderStageFlags vk_stage_;
	~RHIShaderVk() = default;
public:
	void Destroy(IRHIDevice* device);
	static RHIShaderVk* Create(IRHIDevice* device, const uint32_t* pdata, uint32_t size, RHIShaderStageFlags stage);
	//const unsigned char* code() const { return code_.get(); }
	VkShaderModule Handle() const { return shader_module_; }
};

////////////////////////////////////////////////////////////////////////////////
class RHIPipelineLayoutVk : public IRHIPipelineLayout {
	VkPipelineLayout handle_;
public:
	void Destroy(IRHIDevice* device);
	static RHIPipelineLayoutVk* Create(IRHIDevice* device, IRHIDescriptorSetLayout* desc_set_layout);
	VkPipelineLayout Handle() const { return handle_; }
};

////////////////////////////////////////////////////////////////////////////////
class RHIGraphicsPipelineVk : public IRHIGraphicsPipeline {
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
class RHIBufferVk: public IRHIBuffer {
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
	static RHIBufferVk* Create(IRHIDevice* device, uint32_t size, uint32_t usage, uint32_t memprop, RHISharingMode sharing);
	void Destroy(IRHIDevice *device);

    void* Map(IRHIDevice* device, uint32_t offset, uint32_t size, uint32_t map_flags);
    void Unmap(IRHIDevice* device);

    uint32_t Size() const { return buf_size_; }
	VkBuffer Handle() const { return handle_; }
};

////////////////////////////////////////////////////////////////////////////////
class RHIFenceVk : public IRHIFence {
        VkFence handle_;

        //TODO: add variables to cache the state

        RHIFenceVk() = default;
        ~RHIFenceVk() = default;
    public:
        virtual void Reset(IRHIDevice *device) override;
        virtual void Wait(IRHIDevice *device, uint64_t timeout) override;
        virtual bool IsSignalled(IRHIDevice *device) override;
        virtual void Destroy(IRHIDevice *device) override;

        VkFence Handle() const { return handle_; }

        static RHIFenceVk* Create(IRHIDevice *device, bool create_signalled);
};

////////////////////////////////////////////////////////////////////////////////
class RHIEventVk: public IRHIEvent {
        VkEvent handle_;
    public:
        // immediate set/reset by host
        virtual void Set(IRHIDevice *device) override;
        virtual void Reset(IRHIDevice *device) override;
        virtual bool IsSet(IRHIDevice *device) override;
        virtual void Destroy(IRHIDevice *device) override;

        VkEvent Handle() const { return handle_; }

        static RHIEventVk* Create(IRHIDevice *device);
};

////////////////////////////////////////////////////////////////////////////////
class RHICmdBufVk : public IRHICmdBuf {
	VkCommandBuffer cb_;
	bool is_recording_ = false;
public:
	RHICmdBufVk(VkCommandBuffer cb/*, uint32_t qfi, VkCommandPool cmd_pool*/) :
		cb_(cb) {}
	VkCommandBuffer Handle() const { return cb_; }

	virtual void Barrier_ClearToPresent(IRHIImage* image) override;
	virtual void Barrier_PresentToClear(IRHIImage* image) override;
	virtual void Barrier_PresentToDraw(IRHIImage* image) override;
	virtual void Barrier_DrawToPresent(IRHIImage* image) override;

	virtual void BufferBarrier(IRHIBuffer *i_buffer, RHIAccessFlags src_acc_flags,
							   RHIPipelineStageFlags src_stage, RHIAccessFlags dst_acc_fags,
							   RHIPipelineStageFlags dst_stage) override;

	virtual bool Begin() override;
	virtual bool BeginRenderPass(IRHIRenderPass *i_rp, IRHIFrameBuffer *i_fb, const ivec4 *render_area,
					   const RHIClearValue *clear_values, uint32_t count) override;
	virtual void Draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex,
					  uint32_t first_instance) override;
    virtual void BindVertexBuffers(IRHIBuffer** i_vb, uint32_t first_binding, uint32_t count) override;

	virtual void CopyBuffer(class IRHIBuffer *dst, uint32_t dst_offset, class IRHIBuffer *src,
							uint32_t src_offset, uint32_t size) override;

    virtual void SetEvent(IRHIEvent* event, RHIPipelineStageFlags stage) override;
    virtual void ResetEvent(IRHIEvent* event, RHIPipelineStageFlags stage) override;

	virtual bool End() override;
	virtual void EndRenderPass(const IRHIRenderPass *i_rp, IRHIFrameBuffer *i_fb) override;
	virtual void Clear(IRHIImage* image_in, const vec4& color, uint32_t img_aspect_bits) override;
	virtual void BindPipeline(RHIPipelineBindPoint bind_point, IRHIGraphicsPipeline* pipeline) override;

};

////////////////////////////////////////////////////////////////////////////////

class RHIFrameBufferVk : public IRHIFrameBuffer {
	VkFramebuffer handle_;
    std::vector<RHIImageViewVk*> attachments_;
	virtual ~RHIFrameBufferVk() = default;
public:
  RHIFrameBufferVk(VkFramebuffer fb, std::vector<RHIImageViewVk *> attachments)
	  : handle_(fb), attachments_(attachments) {}
  void Destroy(IRHIDevice *);
  VkFramebuffer Handle() const { return handle_; }
  const std::vector<RHIImageViewVk*> GetAttachments() const {
      return attachments_;
  }
};

class RHIRenderPassVk : public IRHIRenderPass {
	VkRenderPass handle_;
	std::vector<RHIImageLayout> att_final_layouts_;
	virtual ~RHIRenderPassVk() = default;

  public:
	RHIRenderPassVk(VkRenderPass rp, std::vector<RHIImageLayout> att_final_layouts)
		: handle_(rp), att_final_layouts_(att_final_layouts) {}
	void Destroy(IRHIDevice *device);
	VkRenderPass Handle() const { return handle_; }

	RHIImageLayout GetFinalLayout(uint32_t i) const {
		assert(att_final_layouts_.size() > i);
		return att_final_layouts_[i];
	}
};

class RHIDeviceVk : public IRHIDevice {
	VulkanDevice& dev_;

	// queue family index -> pool
	std::unordered_map<uint32_t, VkCommandPool> cmd_pools_;
	// queue family index -> cb
	std::unordered_map<uint32_t, VkCommandBuffer> cmd_buffers_;

	// 
	int32_t prev_frame_ = -1;
	int32_t cur_frame_ = -1;
	uint32_t cur_swap_chain_img_idx_ = 0xffffffff;
	bool between_begin_frame = false;

public:
	explicit RHIDeviceVk(VulkanDevice& device) : dev_(device) {}

	// interface implementation
	virtual ~RHIDeviceVk() override;
	virtual IRHICmdBuf* CreateCommandBuffer(RHIQueueType queue_type) override;
	virtual IRHIRenderPass* CreateRenderPass(const RHIRenderPassDesc* desc) override;
	virtual IRHIFrameBuffer* CreateFrameBuffer(RHIFrameBufferDesc* desc, const IRHIRenderPass* rp_in) override;
	virtual IRHIImageView* CreateImageView(const RHIImageViewDesc* desc) override;
	virtual IRHIBuffer* CreateBuffer(uint32_t size, uint32_t usage, uint32_t memprop, RHISharingMode sharing) override;

    virtual IRHIFence* CreateFence(bool create_signalled) override;
    virtual IRHIEvent* CreateEvent() override;

    virtual IRHIGraphicsPipeline *CreateGraphicsPipeline(
            const RHIShaderStage *shader_stage, uint32_t shader_stage_count,
            const RHIVertexInputState *vertex_input_state,
            const RHIInputAssemblyState *input_assembly_state, const RHIViewportState *viewport_state,
            const RHIRasterizationState *raster_state, const RHIMultisampleState *multisample_state,
            const RHIColorBlendState *color_blend_state, const IRHIPipelineLayout *i_pipleline_layout,
            const IRHIRenderPass *i_render_pass) override;

    virtual IRHIPipelineLayout* CreatePipelineLayout(IRHIDescriptorSetLayout* desc_set_layout) override;
    virtual IRHIShader* CreateShader(RHIShaderStageFlags stage, const uint32_t *pdata, uint32_t size) override;

	virtual RHIFormat GetSwapChainFormat() override;
	virtual uint32_t GetSwapChainSize() override { return (uint32_t)dev_.swap_chain_.images_.size(); }
	virtual class IRHIImageView* GetSwapChainImageView(uint32_t index) override;
	virtual class IRHIImage* GetSwapChainImage(uint32_t index) override;
	virtual IRHIImage* GetCurrentSwapChainImage() override;

    // TODO: this should belong to the renderer
    virtual uint32_t GetNumBufferedFrames() override { return rhi_vulkan::kNumBufferedFrames; }
    virtual uint32_t GetCurrentFrame() { return cur_frame_; }

	virtual bool Submit(IRHICmdBuf* cb_in, RHIQueueType queue_type) override;
	virtual bool BeginFrame() override;
	virtual bool Present() override;
	virtual bool EndFrame() override;

	VkDevice Handle() const { return dev_.device_; }
	VkPhysicalDevice PhysDeviceHandle() const { return dev_.phys_device_; }
	VkAllocationCallbacks* Allocator() const { return dev_.pallocator_; }
};
