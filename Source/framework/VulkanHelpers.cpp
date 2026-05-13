#include "VulkanHelpers.h"
#include <string>
#include <fstream>
#include <cstring> // for memcpy

#define STB_IMAGE_IMPLEMENTATION
// excluding old and unusefull formats
#define STBI_NO_PSD
#define STBI_NO_GIF
#define STBI_NO_PIC
#define STBI_NO_PNM

#include <stb_image.h>

namespace VulkanHelpers
{
	void Initialize(VkPhysicalDevice physicalDevice, VkDevice device, VkCommandPool commandPool, VkQueue transferQueue)
	{
		__details::s_PhysicalDevice = physicalDevice;
		__details::s_Device = device;
		__details::s_CommandPool = commandPool;
		__details::s_TransferQueue = transferQueue;

		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &__details::s_PhysicalDeviceMemoryProperties);
	}

	uint32_t GetMemoryType(VkMemoryRequirements& memoryRequiriments, VkMemoryPropertyFlags memoryProperties)
	{
		uint32_t result = 0;

		for (uint32_t memoryTypeIndex = 0; memoryTypeIndex < VK_MAX_MEMORY_TYPES; ++memoryTypeIndex)
		{
			if (memoryRequiriments.memoryTypeBits & (1 << memoryTypeIndex))
			{
				if ((__details::s_PhysicalDeviceMemoryProperties.memoryTypes[memoryTypeIndex].propertyFlags & memoryProperties) == memoryProperties)
				{
					result = memoryTypeIndex;
					break;
				}
			}
		}

		return result;
	}

	void ImageBarrier(VkCommandBuffer commandBuffer,
		VkImage image,
		VkImageSubresourceRange& subresourceRange,
		VkAccessFlags srcAccessMask,
		VkAccessFlags dstAccessMask,
		VkImageLayout oldLayout,
		VkImageLayout newLayout)
	{

		VkImageMemoryBarrier imageMemoryBarrier;
		imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageMemoryBarrier.pNext = nullptr;
		imageMemoryBarrier.srcAccessMask = srcAccessMask;
		imageMemoryBarrier.dstAccessMask = dstAccessMask;
		imageMemoryBarrier.oldLayout = oldLayout;
		imageMemoryBarrier.newLayout = newLayout;
		imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageMemoryBarrier.image = image;
		imageMemoryBarrier.subresourceRange = subresourceRange;

		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &imageMemoryBarrier);
	}

	VkCommandBuffer BeginSingleTimeCommandBuffer()
	{
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = __details::s_CommandPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(__details::s_Device, &allocInfo, &commandBuffer);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(commandBuffer, &beginInfo);

		return commandBuffer;
	}

	void EndSingleTimeCommandBuffer(VkCommandBuffer commandBuffer)
	{
		vkEndCommandBuffer(commandBuffer);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		vkQueueSubmit(__details::s_TransferQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(__details::s_TransferQueue);

		vkFreeCommandBuffers(__details::s_Device, __details::s_CommandPool, 1, &commandBuffer);
	}



	Buffer::~Buffer()
	{
		this->Destroy();
	}

	VkResult Buffer::Create(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryProperties)
	{
		VkResult result = VK_SUCCESS;

		VkBufferCreateInfo bufferCreateInfo;
		bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferCreateInfo.pNext = nullptr;
		bufferCreateInfo.flags = 0;
		bufferCreateInfo.size = size;
		bufferCreateInfo.usage = usage;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		bufferCreateInfo.queueFamilyIndexCount = 0;
		bufferCreateInfo.pQueueFamilyIndices = nullptr;

		m_Size = size;

		result = vkCreateBuffer(__details::s_Device, &bufferCreateInfo, nullptr, &m_Buffer);

		if (VK_SUCCESS == result)
		{
			VkMemoryRequirements memoryRequirements;
			vkGetBufferMemoryRequirements(__details::s_Device, m_Buffer, &memoryRequirements);

			VkMemoryAllocateInfo memoryAllocateInfo;
			memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			memoryAllocateInfo.pNext = nullptr;
			memoryAllocateInfo.allocationSize = memoryRequirements.size;
			memoryAllocateInfo.memoryTypeIndex = GetMemoryType(memoryRequirements, memoryProperties);

			VkMemoryAllocateFlagsInfo allocationFlags{};
			allocationFlags.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
			allocationFlags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

			if ((usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) == VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
			{
				memoryAllocateInfo.pNext = &allocationFlags;
			}

			result = vkAllocateMemory(__details::s_Device, &memoryAllocateInfo, nullptr, &m_Memory);

			if (VK_SUCCESS != result)
			{
				vkDestroyBuffer(__details::s_Device, m_Buffer, nullptr);
				m_Buffer = VK_NULL_HANDLE;
				m_Memory = VK_NULL_HANDLE;
			}

			else
			{
				result = vkBindBufferMemory(__details::s_Device, m_Buffer, m_Memory, 0);

				if (VK_SUCCESS != result)
				{
					vkDestroyBuffer(__details::s_Device, m_Buffer, nullptr);
					vkFreeMemory(__details::s_Device, m_Memory, nullptr);
					m_Buffer = VK_NULL_HANDLE;
					m_Memory = VK_NULL_HANDLE;
				}
			}
		}

		return result;
	}

	void Buffer::Destroy()
	{
		if (m_Buffer)
		{
			vkDestroyBuffer(__details::s_Device, m_Buffer, nullptr);
			m_Buffer = VK_NULL_HANDLE;
		}

		if (m_Memory)
		{
			vkFreeMemory(__details::s_Device, m_Memory, nullptr);
			m_Memory = VK_NULL_HANDLE;
		}
	}

	void* Buffer::Map(VkDeviceSize size, VkDeviceSize offset) const
	{
		void* mem = nullptr;

		if (size > m_Size)
		{
			size = m_Size;
		}

		VkResult result = vkMapMemory(__details::s_Device, m_Memory, offset, size, 0, &mem);

		if (VK_SUCCESS != result)
		{
			mem = nullptr;
		}

		return mem;
	}

	void Buffer::Unmap() const
	{
		vkUnmapMemory(__details::s_Device, m_Memory);
	}

	bool Buffer::UploadData(const void* data, VkDeviceSize size, VkDeviceSize offset) const
	{
		void* mem = this->Map(size, offset);

		if (mem)
		{
			std::memcpy(mem, data, size);
			this->Unmap();
		}

		return mem;
	}



	Image::~Image()
	{
		this->Destroy();
	}

	VkResult Image::Create(VkImageType imageType,
		VkFormat format,
		VkExtent3D extent,
		VkImageTiling tiling,
		VkImageUsageFlags usage,
		VkMemoryPropertyFlags memoryProperties)
	{
		VkResult result = VK_SUCCESS;

		m_Format = format;

		VkImageCreateInfo imageCreateInfo{};
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.imageType = imageType;
		imageCreateInfo.format = format;
		imageCreateInfo.extent = extent;
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = tiling;
		imageCreateInfo.usage = usage;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		result = vkCreateImage(__details::s_Device, &imageCreateInfo, nullptr, &m_Image);

		if (VK_SUCCESS == result)
		{
			VkMemoryRequirements memoryRequirements = {};
			vkGetImageMemoryRequirements(__details::s_Device, m_Image, &memoryRequirements);

			VkMemoryAllocateInfo memoryAllocateInfo = {};
			memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			memoryAllocateInfo.allocationSize = memoryRequirements.size;
			memoryAllocateInfo.memoryTypeIndex = GetMemoryType(memoryRequirements, memoryProperties);

			result = vkAllocateMemory(__details::s_Device, &memoryAllocateInfo, nullptr, &m_Memory);

			if (VK_SUCCESS != result)
			{
				vkDestroyImage(__details::s_Device, m_Image, nullptr);
				m_Image = VK_NULL_HANDLE;
				m_Memory = VK_NULL_HANDLE;
			}

			else
			{
				result = vkBindImageMemory(__details::s_Device, m_Image, m_Memory, 0);
				if (VK_SUCCESS != result) {
					vkDestroyImage(__details::s_Device, m_Image, nullptr);
					vkFreeMemory(__details::s_Device, m_Memory, nullptr);
					m_Image = VK_NULL_HANDLE;
					m_Memory = VK_NULL_HANDLE;
				}
			}
		}

		return result;
	}

	VkResult Image::CreateRGBA32(VkExtent3D extent)
	{
		Create(VK_IMAGE_TYPE_2D,
			VK_FORMAT_R32G32B32A32_SFLOAT,
			extent,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		CreateSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
		CreateImageView(VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT, range);

		return VK_SUCCESS;
	}

	void Image::Destroy()
	{
		if (m_Sampler)
		{
			vkDestroySampler(__details::s_Device, m_Sampler, nullptr);
			m_Sampler = VK_NULL_HANDLE;
		}

		if (m_ImageView)
		{
			vkDestroyImageView(__details::s_Device, m_ImageView, nullptr);
			m_ImageView = VK_NULL_HANDLE;
		}

		if (m_Memory)
		{
			vkFreeMemory(__details::s_Device, m_Memory, nullptr);
			m_Memory = VK_NULL_HANDLE;
		}

		if (m_Image)
		{
			vkDestroyImage(__details::s_Device, m_Image, nullptr);
			m_Image = VK_NULL_HANDLE;
		}
	}

	bool Image::Load(const char* fileName)
	{
		int width, height, channels;
		bool textureHDR = false;
		stbi_uc* imageData = nullptr;

		std::string fileNameString(fileName);
		const std::string extension = fileNameString.substr(fileNameString.length() - 3);

		if (extension == "hdr")
		{
			textureHDR = true;
			imageData = reinterpret_cast<stbi_uc*>(stbi_loadf(fileName, &width, &height, &channels, STBI_rgb_alpha));
		}

		else
		{
			imageData = stbi_load(fileName, &width, &height, &channels, STBI_rgb_alpha);
		}

		if (imageData)
		{
			const int bpp = textureHDR ? sizeof(float[4]) : sizeof(uint8_t[4]);
			VkDeviceSize imageSize = static_cast<VkDeviceSize>(width * height * bpp);

			Buffer stagingBuffer;
			VkResult error = stagingBuffer.Create(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

			if (VK_SUCCESS == error && stagingBuffer.UploadData(imageData, imageSize))
			{
				stbi_image_free(imageData);

				VkExtent3D imageExtent{
					static_cast<uint32_t>(width),
					static_cast<uint32_t>(height),
					1
				};

				const VkFormat fmt = textureHDR ? VK_FORMAT_R32G32B32A32_SFLOAT : VK_FORMAT_R8G8B8A8_SRGB;

				error = this->Create(VK_IMAGE_TYPE_2D, fmt, imageExtent, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
				
				if (VK_SUCCESS != error)
				{
					return false;
				}

				VkCommandBufferAllocateInfo allocInfo = {};
				allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
				allocInfo.commandPool = __details::s_CommandPool;
				allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
				allocInfo.commandBufferCount = 1;

				VkCommandBuffer commandBuffer;
				error = vkAllocateCommandBuffers(__details::s_Device, &allocInfo, &commandBuffer);
				
				if (VK_SUCCESS != error)
				{
					return false;
				}

				VkCommandBufferBeginInfo beginInfo = {};
				beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

				error = vkBeginCommandBuffer(commandBuffer, &beginInfo);
				
				if (VK_SUCCESS != error)
				{
					vkFreeCommandBuffers(__details::s_Device, __details::s_CommandPool, 1, &commandBuffer);
					return false;
				}

				VkImageMemoryBarrier barrier{};
				barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;;
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.image = m_Image;
				barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

				vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

				VkBufferImageCopy region{};
				region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
				region.imageExtent = imageExtent;

				vkCmdCopyBufferToImage(commandBuffer, stagingBuffer.GetBuffer(), m_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

				barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

				error = vkEndCommandBuffer(commandBuffer);

				if (VK_SUCCESS != error)
				{
					vkFreeCommandBuffers(__details::s_Device, __details::s_CommandPool, 1, &commandBuffer);
					return false;
				}

				VkSubmitInfo submitInfo = {};
				submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
				submitInfo.commandBufferCount = 1;
				submitInfo.pCommandBuffers = &commandBuffer;

				error = vkQueueSubmit(__details::s_TransferQueue, 1, &submitInfo, VK_NULL_HANDLE);

				if (VK_SUCCESS != error)
				{
					vkFreeCommandBuffers(__details::s_Device, __details::s_CommandPool, 1, &commandBuffer);
					return false;
				}

				error = vkQueueWaitIdle(__details::s_TransferQueue);

				if (VK_SUCCESS != error)
				{
					vkFreeCommandBuffers(__details::s_Device, __details::s_CommandPool, 1, &commandBuffer);
					return false;
				}

				vkFreeCommandBuffers(__details::s_Device, __details::s_CommandPool, 1, &commandBuffer);
			}

			else
			{
				stbi_image_free(imageData);

				return false;
			}

			return true;
		}

		return false;
	}

	VkResult Image::CreateImageView(VkImageViewType viewType, VkFormat format, VkImageSubresourceRange subresourceRange)
	{
		VkImageViewCreateInfo imageViewCreateInfo;
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.pNext = nullptr;
		imageViewCreateInfo.viewType = viewType;
		imageViewCreateInfo.format = format;
		imageViewCreateInfo.subresourceRange = subresourceRange;
		imageViewCreateInfo.image = m_Image;
		imageViewCreateInfo.flags = 0;
		imageViewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };

		return vkCreateImageView(__details::s_Device, &imageViewCreateInfo, nullptr, &m_ImageView);
	}

	VkResult Image::CreateSampler(VkFilter magFilter, VkFilter minFilter, VkSamplerMipmapMode mipmapMode, VkSamplerAddressMode addressMode)
	{
		VkSamplerCreateInfo samplerCreateInfo;
		samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCreateInfo.pNext = nullptr;
		samplerCreateInfo.flags = 0;
		samplerCreateInfo.magFilter = magFilter;
		samplerCreateInfo.minFilter = minFilter;
		samplerCreateInfo.mipmapMode = mipmapMode;
		samplerCreateInfo.addressModeU = addressMode;
		samplerCreateInfo.addressModeV = addressMode;
		samplerCreateInfo.addressModeW = addressMode;
		samplerCreateInfo.mipLodBias = 0;
		samplerCreateInfo.anisotropyEnable = VK_FALSE;
		samplerCreateInfo.maxAnisotropy = 1;
		samplerCreateInfo.compareEnable = VK_FALSE;
		samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerCreateInfo.minLod = 0;
		samplerCreateInfo.maxLod = 0;
		samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;

		return vkCreateSampler(__details::s_Device, &samplerCreateInfo, nullptr, &m_Sampler);
	}



	Shader::~Shader()
	{
		this->Destroy();
	}

	bool Shader::LoadFromFile(const char* fileName)
	{
		bool result = false;

		std::ifstream file(fileName, std::ios::ate | std::ios::binary);
		if (file)
		{
			const size_t fileSize = static_cast<size_t>(file.tellg());
			std::vector<char> buffer(fileSize);

			file.seekg(0);
			file.read(buffer.data(), fileSize);
			file.close();

			VkShaderModuleCreateInfo shaderModuleCreateInfo;
			shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			shaderModuleCreateInfo.pNext = nullptr;
			shaderModuleCreateInfo.codeSize = buffer.size();
			shaderModuleCreateInfo.pCode = reinterpret_cast<uint32_t*>(buffer.data());
			shaderModuleCreateInfo.flags = 0;

			const VkResult error = vkCreateShaderModule(__details::s_Device, &shaderModuleCreateInfo, nullptr, &m_Module);
			result = (VK_SUCCESS == error);
		}

		return result;
	}

	void Shader::Destroy()
	{
		if (m_Module)
		{
			vkDestroyShaderModule(__details::s_Device, m_Module, nullptr);
			m_Module = VK_NULL_HANDLE;
		}
	}

	VkPipelineShaderStageCreateInfo Shader::GetShaderStage(VkShaderStageFlagBits stage)
	{
		VkPipelineShaderStageCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		createInfo.pNext = nullptr;
		createInfo.flags = 0;
		createInfo.stage = stage;
		createInfo.module = m_Module;
		createInfo.pName = "main";
		createInfo.pSpecializationInfo = nullptr;

		return createInfo;
	}

	void ComputePass::CreatePipeline(const std::filesystem::path& path, VkPipelineLayout pipelineLayout)
	{
		m_Shader.LoadFromFile(path.string().c_str());

		VkComputePipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		pipelineInfo.layout = pipelineLayout;
		pipelineInfo.stage = m_Shader.GetShaderStage(VK_SHADER_STAGE_COMPUTE_BIT);

		VkResult error = vkCreateComputePipelines(__details::s_Device, VK_NULL_HANDLE, 1, &pipelineInfo, VK_NULL_HANDLE, &m_Pipeline);
		CHECK_VK_ERROR(error, "vkCreateComputePipelines");
	}

	void ComputePass::Dispatch(VkCommandBuffer commandBuffer, glm::uvec3 dimensions)
	{
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_Pipeline);
		vkCmdDispatch(commandBuffer, dimensions.x, dimensions.y, dimensions.z);
	}



	VkDeviceOrHostAddressKHR GetBufferDeviceAddress(const Buffer& buffer)
	{
		VkBufferDeviceAddressInfoKHR info{};
		info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		info.pNext = nullptr;
		info.buffer = buffer.GetBuffer();

		VkDeviceOrHostAddressKHR result;
		result.deviceAddress = PFN(vkGetBufferDeviceAddressKHR)(__details::s_Device, &info);

		return result;
	}

	VkDeviceOrHostAddressConstKHR GetBufferDeviceAddressConst(const Buffer& buffer)
	{
		VkDeviceOrHostAddressKHR address = GetBufferDeviceAddress(buffer);

		VkDeviceOrHostAddressConstKHR result;
		result.deviceAddress = address.deviceAddress;

		return result;
	}

} // namespace vulkanhelpers
