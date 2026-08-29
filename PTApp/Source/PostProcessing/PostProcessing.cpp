#include "PostProcessing.h"

PostProcessor::~PostProcessor()
{
	DestroyImage();
}

void PostProcessor::CreateImage(VkExtent3D extent)
{
	m_Image.CreateRGBA32(extent);

	for (const auto& effect : m_Effects)
	{
		effect->Reset();
		effect->Create(m_Image, m_PipelineCache);
	}
}

void PostProcessor::DestroyImage()
{
	m_Image.Destroy();
}

void PostProcessor::CopyImage(VkCommandBuffer commandBuffer, const VulkanHelpers::Image& image)
{
	VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

	VulkanHelpers::ImageBarrier(commandBuffer, image.GetImage(), range,
		VK_ACCESS_2_SHADER_WRITE_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

	VulkanHelpers::ImageBarrier(commandBuffer, m_Image.GetImage(), range,
		VK_ACCESS_2_NONE, VK_ACCESS_2_TRANSFER_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	VkImageCopy region{};
	region.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	region.srcOffset = { 0, 0, 0 };
	region.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	region.dstOffset = { 0, 0, 0 };
	region.extent = m_Image.GetExtent();

	vkCmdCopyImage(commandBuffer, image.GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_Image.GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

void PostProcessor::Dispatch(VkCommandBuffer commandBuffer, VkExtent3D size) const
{
	for (const auto& effect : m_Effects)
	{
		effect->Dispatch(commandBuffer, size);
	}
}

void PostProcessor::OnUIRender() const
{
	for (const auto& effect : m_Effects)
	{
		effect->OnUIRender();
	}
}

void PostProcessor::SetPipelineCache(std::shared_ptr<VulkanHelpers::PipelineCache> cache)
{
	m_PipelineCache = cache;
}

void PostProcessingEffect::Reset()
{
	m_ComputePass.Reset();
}

void PostProcessingEffect::Dispatch(VkCommandBuffer commandBuffer, VkExtent3D size)
{
	m_ComputePass.Dispatch(commandBuffer, size);
}