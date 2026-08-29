#pragma once

#include "PostProcessing.h"

class ColorAdjustment : public PostProcessingEffect
{
public:
	ColorAdjustment();

	virtual void CreateBindings(const VulkanHelpers::Image& image) override;
	virtual void OnUIRender() override;
private:
	VulkanHelpers::Buffer m_Params{};
};