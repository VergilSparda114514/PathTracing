#include "ColorAdjustment.h"

#include <imgui.h>

#include "../shared_with_shaders.h"

ColorAdjustment::ColorAdjustment()
{
	VkResult error = m_Params.Create(sizeof(ColorAdjustmentParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	CHECK_VK_ERROR(error, "ColorAdjustment::m_Params.Create");

	m_Params.UploadData(new ColorAdjustmentParams(), sizeof(ColorAdjustmentParams));
}

void ColorAdjustment::Create(const VulkanHelpers::Image& image, std::shared_ptr<VulkanHelpers::PipelineCache> cache)
{
	m_ComputePass
		.BindImage(image)
		.BindUniform(m_Params)
		.CreatePipeline("color_adjustment.comp", {}, cache);
}

void ColorAdjustment::OnUIRender()
{
	ColorAdjustmentParams* ppParams = reinterpret_cast<ColorAdjustmentParams*>(m_Params.Map());

	if (ImGui::TreeNode("Colour Adjustment"))
	{
		const char* items[] =
		{
			"None",
			"Neutral",
			"ACES"
		};

		ImGui::Combo("Tone Mapping", &ppParams->toneMappingMode, items, IM_ARRAYSIZE(items));
		ImGui::SliderFloat("Exposure", &ppParams->exposure, 0.0f, 10.0f);
		ImGui::SliderFloat("Contrast", &ppParams->contrast, 0.0f, 10.0f);
		ImGui::SliderFloat("Saturation", &ppParams->saturation, 0.0f, 10.0f);

		ImGui::TreePop();
	}

	m_Params.Unmap();
}