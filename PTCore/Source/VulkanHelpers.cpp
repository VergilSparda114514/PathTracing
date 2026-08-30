#include "VulkanHelpers.h"

#include <string>
#include <fstream>
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
// excluding old and unusefull formats
#define STBI_NO_PSD
#define STBI_NO_GIF
#define STBI_NO_PIC
#define STBI_NO_PNM

#include <stb_image.h>

static const std::filesystem::path s_ShadersFolder = "Source/Shaders/";

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

	// Buffer

	Buffer::~Buffer()
	{
		Destroy();
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
		void* mem = Map(size, offset);

		if (mem)
		{
			std::memcpy(mem, data, size);
			Unmap();
		}

		return mem;
	}

	// Image

	Image::~Image()
	{
		Destroy();
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
			VkMemoryRequirements memoryRequirements{};
			vkGetImageMemoryRequirements(__details::s_Device, m_Image, &memoryRequirements);

			VkMemoryAllocateInfo memoryAllocateInfo{};
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
		m_Extent = extent;

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

	bool Image::Load(const std::filesystem::path& fileName)
	{
		int width, height, channels;
		bool textureHDR = false;
		stbi_uc* imageData = nullptr;

		if (fileName.extension() == ".hdr")
		{
			textureHDR = true;
			imageData = reinterpret_cast<stbi_uc*>(stbi_loadf(fileName.string().c_str(), &width, &height, &channels, STBI_rgb_alpha));
		}

		else
		{
			imageData = stbi_load(fileName.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
		}

		if (imageData)
		{
			m_Extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1u};

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

				error = Create(VK_IMAGE_TYPE_2D, fmt, imageExtent, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

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

	void Image::Copy(VkCommandBuffer commandBuffer, const Image& other) const
	{
		VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

		VulkanHelpers::ImageBarrier(commandBuffer, other.GetImage(), range,
			VK_ACCESS_2_SHADER_WRITE_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
			VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

		VulkanHelpers::ImageBarrier(commandBuffer, GetImage(), range,
			VK_ACCESS_2_NONE, VK_ACCESS_2_TRANSFER_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		VkImageCopy region{};
		region.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		region.srcOffset = { 0, 0, 0 };
		region.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		region.dstOffset = { 0, 0, 0 };
		region.extent = GetExtent();

		vkCmdCopyImage(commandBuffer, other.GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
	}

	// Shader Includer

	shaderc_include_result* ShaderIncluder::GetInclude(const char* requested_source, shaderc_include_type type, const char* requesting_source, size_t include_depth)
	{
		std::filesystem::path requestedPath = type == shaderc_include_type_relative ? s_ShadersFolder / requested_source : requested_source;
		requestedPath = std::filesystem::weakly_canonical(requestedPath);

		std::ifstream file(requestedPath, std::ios::binary);
		std::string content(std::istreambuf_iterator<char>(file), {});
		file.close();

		IncludeData* data = new IncludeData{};
		data->name = requestedPath.string();
		data->content = std::move(content);

		shaderc_include_result* result = new shaderc_include_result{};
		result->user_data = data;
		result->source_name = data->name.c_str();
		result->source_name_length = data->name.length();
		result->content = data->content.c_str();
		result->content_length = data->content.length();

		return result;
	}

	void ShaderIncluder::ReleaseInclude(shaderc_include_result* data)
	{
		IncludeData* user_data = reinterpret_cast<IncludeData*>(data->user_data);

		delete user_data;
		delete data;
	}

	// Shader

	Shader::~Shader()
	{
		Destroy();
	}

	std::vector<uint32_t> Shader::Compile(const std::filesystem::path& fileName, shaderc_shader_kind kind, VkShaderStageFlagBits stage,
		const std::vector<std::pair<std::string, std::string>>& definitions, const VkSpecializationInfo* specializationConstants)
	{
#ifdef WL_DEBUG
		static_assert(false, "ShaderC does not support Debug build -- Please build under either the Release or Dist configurations");
#endif

		// Compile

		shaderc::Compiler compiler{};
		shaderc::CompileOptions options{};

		options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_4);
		options.SetOptimizationLevel(shaderc_optimization_level_performance);
		options.SetIncluder(std::make_unique<ShaderIncluder>());

		for (const auto& [name, value] : definitions)
		{
			options.AddMacroDefinition(name, value);
		}

		std::ifstream file(s_ShadersFolder / fileName);
		std::string src(std::istreambuf_iterator<char>(file), {});
		file.close();

		shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(src, kind, fileName.string().c_str(), options);

		if (result.GetCompilationStatus() != shaderc_compilation_status_success)
		{
			throw std::runtime_error(fileName.string() + '\n' + result.GetErrorMessage());
		}

		std::vector<uint32_t> data(result.cbegin(), result.cend());

		// Shader module

		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.pNext = nullptr;
		createInfo.pCode = data.data();
		createInfo.codeSize = data.size() * sizeof(uint32_t);
		createInfo.flags = 0;

		VkResult error = vkCreateShaderModule(__details::s_Device, &createInfo, nullptr, &m_Module);
		CHECK_VK_ERROR(error, "vkCreateShaderModule");

		// Shader stage

		m_Stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		m_Stage.pNext = nullptr;
		m_Stage.flags = 0;
		m_Stage.stage = stage;
		m_Stage.module = m_Module;
		m_Stage.pName = "main";
		m_Stage.pSpecializationInfo = specializationConstants;

		return data;
	}

	void Shader::Destroy()
	{
		if (m_Module)
		{
			vkDestroyShaderModule(__details::s_Device, m_Module, nullptr);
			m_Module = VK_NULL_HANDLE;
		}
	}

	// Compute pass

	ComputePass::~ComputePass()
	{
		Reset();
	}

	void ComputePass::Reset()
	{
		if (m_DescriptorSetLayout)
		{
			vkDestroyDescriptorSetLayout(__details::s_Device, m_DescriptorSetLayout, nullptr);
			m_DescriptorSetLayout = VK_NULL_HANDLE;
		}

		if (m_DescriptorSet)
		{
			vkFreeDescriptorSets(__details::s_Device, m_DescriptorPool, 1, &m_DescriptorSet);
			m_DescriptorSet = VK_NULL_HANDLE;
		}

		if (m_DescriptorPool)
		{
			vkDestroyDescriptorPool(__details::s_Device, m_DescriptorPool, nullptr);
			m_DescriptorPool = VK_NULL_HANDLE;
		}

		m_ImageInfos.clear();
		m_BufferInfos.clear();

		m_DescriptorSetLayoutBindings.clear();
		m_DescriptorWrites.clear();

		if (m_PipelineLayout)
		{
			vkDestroyPipelineLayout(__details::s_Device, m_PipelineLayout, nullptr);
		}

		if (m_Pipeline)
		{
			vkDestroyPipeline(__details::s_Device, m_Pipeline, nullptr);
		}
	}

	ComputePass& ComputePass::BindImage(const Image& image)
	{
		VkDescriptorSetLayoutBinding imageLayoutBinding = GetBinding();
		imageLayoutBinding.binding = static_cast<uint32_t>(m_DescriptorSetLayoutBindings.size());
		imageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

		m_DescriptorSetLayoutBindings.emplace_back(imageLayoutBinding);

		// Descriptor write

		m_ImageInfos.emplace_back(image.GetSampler(), image.GetImageView(), VK_IMAGE_LAYOUT_GENERAL);
		m_BufferInfos.emplace_back();

		VkWriteDescriptorSet imageWrite = GetWrite();
		imageWrite.dstBinding = static_cast<uint32_t>(m_DescriptorWrites.size());
		imageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

		m_DescriptorWrites.emplace_back(imageWrite);

		return *this;
	}

	ComputePass& ComputePass::BindSampler(const Image& image)
	{
		VkDescriptorSetLayoutBinding samplerLayoutBinding = GetBinding();
		samplerLayoutBinding.binding = static_cast<uint32_t>(m_DescriptorSetLayoutBindings.size());
		samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

		m_DescriptorSetLayoutBindings.emplace_back(samplerLayoutBinding);

		// Descriptor write

		m_ImageInfos.emplace_back(image.GetSampler(), image.GetImageView(), VK_IMAGE_LAYOUT_GENERAL);
		m_BufferInfos.emplace_back();

		VkWriteDescriptorSet samplerWrite = GetWrite();
		samplerWrite.dstBinding = static_cast<uint32_t>(m_DescriptorWrites.size());
		samplerWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

		m_DescriptorWrites.emplace_back(samplerWrite);

		return *this;
	}

	ComputePass& ComputePass::BindUniform(const Buffer& buffer)
	{
		VkDescriptorSetLayoutBinding uniformLayoutBinding = GetBinding();
		uniformLayoutBinding.binding = static_cast<uint32_t>(m_DescriptorSetLayoutBindings.size());
		uniformLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

		m_DescriptorSetLayoutBindings.emplace_back(uniformLayoutBinding);

		// Descriptor write

		m_ImageInfos.emplace_back();
		m_BufferInfos.emplace_back(buffer.GetBuffer(), 0, buffer.GetSize());

		VkWriteDescriptorSet uniformWrite = GetWrite();
		uniformWrite.dstBinding = static_cast<uint32_t>(m_DescriptorWrites.size());
		uniformWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

		m_DescriptorWrites.emplace_back(uniformWrite);

		return *this;
	}

	ComputePass& ComputePass::BindBuffer(const Buffer& buffer)
	{
		VkDescriptorSetLayoutBinding bufferLayoutBinding = GetBinding();
		bufferLayoutBinding.binding = static_cast<uint32_t>(m_DescriptorSetLayoutBindings.size());
		bufferLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

		m_DescriptorSetLayoutBindings.emplace_back(bufferLayoutBinding);

		// Descriptor write

		m_ImageInfos.emplace_back();
		m_BufferInfos.emplace_back(buffer.GetBuffer(), 0, buffer.GetSize());

		VkWriteDescriptorSet bufferWrite = GetWrite();
		bufferWrite.dstBinding = static_cast<uint32_t>(m_DescriptorWrites.size());
		bufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

		m_DescriptorWrites.emplace_back(bufferWrite);

		return *this;
	}

	void ComputePass::CreatePipeline(const std::filesystem::path& path, const std::vector<std::pair<std::string, std::string>>& definitions,
		std::shared_ptr<PipelineCache> pipelineCache)
	{
		// Create descriptor set layout

		constexpr VkDescriptorBindingFlags flag = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;

		VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags{};
		bindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
		bindingFlags.pNext = nullptr;
		bindingFlags.bindingCount = static_cast<uint32_t>(m_DescriptorSetLayoutBindings.size());
		bindingFlags.pBindingFlags = &flag;

		VkDescriptorSetLayoutCreateInfo setLayoutInfo{};
		setLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		setLayoutInfo.pNext = &bindingFlags;
		setLayoutInfo.flags = 0;
		setLayoutInfo.bindingCount = static_cast<uint32_t>(m_DescriptorSetLayoutBindings.size());
		setLayoutInfo.pBindings = m_DescriptorSetLayoutBindings.data();

		VkResult error = vkCreateDescriptorSetLayout(__details::s_Device, &setLayoutInfo, nullptr, &m_DescriptorSetLayout);
		CHECK_VK_ERROR(error, "vkCreateDescriptorSetLayout");

		// Create descriptor pool

		std::vector<VkDescriptorPoolSize> poolSizes{};
		poolSizes.reserve(m_DescriptorSetLayoutBindings.size());

		for (const auto& binding : m_DescriptorSetLayoutBindings)
		{
			poolSizes.emplace_back(binding.descriptorType, binding.descriptorCount);
		}

		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
		descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolCreateInfo.pNext = nullptr;
		descriptorPoolCreateInfo.flags = 0;
		descriptorPoolCreateInfo.maxSets = 1;
		descriptorPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		descriptorPoolCreateInfo.pPoolSizes = poolSizes.data();

		error = vkCreateDescriptorPool(__details::s_Device, &descriptorPoolCreateInfo, nullptr, &m_DescriptorPool);
		CHECK_VK_ERROR(error, "vkCreateDescriptorPool");

		// Allocate descriptor sets

		const uint32_t variableDescriptorCounts = 1;

		VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescriptorCountInfo{};
		variableDescriptorCountInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
		variableDescriptorCountInfo.pNext = nullptr;
		variableDescriptorCountInfo.descriptorSetCount = 1;
		variableDescriptorCountInfo.pDescriptorCounts = &variableDescriptorCounts; // actual number of descriptors

		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
		descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocateInfo.pNext = &variableDescriptorCountInfo;
		descriptorSetAllocateInfo.descriptorPool = m_DescriptorPool;
		descriptorSetAllocateInfo.descriptorSetCount = 1;
		descriptorSetAllocateInfo.pSetLayouts = &m_DescriptorSetLayout;

		error = vkAllocateDescriptorSets(__details::s_Device, &descriptorSetAllocateInfo, &m_DescriptorSet);
		CHECK_VK_ERROR(error, "vkAllocateDescriptorSets");

		for (auto& write : m_DescriptorWrites)
		{
			write.dstSet = m_DescriptorSet;
			write.pImageInfo = &m_ImageInfos[write.dstBinding];
			write.pBufferInfo = &m_BufferInfos[write.dstBinding];
		}

		// Update descriptor set

		vkUpdateDescriptorSets(__details::s_Device, static_cast<uint32_t>(m_DescriptorWrites.size()), m_DescriptorWrites.data(), 0, VK_NULL_HANDLE);

		// Create pipeline layout

		VkPipelineLayoutCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		createInfo.pNext = nullptr;
		createInfo.setLayoutCount = 1;
		createInfo.pSetLayouts = &m_DescriptorSetLayout;

		error = vkCreatePipelineLayout(__details::s_Device, &createInfo, nullptr, &m_PipelineLayout);
		CHECK_VK_ERROR(error, "vkCreatePipelineLayout");

		// Create pipeline

		m_Shader.Compile(path, shaderc_glsl_compute_shader, VK_SHADER_STAGE_COMPUTE_BIT, definitions);

		VkComputePipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		pipelineInfo.layout = m_PipelineLayout;
		pipelineInfo.stage = m_Shader.GetShaderStage();

		VkPipelineCache cache = pipelineCache ? pipelineCache->Get() : VK_NULL_HANDLE;

		error = vkCreateComputePipelines(__details::s_Device, cache, 1, &pipelineInfo, VK_NULL_HANDLE, &m_Pipeline);
		CHECK_VK_ERROR(error, "vkCreateComputePipelines");
	}

	void ComputePass::Dispatch(VkCommandBuffer commandBuffer, VkExtent3D dimensions) const
	{
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_PipelineLayout, 0, 1, &m_DescriptorSet, 0, nullptr);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_Pipeline);
		vkCmdDispatch(commandBuffer, dimensions.width, dimensions.height, dimensions.depth);
	}

	VkDescriptorSetLayoutBinding ComputePass::GetBinding()
	{
		VkDescriptorSetLayoutBinding binding{};
		binding.descriptorCount = 1;
		binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		binding.pImmutableSamplers = nullptr;

		return binding;
	}

	VkWriteDescriptorSet ComputePass::GetWrite()
	{
		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.pNext = nullptr;
		write.dstArrayElement = 0;
		write.descriptorCount = 1;
		write.pImageInfo = nullptr;
		write.pBufferInfo = nullptr;
		write.pTexelBufferView = nullptr;

		return write;
	}

	// Pipeline Cache

	PipelineCache::~PipelineCache()
	{
		if (m_PipelineCache)
		{
			size_t cacheSize = 0;
			vkGetPipelineCacheData(__details::s_Device, m_PipelineCache, &cacheSize, nullptr);

			std::vector<uint8_t> cacheData(cacheSize);
			vkGetPipelineCacheData(__details::s_Device, m_PipelineCache, &cacheSize, cacheData.data());

			std::ofstream out(m_FilePath, std::ios::binary);
			out.write(reinterpret_cast<const char*>(cacheData.data()), cacheData.size());

			vkDestroyPipelineCache(__details::s_Device, m_PipelineCache, nullptr);
		}
	}

	void PipelineCache::Create(const std::filesystem::path& fileName)
	{
		m_FilePath = "Resource" / fileName;

		VkPipelineCacheCreateInfo cacheInfo{};
		cacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

		if (std::filesystem::exists(m_FilePath))
		{
			std::vector<uint8_t> initialData{};

			std::ifstream in(m_FilePath, std::ios::binary | std::ios::ate);

			const std::streamsize size = in.tellg();

			if (size)
			{
				initialData.resize(size);

				in.seekg(0);

				in.read(reinterpret_cast<char*>(initialData.data()), size);
			}

			cacheInfo.pInitialData = initialData.data();
			cacheInfo.initialDataSize = initialData.size();
		}

		VkResult error = vkCreatePipelineCache(__details::s_Device, &cacheInfo, nullptr, &m_PipelineCache);
		CHECK_VK_ERROR(error, "vkCreatePipelineCache");
	}

	// Helper functions

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
}