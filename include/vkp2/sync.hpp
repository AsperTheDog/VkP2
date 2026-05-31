#pragma once
#include <volk.h>

#include "base.hpp"

namespace vkp
{
	inline VkSemaphore createSemaphore(const VkDevice p_Device)
	{
		VkSemaphoreCreateInfo l_CreateInfo{};
		l_CreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkSemaphore l_Semaphore;
		VULKAN_TRY(vkCreateSemaphore(p_Device, &l_CreateInfo, nullptr, &l_Semaphore));
		return l_Semaphore;
	}

	inline VkSemaphore createTimelineSemaphore(const VkDevice p_Device, const uint64_t p_InitialValue = 0)
	{
		VkSemaphoreCreateInfo l_CreateInfo{};
		l_CreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		VkSemaphoreTypeCreateInfo l_TimelineInfo{};
		l_TimelineInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
		l_TimelineInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
		l_TimelineInfo.initialValue = p_InitialValue;
		l_CreateInfo.pNext = &l_TimelineInfo;

		VkSemaphore l_Semaphore;
		VULKAN_TRY(vkCreateSemaphore(p_Device, &l_CreateInfo, nullptr, &l_Semaphore));
		return l_Semaphore;
	}

	inline VkFence createFence(const VkDevice p_Device, const bool p_Signaled = false)
	{
		VkFenceCreateInfo l_CreateInfo{};
		l_CreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		l_CreateInfo.flags = p_Signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;

		VkFence l_Fence;
		VULKAN_TRY(vkCreateFence(p_Device, &l_CreateInfo, nullptr, &l_Fence));
		return l_Fence;
	}
}
