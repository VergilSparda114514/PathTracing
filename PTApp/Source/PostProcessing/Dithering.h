#pragma once

#include "PostProcessing.h"

class Dithering : public PostProcessingEffect
{
public:
	Dithering();

	virtual void Create(const VulkanHelpers::Image& image, std::shared_ptr<VulkanHelpers::PipelineCache> cache) override;
	virtual void OnUIRender(bool open) override;
private:
	VulkanHelpers::Buffer m_Params{};
};