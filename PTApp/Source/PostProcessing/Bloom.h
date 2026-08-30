#pragma once

#include "PostProcessing.h"

#include <filesystem>

class Bloom : public PostProcessingEffect
{
public:
	Bloom(const std::filesystem::path& kernelImageName);

	virtual void Create(const VulkanHelpers::Image& image, std::shared_ptr<VulkanHelpers::PipelineCache> cache) override;
	virtual void OnUIRender(bool open) override;
private:
	VulkanHelpers::Buffer m_Params{};

	VulkanHelpers::Image m_KernelImage{};
	VulkanHelpers::ComputePass m_KernelPass{};
};