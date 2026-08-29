#pragma once

#include "PostProcessing.h"

class ColorAdjustment : public PostProcessingEffect
{
public:
	ColorAdjustment();

	virtual void Create(const VulkanHelpers::Image& image, std::shared_ptr<VulkanHelpers::PipelineCache> cache) override;
	virtual void OnUIRender() override;
private:
	VulkanHelpers::Buffer m_Params{};
};