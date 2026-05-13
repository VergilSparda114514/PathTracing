#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include <filesystem>
#include <cassert>

#define CHECK_VK_ERROR(_error, _message) do {   \
    if (VK_SUCCESS != error) {                  \
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
        bool        Load(const char* fileName);
        VkResult    CreateImageView(VkImageViewType viewType, VkFormat format, VkImageSubresourceRange subresourceRange);
        VkResult    CreateSampler(VkFilter magFilter, VkFilter minFilter, VkSamplerMipmapMode mipmapMode, VkSamplerAddressMode addressMode);

        // getters
        VkFormat    GetFormat() const { return m_Format; }
        VkImage     GetImage() const { return m_Image; }
        VkImageView GetImageView() const { return m_ImageView; }
        VkSampler   GetSampler() const { return m_Sampler; }

    private:
        VkFormat        m_Format = VK_FORMAT_B8G8R8A8_UNORM;
        VkImage         m_Image = VK_NULL_HANDLE;
        VkDeviceMemory  m_Memory = VK_NULL_HANDLE;
        VkImageView     m_ImageView = VK_NULL_HANDLE;
        VkSampler       m_Sampler = VK_NULL_HANDLE;
    };


    class Shader
    {
    public:
        ~Shader();

        bool    LoadFromFile(const char* fileName);
        void    Destroy();

        VkPipelineShaderStageCreateInfo GetShaderStage(VkShaderStageFlagBits stage);

    private:
        VkShaderModule  m_Module = VK_NULL_HANDLE;
    };


    class ComputePass
    {
    public:
        virtual ~ComputePass() = default;

        void CreatePipeline(const std::filesystem::path& path, VkPipelineLayout pipelineLayout);

        void Dispatch(VkCommandBuffer commandBuffer, glm::uvec3 dimensions);
        template <class PushConstants>
        void Dispatch(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, glm::uvec3 dimensions, const PushConstants* pc);
    protected:
        Shader m_Shader;
        VkPipeline m_Pipeline = VK_NULL_HANDLE;
    };

    template<class PushConstants>
    inline void ComputePass::Dispatch(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, glm::uvec3 dimensions, const PushConstants* pc)
    {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_Pipeline);
        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstants), pc);
        vkCmdDispatch(commandBuffer, dimensions.x, dimensions.y, dimensions.z);
    }



    VkDeviceOrHostAddressKHR GetBufferDeviceAddress(const Buffer& buffer);
    VkDeviceOrHostAddressConstKHR GetBufferDeviceAddressConst(const Buffer& buffer);

} // namespace vulkanhelpers
