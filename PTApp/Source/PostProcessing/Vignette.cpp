#include "Vignette.h"

#include <imgui.h>

#include "../shared_with_shaders.h"

Vignette::Vignette()
{
	VkResult error = m_Params.Create(sizeof(VignetteParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	CHECK_VK_ERROR(error, "Vignette::m_Params.Create");

	m_Params.UploadData(new VignetteParams(), sizeof(VignetteParams));
}

void Vignette::CreateBindings(const VulkanHelpers::Image& image)
{
	m_ComputePass
		.BindImage(image)
		.BindUniform(m_Params)
		.CreatePipeline("vignette.comp", {}, std::nullopt);
}

void Vignette::OnUIRender()
{
	VignetteParams* params = reinterpret_cast<VignetteParams*>(m_Params.Map());

	if (ImGui::CollapsingHeader("Vignette"))
	{
		ImGui::DragFloat("Intensity", &params->intensity, 0.1f, 0.0f, std::numeric_limits<float>::max());
	}

	m_Params.Unmap();
}