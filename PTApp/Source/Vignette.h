#pragma once

#include "PostProcessing.h"

class Vignette : public PostProcessingEffect
{
public:
	Vignette();

	virtual void CreateBindings(const VulkanHelpers::Image& image) override;
	virtual void Dispatch(VkCommandBuffer commandBuffer, VkExtent3D size) override;
	virtual void OnUIRender() override;
private:
	VulkanHelpers::Buffer m_Params{};
};