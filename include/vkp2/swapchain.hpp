#pragma once
#include <vector>

#include "extra/window.hpp"

namespace vkp
{
	namespace device
	{
		struct DeviceData;
	}

	struct SwapchainProperties
    {
        VkSurfaceFormatKHR format;
        VkExtent2D extent;
        uint32_t framesInFlight;
        VkPresentModeKHR presentMode;
    };

	struct Swapchain
	{
		Swapchain() = default;
        Swapchain(const device::DeviceData& p_DeviceData, VkSurfaceKHR p_Surface, uint32_t p_FramesInFlight, VkExtent2D p_Extent, VkPresentModeKHR p_PresentMode);

		VkSwapchainKHR swapchain;
		SwapchainProperties properties;

		std::vector<VkImage> images;
        std::vector<VkImageView> imageViews;
        std::vector<VkSemaphore> renderFinishedSemaphores;

        void recreate(const device::DeviceData& p_DeviceData, VkSurfaceKHR p_Surface, VkExtent2D p_NewExtent);
		void destroy(const device::DeviceData& p_DeviceData);
	};

    SwapchainProperties querySwapchainProperties(VkPhysicalDevice p_PhysicalDevice, VkSurfaceKHR p_Surface, uint32_t p_DesiredFramesInFlight);
}
