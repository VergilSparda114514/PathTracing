#pragma once

#include "PostProcessing.h"

class Vignette : public PostProcessingEffect
{
public:
	Vignette();

	virtual void Create(const VulkanHelpers::Image& image, std::shared_ptr<VulkanHelpers::PipelineCache> cache) override;
	virtual void OnUIRender(bool open) override;
private:
	VulkanHelpers::Buffer m_Params{};
};