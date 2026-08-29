#pragma once

#include "PostProcessing.h"

class Bloom : public PostProcessingEffect
{
public:
	Bloom();

	virtual void Create(const VulkanHelpers::Image& image, std::shared_ptr<VulkanHelpers::PipelineCache> cache) override;
	virtual void OnUIRender() override;
private:
	VulkanHelpers::Buffer m_Params{};

	VulkanHelpers::Image m_KernelImage{};
	VulkanHelpers::ComputePass m_KernelPass{};
};