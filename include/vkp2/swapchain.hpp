#pragma once
#include <span>
#include <vector>

#include "image.hpp"

namespace vkp
{
	namespace device
	{
		struct DeviceData;
	}

	struct SwapchainProperties
    {
        VkSurfaceFormatKHR format{};
        VkExtent2D extent{};
        uint32_t framesInFlight;
        VkPresentModeKHR presentMode;
		uint32_t minImageCount = 0;
    };

	struct Swapchain
	{
		Swapchain() = default;
        Swapchain(const device::DeviceData& p_DeviceData, VkSurfaceKHR p_Surface, uint32_t p_FramesInFlight, VkExtent2D p_Extent, VkPresentModeKHR p_PresentMode, std::span<const VkSurfaceFormatKHR> p_PreferredFormats = {});

		VkSwapchainKHR swapchain;
		SwapchainProperties properties{};

		[[nodiscard]] ImageProperties imageProperties() const;

		std::vector<VkImage> images;
        std::vector<VkImageView> imageViews;
        std::vector<VkSemaphore> renderFinishedSemaphores;

		void recreate(const device::DeviceData& p_DeviceData, VkSurfaceKHR p_Surface, VkExtent2D p_NewExtent);
		void destroy(const device::DeviceData& p_DeviceData);
	};

    SwapchainProperties querySwapchainProperties(VkPhysicalDevice p_PhysicalDevice, VkSurfaceKHR p_Surface, uint32_t p_DesiredFramesInFlight, std::span<const VkSurfaceFormatKHR> p_PreferredFormats = {});
}
