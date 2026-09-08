#include "image.hpp"

#include <cstring>

#include "base.hpp"
#include "buffer.hpp"
#include "command_buffer.hpp"
#include "device.hpp"

namespace vkp
{

	ImageData createImage(const device::DeviceData& p_DeviceData, const VkImageCreateInfo& p_ImageInfo, const VmaAllocationCreateInfo& p_AllocInfo)
	{
		ImageData l_Ret{};
#ifndef NDEBUG
		VULKAN_TRY(vmaCreateImage(p_DeviceData.allocator, &p_ImageInfo, &p_AllocInfo, &l_Ret.image, &l_Ret.alloc, &l_Ret.info));
#else
		VULKAN_TRY(vmaCreateImage(p_DeviceData.allocator, &p_ImageInfo, &p_AllocInfo, &l_Ret.image, &l_Ret.alloc, VK_NULL_HANDLE));
#endif

		return l_Ret;
	}

	ImageData createImage(const device::DeviceData& p_DeviceData, const SimpleImgInfo& p_ImageInfo, const VmaAllocationCreateInfo& p_AllocInfo)
	{
		return createImage(p_DeviceData, p_ImageInfo.toVkImageCreateInfo(), p_AllocInfo);
	}

	ImageData createDepthBuffer(const device::DeviceData& p_DeviceData, const Swapchain& p_Swapchain, const VmaAllocationCreateInfo& p_AllocInfo)
	{
		const VkExtent3D l_Extent = {
			.width = p_Swapchain.properties.extent.width,
			.height = p_Swapchain.properties.extent.height,
			.depth = 1
		};

		const VkImageCreateInfo l_ImageInfo{
			.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.imageType = VK_IMAGE_TYPE_2D,
			.format = VK_FORMAT_D32_SFLOAT,
			.extent = l_Extent,
			.mipLevels = 1,
			.arrayLayers = 1,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.tiling = VK_IMAGE_TILING_OPTIMAL,
			.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 0,
			.pQueueFamilyIndices = nullptr,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
		};

		return createImage(p_DeviceData, l_ImageInfo, p_AllocInfo);
	}

	VkImageView createImageView(const vkp::device::DeviceData& p_DeviceData, const VkImage p_Image, const VkFormat p_Format, const VkImageAspectFlags p_AspectFlags, const VkImageViewType p_Type)
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
			.subresourceRange = {
				.aspectMask = p_AspectFlags,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			}
		};

		VkImageView l_ImageView;
		VULKAN_TRY(p_DeviceData.deviceTable.vkCreateImageView(p_DeviceData.device, &l_ImageViewInfo, nullptr, &l_ImageView));
		return l_ImageView;
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
		uint32_t imagePixelSize(const VkFormat p_Format)
		{
			switch (p_Format)
			{
			case VK_FORMAT_R8_UNORM:
			case VK_FORMAT_R8_SRGB:
			case VK_FORMAT_R8_UINT:
			case VK_FORMAT_R8_SINT: return 1;
			case VK_FORMAT_R8G8_UNORM:
			case VK_FORMAT_R8G8_SRGB:
			case VK_FORMAT_R16_SFLOAT:
			case VK_FORMAT_R16_UNORM:
			case VK_FORMAT_R16_UINT:
			case VK_FORMAT_R16_SINT: return 2;
			case VK_FORMAT_R8G8B8A8_UNORM:
			case VK_FORMAT_R8G8B8A8_SRGB:
			case VK_FORMAT_R8G8B8A8_UINT:
			case VK_FORMAT_R8G8B8A8_SINT:
			case VK_FORMAT_B8G8R8A8_UNORM:
			case VK_FORMAT_B8G8R8A8_SRGB:
			case VK_FORMAT_R32_SFLOAT:
			case VK_FORMAT_R32_UINT:
			case VK_FORMAT_R32_SINT:
			case VK_FORMAT_R16G16_SFLOAT:
			case VK_FORMAT_R16G16_UNORM:
			case VK_FORMAT_R16G16_UINT:
			case VK_FORMAT_R16G16_SINT: return 4;
			case VK_FORMAT_R16G16B16A16_SFLOAT:
			case VK_FORMAT_R16G16B16A16_UNORM:
			case VK_FORMAT_R16G16B16A16_UINT:
			case VK_FORMAT_R16G16B16A16_SINT:
			case VK_FORMAT_R32G32_SFLOAT:
			case VK_FORMAT_R32G32_UINT:
			case VK_FORMAT_R32G32_SINT: return 8;
			case VK_FORMAT_R32G32B32A32_SFLOAT:
			case VK_FORMAT_R32G32B32A32_UINT:
			case VK_FORMAT_R32G32B32A32_SINT: return 16;
			default: throw std::runtime_error("vkp::image: unsupported upload format");
			}
		}
	}

	void uploadImage(device::DeviceData& p_DeviceData, const VkCommandPool p_Pool, const VkQueue p_Queue, const ImageData& p_Image, const VkFormat p_Format, const uint32_t p_Width, const uint32_t p_Height, const void* p_Data, const VkImageLayout p_FinalLayout)
	{
		const uint32_t l_PixelSize = imagePixelSize(p_Format);
		const VkDeviceSize l_Size = static_cast<VkDeviceSize>(p_Width) * p_Height * l_PixelSize;

		constexpr VmaAllocationCreateInfo l_StagingAllocInfo{
			.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
			.usage = VMA_MEMORY_USAGE_AUTO,
		};
		BufferData l_Staging = createBuffer(p_DeviceData, l_Size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, l_StagingAllocInfo);

		void* l_Mapped = nullptr;
		vmaMapMemory(p_DeviceData.allocator, l_Staging.alloc, &l_Mapped);
		std::memcpy(l_Mapped, p_Data, static_cast<size_t>(l_Size));
		vmaFlushAllocation(p_DeviceData.allocator, l_Staging.alloc, 0, VK_WHOLE_SIZE);
		vmaUnmapMemory(p_DeviceData.allocator, l_Staging.alloc);

		cmd::immediateSubmitScope(p_DeviceData, p_DeviceData.device, p_Pool, p_Queue, [&](const VkCommandBuffer p_Cb)
		{
			cmd::transitionImage(p_DeviceData, p_Cb, p_Image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

			const VkBufferImageCopy l_Region{
				.bufferOffset = 0,
				.bufferRowLength = 0,
				.bufferImageHeight = 0,
				.imageSubresource = VkImageSubresourceLayers{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1 },
				.imageOffset = VkOffset3D{ 0, 0, 0 },
				.imageExtent = VkExtent3D{ p_Width, p_Height, 1 },
			};
			p_DeviceData.deviceTable.vkCmdCopyBufferToImage(p_Cb, l_Staging.buffer, p_Image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &l_Region);

			cmd::transitionImage(p_DeviceData, p_Cb, p_Image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, p_FinalLayout, VK_IMAGE_ASPECT_COLOR_BIT);
		});

		destroyBuffer(p_DeviceData, l_Staging);
	}
}
