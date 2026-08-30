#include "PostProcessing.h"

PostProcessor::~PostProcessor()
{
	DestroyImage();
}

void PostProcessor::CreateImage(VkExtent3D extent)
{
	m_Image.CreateRGBA32(extent);

	for (const auto& effect : m_Effects)
	{
		effect->Reset();
		effect->Create(m_Image, m_PipelineCache);
	}
}

void PostProcessor::DestroyImage()
{
	m_Image.Destroy();
}

void PostProcessor::CopyImage(VkCommandBuffer commandBuffer, const VulkanHelpers::Image& image)
{
	m_Image.Copy(commandBuffer, image);
}

void PostProcessor::Dispatch(VkCommandBuffer commandBuffer, VkExtent3D size) const
{
	for (const auto& effect : m_Effects)
	{
		effect->Dispatch(commandBuffer, size);
	}
}

bool PostProcessor::OnUIRender()
{
	auto to_remove = m_Effects.end();

	for (auto it = m_Effects.begin(); it != m_Effects.end(); it++)
	{
		bool open = ImGui::TreeNode((*it)->name.c_str());

		(*it)->OnUIRender(open);

		if (open)
		{
			if (ImGui::Button("Remove"))
			{
				to_remove = it;
			}

			ImGui::TreePop();
		}
	}

	if (to_remove != m_Effects.end())
	{
		m_Effects.erase(to_remove);

		return true;
	}

	return false;
}

void PostProcessor::SetPipelineCache(std::shared_ptr<VulkanHelpers::PipelineCache> cache)
{
	m_PipelineCache = cache;
}

void PostProcessingEffect::Reset()
{
	m_ComputePass.Reset();
}

void PostProcessingEffect::Dispatch(VkCommandBuffer commandBuffer, VkExtent3D size)
{
	m_ComputePass.Dispatch(commandBuffer, size);
}