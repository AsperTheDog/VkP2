#include "swapchain.hpp"

#include <algorithm>

#include "base.hpp"
#include "device.hpp"
#include "sync.hpp"

namespace vkp
{
    Swapchain::Swapchain(const device::DeviceData& p_DeviceData, const VkSurfaceKHR p_Surface, const uint32_t p_FramesInFlight, const VkExtent2D p_Extent, const VkPresentModeKHR p_PresentMode)
		: swapchain(VK_NULL_HANDLE), properties{ querySwapchainProperties(p_DeviceData.physicalDevice, p_Surface, p_FramesInFlight) }
	{
		properties.extent = p_Extent;
        properties.presentMode = p_PresentMode;

		for (uint32_t i = 0; i < properties.framesInFlight + 1; i++)
		{
			renderFinishedSemaphores.push_back(createSemaphore(p_DeviceData.device));
		}
	}

	void Swapchain::recreate(const device::DeviceData& p_DeviceData, const VkSurfaceKHR p_Surface, const VkExtent2D p_NewExtent)
	{
        p_DeviceData.deviceTable.vkDeviceWaitIdle(p_DeviceData.device);

        for (const VkImageView& l_ImageView : imageViews)
        {
            if (l_ImageView != VK_NULL_HANDLE)
            {
                p_DeviceData.deviceTable.vkDestroyImageView(p_DeviceData.device, l_ImageView, nullptr);
            }
        }
        imageViews.clear();
        images.clear();

        VkSurfaceCapabilitiesKHR l_Capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(p_DeviceData.physicalDevice, p_Surface, &l_Capabilities);

        VkSwapchainCreateInfoKHR l_SwapchainCreateInfo{};
        l_SwapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        l_SwapchainCreateInfo.surface = p_Surface;
        l_SwapchainCreateInfo.minImageCount = properties.framesInFlight + 1;
        l_SwapchainCreateInfo.imageFormat = properties.format.format;
        l_SwapchainCreateInfo.imageColorSpace = properties.format.colorSpace;
        l_SwapchainCreateInfo.imageExtent = p_NewExtent;
        l_SwapchainCreateInfo.imageArrayLayers = 1;
        l_SwapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        l_SwapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        l_SwapchainCreateInfo.preTransform = l_Capabilities.currentTransform;
        l_SwapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        l_SwapchainCreateInfo.presentMode = properties.presentMode;
        l_SwapchainCreateInfo.clipped = VK_TRUE;
        l_SwapchainCreateInfo.oldSwapchain = swapchain;

        const VkSwapchainKHR l_OldSwapchain = swapchain;
        VULKAN_TRY(p_DeviceData.deviceTable.vkCreateSwapchainKHR(p_DeviceData.device, &l_SwapchainCreateInfo, nullptr, &swapchain));
        
        if (l_OldSwapchain != VK_NULL_HANDLE)
        {
            p_DeviceData.deviceTable.vkDestroySwapchainKHR(p_DeviceData.device, l_OldSwapchain, nullptr);
        }

        uint32_t l_ImageCount;
        VULKAN_TRY(p_DeviceData.deviceTable.vkGetSwapchainImagesKHR(p_DeviceData.device, swapchain, &l_ImageCount, nullptr));

        properties.extent = p_NewExtent;

        images.resize(l_ImageCount);
        imageViews.resize(l_ImageCount);
        VULKAN_TRY(p_DeviceData.deviceTable.vkGetSwapchainImagesKHR(p_DeviceData.device, swapchain, &l_ImageCount, images.data()));
        for (uint32_t l_Index = 0; l_Index < l_ImageCount; l_Index++)
        {
            VkImageViewCreateInfo l_ImageViewCreateInfo{ .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
            l_ImageViewCreateInfo.image = images[l_Index];
            l_ImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            l_ImageViewCreateInfo.format = properties.format.format;
            l_ImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            l_ImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            l_ImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            l_ImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            l_ImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            l_ImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
            l_ImageViewCreateInfo.subresourceRange.levelCount = 1;
            l_ImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
            l_ImageViewCreateInfo.subresourceRange.layerCount = 1;
            VULKAN_TRY(p_DeviceData.deviceTable.vkCreateImageView(p_DeviceData.device, &l_ImageViewCreateInfo, nullptr, &imageViews[l_Index]));
        }
	}

	void Swapchain::destroy(const device::DeviceData& p_DeviceData)
	{
        for (const VkImageView& l_ImageView : imageViews)
        {
            if (l_ImageView != VK_NULL_HANDLE)
            {
                p_DeviceData.deviceTable.vkDestroyImageView(p_DeviceData.device, l_ImageView, nullptr);
            }
        }
        imageViews.clear();
        images.clear();

		for (const VkSemaphore& l_Semaphore : renderFinishedSemaphores)
		{
			if (l_Semaphore != VK_NULL_HANDLE)
			{
				p_DeviceData.deviceTable.vkDestroySemaphore(p_DeviceData.device, l_Semaphore, nullptr);
			}
		}
        renderFinishedSemaphores.clear();

        if (swapchain != VK_NULL_HANDLE)
        {
            p_DeviceData.deviceTable.vkDestroySwapchainKHR(p_DeviceData.device, swapchain, nullptr);
			swapchain = VK_NULL_HANDLE;
        }
	}

	SwapchainProperties querySwapchainProperties(const VkPhysicalDevice p_PhysicalDevice, const VkSurfaceKHR p_Surface, const uint32_t p_DesiredFramesInFlight)
    {
        VkSurfaceFormatKHR l_Format{};

        SwapchainProperties l_Properties{};

        uint32_t l_FormatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(p_PhysicalDevice, p_Surface, &l_FormatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> l_Formats(l_FormatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(p_PhysicalDevice, p_Surface, &l_FormatCount, l_Formats.data());

        for (const VkSurfaceFormatKHR& l_Candidate : l_Formats)
        {
            if (l_Candidate.format == VK_FORMAT_B8G8R8A8_SRGB && l_Candidate.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                l_Format = l_Candidate;
                break;
            }
        }
        if (l_Format.format == VK_FORMAT_UNDEFINED)
        {
            l_Format = l_Formats[0];
        }

        l_Properties.format = l_Format;

        VkSurfaceCapabilitiesKHR l_Capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(p_PhysicalDevice, p_Surface, &l_Capabilities);
        uint32_t l_ImageCount = p_DesiredFramesInFlight + 1;
        l_ImageCount = std::max(l_ImageCount, l_Capabilities.minImageCount);
        if (l_Capabilities.maxImageCount > 0)
        {
	        l_ImageCount = std::min(l_ImageCount, l_Capabilities.maxImageCount);
        }
        l_Properties.framesInFlight = l_ImageCount - 1;

        return l_Properties;
    }
}
