#include "Bloom.h"

#include "../shared_with_shaders.h"

Bloom::Bloom(const std::filesystem::path& kernelImageName)
{
	name = "Bloom";

	VkResult error = m_Params.Create(sizeof(BloomParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	CHECK_VK_ERROR(error, "Bloom::m_Params.Create");

	m_Params.UploadData(new BloomParams(), sizeof(BloomParams));

	m_KernelImage.Load(kernelImageName);
}

void Bloom::Create(const VulkanHelpers::Image& image, std::shared_ptr<VulkanHelpers::PipelineCache> cache)
{
	m_ComputePass
		.BindImage(image);
}

void Bloom::OnUIRender(bool open)
{
	BloomParams* params = m_Params.Map<BloomParams>();

	if (open)
	{
		ImGui::SliderFloat("Intensity", &params->intensity, 0.0f, 1.0f);
	}

	m_Params.Unmap();
}