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
	std::pair<storage_t::iterator, storage_t::iterator> to_swap = { m_Effects.end(), m_Effects.end() };

	for (auto it = m_Effects.begin(); it != m_Effects.end(); it++)
	{
		ImGui::PushID(std::distance(m_Effects.begin(), it));

		bool open = ImGui::TreeNode((*it)->name.c_str());

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

		if (it != m_Effects.begin())
		{
			ImGui::SameLine();

			if (ImGui::Button("^"))
			{
				to_swap.first = it;
				to_swap.second = std::prev(it);
			}
		}

		if (std::next(it) != m_Effects.end())
		{
			ImGui::SameLine();

			if (ImGui::Button("v"))
			{
				to_swap.first = it;
				to_swap.second = std::next(it);
			}
		}

		ImGui::SameLine();

		if (ImGui::Button("-"))
		{
			to_remove = it;
		}

		ImGui::PopStyleColor();

		(*it)->OnUIRender(open);

		if (open)
		{
			ImGui::TreePop();
		}

		ImGui::PopID();
	}

	if (to_remove != m_Effects.end())
	{
		m_Effects.erase(to_remove);

		return true;
	}

	if (to_swap.first != m_Effects.end())
	{
		std::swap(*to_swap.first, *to_swap.second);

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