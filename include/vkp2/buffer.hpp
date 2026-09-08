#pragma once

#include <vk_mem_alloc.h>
#include <volk.h>

#include "device.hpp"

namespace vkp
{
	struct BufferData
	{
		VkBuffer buffer = VK_NULL_HANDLE;
		VmaAllocation alloc = VK_NULL_HANDLE;
#ifndef NDEBUG
		VmaAllocationInfo info{};
#endif
	};

	BufferData createBuffer(const device::DeviceData& p_DeviceData, const VkBufferCreateInfo& p_BufferInfo, const VmaAllocationCreateInfo& p_AllocInfo);
	BufferData createBuffer(const device::DeviceData& p_DeviceData, VkDeviceSize p_Size, VkBufferUsageFlags p_Usage, const VmaAllocationCreateInfo& p_AllocInfo);

	void destroyBuffer(const device::DeviceData& p_DeviceData, BufferData& p_Buffer);

	void uploadBuffer(device::DeviceData& p_DeviceData, VkCommandPool p_Pool, VkQueue p_Queue, const BufferData& p_Target, const void* p_Data, VkDeviceSize p_Size, VkDeviceSize p_Offset = 0);
}
