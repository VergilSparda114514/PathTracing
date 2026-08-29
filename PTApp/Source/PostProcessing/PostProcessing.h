#pragma once

#include <VulkanHelpers.h>

#include <vector>
#include <memory>

class PostProcessingEffect
{
public:
	virtual ~PostProcessingEffect() = default;

	void Reset();

	virtual void Create(const VulkanHelpers::Image& image, std::shared_ptr<VulkanHelpers::PipelineCache> cache) = 0;
	virtual void Dispatch(VkCommandBuffer commandBuffer, VkExtent3D size);
	virtual void OnUIRender() {};
protected:
	VulkanHelpers::ComputePass m_ComputePass{};
};

class PostProcessor
{
public:
	using storage_t = std::vector<std::unique_ptr<PostProcessingEffect>>;
	using effect_t = typename storage_t::value_type;
public:
	~PostProcessor();

	void CreateImage(VkExtent3D extent);
	void DestroyImage();
	void CopyImage(VkCommandBuffer commandBuffer, const VulkanHelpers::Image& image);
	const VulkanHelpers::Image& GetImage() const { return m_Image; }

	void Dispatch(VkCommandBuffer commandBuffer, VkExtent3D size) const;
	void OnUIRender() const;

	void SetPipelineCache(std::shared_ptr<VulkanHelpers::PipelineCache> cache);

	template <typename T, typename... Args>
		requires std::derived_from<T, PostProcessingEffect>
	void PushEffect(Args&&... args);
private:
	VulkanHelpers::Image m_Image{};
	std::shared_ptr<VulkanHelpers::PipelineCache> m_PipelineCache = nullptr;
	storage_t m_Effects{};
};

template <typename T, typename... Args>
	requires std::derived_from<T, PostProcessingEffect>
inline void PostProcessor::PushEffect(Args&&... args)
{
	m_Effects.emplace_back(std::make_unique<T>(std::forward<Args>(args)...))->Create(m_Image, m_PipelineCache);
}