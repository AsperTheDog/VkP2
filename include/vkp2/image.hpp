#pragma once

#include <vk_mem_alloc.h>
#include <volk.h>

#include "device.hpp"

namespace vkp
{
	struct ImageData
	{
		VkImage image = VK_NULL_HANDLE;
		VmaAllocation alloc = VK_NULL_HANDLE;
		VmaAllocationInfo info{};
	};

	struct ImageProperties
	{
		VkImageCreateFlags flags = 0;
		VkImageType imageType = VK_IMAGE_TYPE_2D;
		VkFormat format = VK_FORMAT_UNDEFINED;
		VkExtent3D extent{};
		uint32_t mipLevels = 1;
		uint32_t arrayLayers = 1;
		VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
		VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
		VkImageUsageFlags usage = 0;
		VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		[[nodiscard]] VkImageCreateInfo toVkImageCreateInfo() const;

		[[nodiscard]] VkImageAspectFlags aspectMask() const;
		[[nodiscard]] VkImageSubresourceRange subresourceRange() const;

		[[nodiscard]] static VkImageAspectFlags aspectMaskForFormat(VkFormat p_Format);
	};

	struct Image
	{
		ImageData data{};
		ImageProperties properties{};
	};

	Image createImage(const device::DeviceData& p_DeviceData, const VkImageCreateInfo& p_ImageInfo, const VmaAllocationCreateInfo& p_AllocInfo);
	Image createImage(const device::DeviceData& p_DeviceData, const ImageProperties& p_Properties, const VmaAllocationCreateInfo& p_AllocInfo);

	Image createDepthBuffer(const device::DeviceData& p_DeviceData, VkExtent2D p_Extent, const VmaAllocationCreateInfo& p_AllocInfo);
	void destroyImage(const device::DeviceData& p_DeviceData, Image& p_Image);

	VkImageView createImageView(const device::DeviceData& p_DeviceData, VkImage p_Image, VkFormat p_Format, VkImageAspectFlags p_AspectFlags, VkImageViewType p_Type = VK_IMAGE_VIEW_TYPE_2D);
	VkImageView createImageView(const device::DeviceData& p_DeviceData, const Image& p_Image, VkImageViewType p_Type = VK_IMAGE_VIEW_TYPE_2D);
	void destroyImageView(const device::DeviceData& p_DeviceData, VkImageView& p_ImageView);

	VkSampler createSampler(const device::DeviceData& p_DeviceData, const VkSamplerCreateInfo& p_Info);
	VkSampler createSampler(const device::DeviceData& p_DeviceData, VkFilter p_MagFilter, VkFilter p_MinFilter, VkSamplerMipmapMode p_MipmapMode, VkSamplerAddressMode p_AddressMode, float p_MaxAnisotropy = 0.0f, float p_MaxLod = VK_LOD_CLAMP_NONE);
	void destroySampler(const device::DeviceData& p_DeviceData, VkSampler& p_Sampler);

	void uploadImage(device::DeviceData& p_DeviceData, VkCommandPool p_Pool, VkQueue p_Queue, const Image& p_Image, const void* p_Data, VkDeviceSize p_Size, VkImageLayout p_FinalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}
