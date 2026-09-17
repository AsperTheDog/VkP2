#include "image.hpp"

#include <cstring>

#include "base.hpp"
#include "buffer.hpp"
#include "command_buffer.hpp"
#include "device.hpp"

namespace vkp
{

	VkImageCreateInfo ImageProperties::toVkImageCreateInfo() const
	{
		return VkImageCreateInfo{
			.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = flags,
			.imageType = extent.height == 1 && extent.depth == 1 ? VK_IMAGE_TYPE_1D : (extent.depth == 1 ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_3D),
			.format = format,
			.extent = extent,
			.mipLevels = mipLevels,
			.arrayLayers = arrayLayers,
			.samples = samples,
			.tiling = tiling,
			.usage = usage,
			.sharingMode = sharingMode,
			.queueFamilyIndexCount = 0,
			.pQueueFamilyIndices = nullptr,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		};
	}

	VkImageAspectFlags ImageProperties::aspectMaskForFormat(const VkFormat p_Format)
	{
		switch (p_Format)
		{
		case VK_FORMAT_D16_UNORM:
		case VK_FORMAT_X8_D24_UNORM_PACK32:
		case VK_FORMAT_D32_SFLOAT:
			return VK_IMAGE_ASPECT_DEPTH_BIT;
		case VK_FORMAT_S8_UINT:
			return VK_IMAGE_ASPECT_STENCIL_BIT;
		case VK_FORMAT_D16_UNORM_S8_UINT:
		case VK_FORMAT_D24_UNORM_S8_UINT:
		case VK_FORMAT_D32_SFLOAT_S8_UINT:
			return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		default:
			return VK_IMAGE_ASPECT_COLOR_BIT;
		}
	}

	VkImageAspectFlags ImageProperties::aspectMask() const
	{
		return aspectMaskForFormat(format);
	}

	VkImageSubresourceRange ImageProperties::subresourceRange() const
	{
		return VkImageSubresourceRange{
			.aspectMask = aspectMask(),
			.baseMipLevel = 0,
			.levelCount = mipLevels,
			.baseArrayLayer = 0,
			.layerCount = arrayLayers,
		};
	}

	namespace
	{
		ImageProperties propertiesFromCreateInfo(const VkImageCreateInfo& p_ImageInfo)
		{
			return ImageProperties{
				.flags = p_ImageInfo.flags,
				.format = p_ImageInfo.format,
				.extent = p_ImageInfo.extent,
				.mipLevels = p_ImageInfo.mipLevels,
				.arrayLayers = p_ImageInfo.arrayLayers,
				.samples = p_ImageInfo.samples,
				.tiling = p_ImageInfo.tiling,
				.usage = p_ImageInfo.usage,
				.sharingMode = p_ImageInfo.sharingMode,
			};
		}
	}

	Image createImage(const device::DeviceData& p_DeviceData, const VkImageCreateInfo& p_ImageInfo, const VmaAllocationCreateInfo& p_AllocInfo)
	{
		Image l_Ret{};
		l_Ret.properties = propertiesFromCreateInfo(p_ImageInfo);
		VULKAN_TRY(vmaCreateImage(p_DeviceData.allocator, &p_ImageInfo, &p_AllocInfo, &l_Ret.data.image, &l_Ret.data.alloc, &l_Ret.data.info));

		return l_Ret;
	}

	Image createImage(const device::DeviceData& p_DeviceData, const ImageProperties& p_Properties, const VmaAllocationCreateInfo& p_AllocInfo)
	{
		Image l_Ret = createImage(p_DeviceData, p_Properties.toVkImageCreateInfo(), p_AllocInfo);
		l_Ret.properties = p_Properties;
		return l_Ret;
	}

	Image createDepthBuffer(const device::DeviceData& p_DeviceData, const VkExtent2D p_Extent, const VmaAllocationCreateInfo& p_AllocInfo)
	{
		const ImageProperties l_Properties{
			.format = VK_FORMAT_D32_SFLOAT,
			.extent = VkExtent3D{ .width = p_Extent.width, .height = p_Extent.height, .depth = 1 },
			.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		};

		return createImage(p_DeviceData, l_Properties, p_AllocInfo);
	}

	void destroyImage(const device::DeviceData& p_DeviceData, Image& p_Image)
	{
		if (p_Image.data.image != VK_NULL_HANDLE)
		{
			vmaDestroyImage(p_DeviceData.allocator, p_Image.data.image, p_Image.data.alloc);
			p_Image.data.image = VK_NULL_HANDLE;
			p_Image.data.alloc = VK_NULL_HANDLE;
		}
	}

	namespace
	{
		VkImageView createImageViewWithRange(const device::DeviceData& p_DeviceData, const VkImage p_Image, const VkFormat p_Format, const VkImageSubresourceRange& p_Range, const VkImageViewType p_Type)
		{
			const VkImageViewCreateInfo l_ImageViewInfo{
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.image = p_Image,
				.viewType = p_Type,
				.format = p_Format,
				.components = {
					.r = VK_COMPONENT_SWIZZLE_IDENTITY,
					.g = VK_COMPONENT_SWIZZLE_IDENTITY,
					.b = VK_COMPONENT_SWIZZLE_IDENTITY,
					.a = VK_COMPONENT_SWIZZLE_IDENTITY
				},
				.subresourceRange = p_Range
			};

			VkImageView l_ImageView = VK_NULL_HANDLE;
			VULKAN_TRY(p_DeviceData.deviceTable.vkCreateImageView(p_DeviceData.device, &l_ImageViewInfo, nullptr, &l_ImageView));
			return l_ImageView;
		}
	}

