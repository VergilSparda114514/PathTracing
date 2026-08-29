#include "Bloom.h"

void Bloom::Create(const VulkanHelpers::Image& image, std::shared_ptr<VulkanHelpers::PipelineCache> cache)
{
	m_ComputePass
		.BindImage(image);
}

void Bloom::OnUIRender()
{

}