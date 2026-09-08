#pragma once
#include "swapchain.hpp"

#include <vk_mem_alloc.h>

namespace vkp
{
	struct ImageData
	{
		VkImage image = VK_NULL_HANDLE;
		VmaAllocation alloc = VK_NULL_HANDLE;
#ifndef NDEBUG
		VmaAllocationInfo info{};
#endif
	};

	struct SimpleImgInfo
	{
		VkImageCreateFlags flags;
		VkFormat format;
		VkExtent3D extent;
		VkSampleCountFlagBits samples;
		VkImageUsageFlags usage;

		[[nodiscard]] VkImageCreateInfo toVkImageCreateInfo() const
		{
			return VkImageCreateInfo{
				.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
				.pNext = nullptr,
				.flags = flags,
				.imageType = VK_IMAGE_TYPE_2D,
				.format = format,
				.extent = extent,
				.mipLevels = 1,
				.arrayLayers = 1,
				.samples = samples,
				.tiling = VK_IMAGE_TILING_OPTIMAL,
				.usage = usage,
				.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
				.queueFamilyIndexCount = 0,
				.pQueueFamilyIndices = nullptr,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
			};
		}
	};

	ImageData createImage(const device::DeviceData& p_DeviceData, const VkImageCreateInfo& p_ImageInfo, const VmaAllocationCreateInfo& p_AllocInfo);
	ImageData createImage(const device::DeviceData& p_DeviceData, const SimpleImgInfo& p_ImageInfo, const VmaAllocationCreateInfo& p_AllocInfo);

	ImageData createDepthBuffer(const vkp::device::DeviceData& p_DeviceData, const Swapchain& p_Swapchain, const VmaAllocationCreateInfo& p_AllocInfo);
	VkImageView createImageView(const device::DeviceData& p_DeviceData, VkImage p_Image, VkFormat p_Format, VkImageAspectFlags p_AspectFlags, VkImageViewType p_Type = VK_IMAGE_VIEW_TYPE_2D);

	VkSampler createSampler(const device::DeviceData& p_DeviceData, const VkSamplerCreateInfo& p_Info);
	VkSampler createSampler(const device::DeviceData& p_DeviceData, VkFilter p_MagFilter, VkFilter p_MinFilter, VkSamplerMipmapMode p_MipmapMode, VkSamplerAddressMode p_AddressMode, float p_MaxAnisotropy = 0.0f, float p_MaxLod = VK_LOD_CLAMP_NONE);
	void destroySampler(const device::DeviceData& p_DeviceData, VkSampler& p_Sampler);

	void uploadImage(device::DeviceData& p_DeviceData, VkCommandPool p_Pool, VkQueue p_Queue, const ImageData& p_Image, VkFormat p_Format, uint32_t p_Width, uint32_t p_Height, const void* p_Data, VkImageLayout p_FinalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}
