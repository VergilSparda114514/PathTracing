#pragma once

#include <vulkan/vulkan.h>
#include <shaderc/shaderc.hpp>

#include <glm/glm.hpp>

#include <filesystem>
#include <cassert>

#define CHECK_VK_ERROR(_error, _message) do {   \
    if (VK_SUCCESS != _error) {                 \
        assert(false && _message);              \
    }                                           \
} while (false)

#define PFN(name) ((PFN_##name)vkGetDeviceProcAddr(__details::s_Device, #name))

namespace VulkanHelpers
{
    namespace __details
    {
        static VkPhysicalDevice                 s_PhysicalDevice;
        static VkDevice                         s_Device;
        static VkCommandPool                    s_CommandPool;
        static VkQueue                          s_TransferQueue;
        static VkPhysicalDeviceMemoryProperties s_PhysicalDeviceMemoryProperties;
    } // namespace __details

    void     Initialize(VkPhysicalDevice physicalDevice, VkDevice device, VkCommandPool commandPool, VkQueue transferQueue);
    uint32_t GetMemoryType(VkMemoryRequirements& memoryRequiriments, VkMemoryPropertyFlags memoryProperties);
    void     ImageBarrier(VkCommandBuffer commandBuffer,
                          VkImage image,
                          VkImageSubresourceRange& subresourceRange,
                          VkAccessFlags srcAccessMask,
                          VkAccessFlags dstAccessMask,
                          VkImageLayout oldLayout,
                          VkImageLayout newLayout);
    VkCommandBuffer BeginSingleTimeCommandBuffer();
    void EndSingleTimeCommandBuffer(VkCommandBuffer commandBuffer);


    class Buffer
    {
    public:
        ~Buffer();

        VkResult        Create(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryProperties);
        void            Destroy();

        void*           Map(VkDeviceSize size = UINT64_MAX, VkDeviceSize offset = 0) const;
        void            Unmap() const;

        bool            UploadData(const void* data, VkDeviceSize size, VkDeviceSize offset = 0) const;

        // getters
        VkBuffer        GetBuffer() const { return m_Buffer; }
        VkDeviceSize    GetSize() const { return m_Size; }

    private:
        VkBuffer        m_Buffer = VK_NULL_HANDLE;
        VkDeviceMemory  m_Memory = VK_NULL_HANDLE;
        VkDeviceSize    m_Size = 0;
    };



    class Image
    {
    public:
        ~Image();

        VkResult    Create(VkImageType imageType,
                           VkFormat format,
                           VkExtent3D extent,
                           VkImageTiling tiling,
                           VkImageUsageFlags usage,
                           VkMemoryPropertyFlags memoryProperties);

        VkResult	CreateRGBA32(VkExtent3D extent);

        void        Destroy();
        bool        Load(const std::filesystem::path& fileName);
        VkResult    CreateImageView(VkImageViewType viewType, VkFormat format, VkImageSubresourceRange subresourceRange);
        VkResult    CreateSampler(VkFilter magFilter, VkFilter minFilter, VkSamplerMipmapMode mipmapMode, VkSamplerAddressMode addressMode);
        void        CreateDescriptorSet();

        // getters
        VkFormat    GetFormat() const { return m_Format; }
        VkImage     GetImage() const { return m_Image; }
        VkImageView GetImageView() const { return m_ImageView; }
        VkSampler   GetSampler() const { return m_Sampler; }
        VkDescriptorSet GetDescriptorSet() const { return m_DescriptorSet; }
		VkExtent3D GetExtent() const { return { m_Width, m_Height, 1u }; }
    private:
        // For Vulkan

        VkFormat        m_Format = VK_FORMAT_B8G8R8A8_UNORM;
        VkImage         m_Image = VK_NULL_HANDLE;
        VkDeviceMemory  m_Memory = VK_NULL_HANDLE;
        VkImageView     m_ImageView = VK_NULL_HANDLE;
        VkSampler       m_Sampler = VK_NULL_HANDLE;

        // For Dear ImGui

        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
        VkDescriptorSet m_DescriptorSet = VK_NULL_HANDLE;
    };



    class ShaderIncluder : public shaderc::CompileOptions::IncluderInterface
    {
    public:
        shaderc_include_result* GetInclude(const char* requested_source, shaderc_include_type type, const char* requesting_source, size_t include_depth) override;

        void ReleaseInclude(shaderc_include_result* data) override;
    private:
        struct IncludeData
        {
            std::string name;
            std::string content;
        };
    };



    class Shader
    {
    public:
        ~Shader();

        std::vector<uint32_t> Compile(const std::filesystem::path& fileName, shaderc_shader_kind kind);
        void    Destroy();

        VkPipelineShaderStageCreateInfo GetShaderStage(VkShaderStageFlagBits stage);

    private:
        VkShaderModule  m_Module = VK_NULL_HANDLE;
    };



    class PipelineCache
    {
    public:
        ~PipelineCache();

        VkPipelineCache Get() const { return m_PipelineCache; }

        void Create(const std::filesystem::path& path);
    private:
        VkPipelineCache m_PipelineCache = VK_NULL_HANDLE;

        std::filesystem::path m_FilePath{};
    };



    class ComputePass
    {
    public:
        ~ComputePass();

        void Reset();

        ComputePass& BindImage(const Image& image);
        ComputePass& BindSampler(const Image& image);
        ComputePass& BindUniform(const Buffer& buffer);
        ComputePass& BindBuffer(const Buffer& buffer);

        void CreatePipeline(const std::filesystem::path& path, const PipelineCache& pipelineCache);

        void Dispatch(VkCommandBuffer commandBuffer, VkExtent3D dimensions) const;
        template <class PushConstant>
        void Dispatch(VkCommandBuffer commandBuffer, VkExtent3D dimensions, const PushConstant* pc) const;
    private:
        static VkDescriptorSetLayoutBinding GetBinding();
        static VkWriteDescriptorSet GetWrite();
    private:
        Shader m_Shader{};

        VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorSet m_DescriptorSet = VK_NULL_HANDLE;

        std::vector<VkDescriptorImageInfo> m_ImageInfos{};
        std::vector<VkDescriptorBufferInfo> m_BufferInfos{};

		std::vector<VkDescriptorSetLayoutBinding> m_DescriptorSetLayoutBindings{};
        std::vector<VkWriteDescriptorSet> m_DescriptorWrites{};

		VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
        VkPipeline m_Pipeline = VK_NULL_HANDLE;
    };

    template <class PushConstant>
    inline void ComputePass::Dispatch(VkCommandBuffer commandBuffer, VkExtent3D dimensions, const PushConstant* pc) const
    {
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_PipelineLayout, 0, 1, &m_DescriptorSet, 0, nullptr);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_Pipeline);
        vkCmdPushConstants(commandBuffer, m_PipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstant), pc);
        vkCmdDispatch(commandBuffer, dimensions.width, dimensions.height, dimensions.depth);
    }



    VkDeviceOrHostAddressKHR GetBufferDeviceAddress(const Buffer& buffer);
    VkDeviceOrHostAddressConstKHR GetBufferDeviceAddressConst(const Buffer& buffer);

} // namespace vulkanhelpers
