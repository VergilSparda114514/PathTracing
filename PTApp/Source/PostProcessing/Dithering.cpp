#include "Dithering.h"

#include "../shared_with_shaders.h"

Dithering::Dithering()
{
	name = "Dithering";

	VkResult error = m_Params.Create(sizeof(DitheringParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	CHECK_VK_ERROR(error, "Dithering::m_Params.Create");

	m_Params.UploadData(new DitheringParams(), sizeof(DitheringParams));
}

void Dithering::Create(const VulkanHelpers::Image& image, std::shared_ptr<VulkanHelpers::PipelineCache> cache)
{
	m_ComputePass
		.BindImage(image)
		.BindUniform(m_Params)
		.CreatePipeline("dithering.comp", {}, cache);
}

void Dithering::OnUIRender(bool open)
{
	DitheringParams* params = m_Params.Map<DitheringParams>();

	if (open)
	{
		ImGui::DragInt("Bands", &params->bands, 1, 2, std::numeric_limits<int>::max());
	}

	m_Params.Unmap();
}