	VkImageView createImageView(const vkp::device::DeviceData& p_DeviceData, const VkImage p_Image, const VkFormat p_Format, const VkImageAspectFlags p_AspectFlags, const VkImageViewType p_Type)
	{
		return createImageViewWithRange(p_DeviceData, p_Image, p_Format, VkImageSubresourceRange{ .aspectMask = p_AspectFlags, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 }, p_Type);
	}

	VkImageView createImageView(const vkp::device::DeviceData& p_DeviceData, const Image& p_Image, const VkImageViewType p_Type)
	{
		return createImageViewWithRange(p_DeviceData, p_Image.data.image, p_Image.properties.format, p_Image.properties.subresourceRange(), p_Type);
	}

	void destroyImageView(const device::DeviceData& p_DeviceData, VkImageView& p_ImageView)
	{
		if (p_ImageView != VK_NULL_HANDLE)
		{
			p_DeviceData.deviceTable.vkDestroyImageView(p_DeviceData.device, p_ImageView, nullptr);
			p_ImageView = VK_NULL_HANDLE;
		}
	}

	VkSampler createSampler(const device::DeviceData& p_DeviceData, const VkSamplerCreateInfo& p_Info)
	{
		VkSampler l_Sampler = VK_NULL_HANDLE;
		VULKAN_TRY(p_DeviceData.deviceTable.vkCreateSampler(p_DeviceData.device, &p_Info, nullptr, &l_Sampler));
		return l_Sampler;
	}

	VkSampler createSampler(const device::DeviceData& p_DeviceData, const VkFilter p_MagFilter, const VkFilter p_MinFilter, const VkSamplerMipmapMode p_MipmapMode, const VkSamplerAddressMode p_AddressMode, const float p_MaxAnisotropy, const float p_MaxLod)
	{
		const VkSamplerCreateInfo l_Info{
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.magFilter = p_MagFilter,
			.minFilter = p_MinFilter,
			.mipmapMode = p_MipmapMode,
			.addressModeU = p_AddressMode,
			.addressModeV = p_AddressMode,
			.addressModeW = p_AddressMode,
			.mipLodBias = 0.0f,
			.anisotropyEnable = p_MaxAnisotropy > 0.0f ? VK_TRUE : VK_FALSE,
			.maxAnisotropy = p_MaxAnisotropy,
			.compareEnable = VK_FALSE,
			.compareOp = VK_COMPARE_OP_NEVER,
			.minLod = 0.0f,
			.maxLod = p_MaxLod,
			.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
			.unnormalizedCoordinates = VK_FALSE,
		};
		return createSampler(p_DeviceData, l_Info);
	}

	void destroySampler(const device::DeviceData& p_DeviceData, VkSampler& p_Sampler)
	{
		if (p_Sampler != VK_NULL_HANDLE)
		{
			p_DeviceData.deviceTable.vkDestroySampler(p_DeviceData.device, p_Sampler, nullptr);
			p_Sampler = VK_NULL_HANDLE;
		}
	}

	namespace
	{
	}

	void uploadImage(device::DeviceData& p_DeviceData, const VkCommandPool p_Pool, const VkQueue p_Queue, const Image& p_Image, const void* p_Data, const VkDeviceSize p_Size, const VkImageLayout p_FinalLayout)
	{
		const uint32_t l_Width = p_Image.properties.extent.width;
		const uint32_t l_Height = p_Image.properties.extent.height;

		constexpr VmaAllocationCreateInfo l_StagingAllocInfo{
			.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
			.usage = VMA_MEMORY_USAGE_AUTO,
		};
		BufferData l_Staging = createBuffer(p_DeviceData, p_Size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, l_StagingAllocInfo);

		void* l_Mapped = nullptr;
		vmaMapMemory(p_DeviceData.allocator, l_Staging.alloc, &l_Mapped);
		std::memcpy(l_Mapped, p_Data, p_Size);
		vmaFlushAllocation(p_DeviceData.allocator, l_Staging.alloc, 0, VK_WHOLE_SIZE);
		vmaUnmapMemory(p_DeviceData.allocator, l_Staging.alloc);

		cmd::immediateSubmitScope(p_DeviceData, p_DeviceData.device, p_Pool, p_Queue, [&](const VkCommandBuffer p_Cb)
		{
			cmd::BasicBarrierBuilder<0, 0, 1>{}
				.image(p_Image, {
					.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,		  .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					.srcStage = VK_PIPELINE_STAGE_2_NONE,		  .srcAccess = VK_ACCESS_2_NONE,
					.dstStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT, .dstAccess = VK_ACCESS_2_TRANSFER_WRITE_BIT
				}).record(p_DeviceData, p_Cb);

			const VkBufferImageCopy l_Region{
				.bufferOffset = 0,
				.bufferRowLength = 0,
				.bufferImageHeight = 0,
				.imageSubresource = VkImageSubresourceLayers{ .aspectMask = p_Image.properties.aspectMask(), .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1 },
				.imageOffset = VkOffset3D{ 0, 0, 0 },
				.imageExtent = VkExtent3D{ l_Width, l_Height, 1 },
			};
			p_DeviceData.deviceTable.vkCmdCopyBufferToImage(p_Cb, l_Staging.buffer, p_Image.data.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &l_Region);

			cmd::BasicBarrierBuilder<0, 0, 1>{}
				.image(p_Image, {
					.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, .newLayout = p_FinalLayout,
					.srcStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,	   .srcAccess = VK_ACCESS_2_TRANSFER_WRITE_BIT,
					.dstStage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,  .dstAccess = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT
				}).record(p_DeviceData, p_Cb);
		});

		destroyBuffer(p_DeviceData, l_Staging);
	}
}
