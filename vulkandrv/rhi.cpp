#include "vulkan_common.h"
#include "rhi.h"

#include <cassert>

IRHIGraphicsPipeline::~IRHIGraphicsPipeline() {}
IRHICmdBuf::~IRHICmdBuf() {}
IRHIRenderPass::~IRHIRenderPass() {}
IRHIFrameBuffer::~IRHIFrameBuffer() {}
IRHIPipelineLayout::~IRHIPipelineLayout() {}
IRHIShader::~IRHIShader() {}
IRHIBuffer::~IRHIBuffer() {}
IRHIFence::~IRHIFence() {}
IRHIEvent::~IRHIEvent() {}
IRHIImageView::~IRHIImageView() {}
IRHISampler::~IRHISampler() {}
IRHIDescriptorSetLayout::~IRHIDescriptorSetLayout() {}
IRHIDescriptorSet::~IRHIDescriptorSet() {}

// can move RHI independent members up to Interfaces  (e.g. getLayout() and implement these in
// platform independent way because we do not need platform dependent data here, or can just make
// add() functions virtual and each plarform will implement them as it wishes, not sure what is
// better for now
RHIDescriptorWriteDescBuilder &RHIDescriptorWriteDescBuilder::add(const IRHIDescriptorSet *ds,
																  int binding,
																  const IRHISampler *sampler) {
	assert(count_ > cur_index);
	const IRHIDescriptorSetLayout *layout = ds->getLayout();
	int count;
	const RHIDescriptorSetLayoutDesc *layout_desc = layout->getBindings(count);
	assert(binding < count); // is this a valid check?
	bool b_valid = false;
	int lidx = 0;
	for (int i = 0; i < count; i++) {
		if (layout_desc[i].binding == binding &&
			layout_desc[i].type == RHIDescriptorType::kSampler) {
			b_valid = true;
			lidx = i;
			break;
		}
	}
	assert(b_valid);
	assert(layout_desc[lidx].count == 1);

	desc_[cur_index].binding = binding;
	desc_[cur_index].type = RHIDescriptorType::kSampler;
	desc_[cur_index].set = ds;
	desc_[cur_index].img.sampler = sampler;
	desc_[cur_index].img.image_layout = RHIImageLayout::kUndefined;
	desc_[cur_index].img.image_view = nullptr;

	cur_index++;
	return *this;
};

RHIDescriptorWriteDescBuilder &RHIDescriptorWriteDescBuilder::add(const IRHIDescriptorSet *ds,
																  int binding,
																  const IRHISampler *sampler,
																  RHIImageLayout::Value img_layout,
																  const IRHIImageView *img_view) {

	assert(count_ > cur_index);
	assert(sampler && img_view);
	const IRHIDescriptorSetLayout *layout = ds->getLayout();
	int count;
	int lidx = 0;
	const RHIDescriptorSetLayoutDesc *layout_desc = layout->getBindings(count);
	assert(binding < count);
	bool b_valid = false;
	for (int i = 0; i < count; i++) {
		if (layout_desc[i].binding == binding &&
			layout_desc[i].type == RHIDescriptorType::kCombinedImageSampler) {
			b_valid = true;
			lidx = i;
			break;
		}
	}
	assert(b_valid);
	assert(layout_desc[lidx].count == 1);

	desc_[cur_index].binding = binding;
	desc_[cur_index].type = RHIDescriptorType::kCombinedImageSampler;
	desc_[cur_index].set = ds;
	desc_[cur_index].img.sampler = sampler;
	desc_[cur_index].img.image_layout = img_layout;
	desc_[cur_index].img.image_view = img_view;

	cur_index++;
	return *this;
}

RHIDescriptorWriteDescBuilder &
RHIDescriptorWriteDescBuilder::add(const IRHIDescriptorSet *ds, int binding,
								   const IRHIImageView *img_view,
								   RHIImageLayout::Value img_layout) {
	assert(count_ > cur_index);
	const IRHIDescriptorSetLayout *layout = ds->getLayout();
	int count;
	const RHIDescriptorSetLayoutDesc *layout_desc = layout->getBindings(count);
	assert(binding < count);
	bool b_valid = false;
	int lidx = 0;
	for (int i = 0; i < count; i++) {
		if (layout_desc[i].binding == binding &&
			(layout_desc[i].type == RHIDescriptorType::kSampledImage ||
			 layout_desc[i].type == RHIDescriptorType::kStorageBuffer ||
			 layout_desc[i].type == RHIDescriptorType::kInputAttachment)) {
			b_valid = true;
			lidx = i;
			break;
		}
	}
	assert(b_valid);
	assert(layout_desc[lidx].count == 1);

	desc_[cur_index].binding = binding;
	desc_[cur_index].type = layout_desc[lidx].type;
	desc_[cur_index].set = ds;
	desc_[cur_index].img.sampler = nullptr;
	desc_[cur_index].img.image_layout = img_layout;
	desc_[cur_index].img.image_view = img_view;

	cur_index++;
	return *this;
}

RHIDescriptorWriteDescBuilder &RHIDescriptorWriteDescBuilder::add(const IRHIDescriptorSet *ds,
																  int binding,
																  const IRHIBuffer *buffer,
																  uint64_t offset, uint64_t range) {
	assert(count_ > cur_index);
	const IRHIDescriptorSetLayout *layout = ds->getLayout();
	int count;
	const RHIDescriptorSetLayoutDesc *layout_desc = layout->getBindings(count);
	assert(binding < count);
	bool b_valid = false;
	int lidx = 0;
	for (int i = 0; i < count; i++) {
		if (layout_desc[i].binding == binding &&
			(layout_desc[i].type == RHIDescriptorType::kUniformBuffer ||
			 layout_desc[i].type == RHIDescriptorType::kUniformBufferDynamic ||
			 layout_desc[i].type == RHIDescriptorType::kStorageBuffer ||
			 layout_desc[i].type == RHIDescriptorType::kStorageBufferDynamic)) {
			b_valid = true;
			lidx = i;
			break;
		}
	}
	assert(b_valid);
	assert(layout_desc[lidx].count == 1);

	desc_[cur_index].binding = binding;
	desc_[cur_index].type = layout_desc[lidx].type;
	desc_[cur_index].set = ds;
	desc_[cur_index].buf.buffer = buffer;
	desc_[cur_index].buf.offset = offset;
	desc_[cur_index].buf.range = range;

	// TODO: check that offset + range < buffer.size

	cur_index++;
	return *this;
}
