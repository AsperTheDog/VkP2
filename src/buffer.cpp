#include "buffer.hpp"

#include <cstring>

#include "base.hpp"
#include "command_buffer.hpp"

namespace vkp
{
	BufferData createBuffer(const device::DeviceData& p_DeviceData, const VkBufferCreateInfo& p_BufferInfo, const VmaAllocationCreateInfo& p_AllocInfo)
	{
		BufferData l_Buffer{};
		VULKAN_TRY(vmaCreateBuffer(p_DeviceData.allocator, &p_BufferInfo, &p_AllocInfo, &l_Buffer.buffer, &l_Buffer.alloc, &l_Buffer.info));
		return l_Buffer;
	}

	BufferData createBuffer(const device::DeviceData& p_DeviceData, const VkDeviceSize p_Size, const VkBufferUsageFlags p_Usage, const VmaAllocationCreateInfo& p_AllocInfo)
	{
		const VkBufferCreateInfo l_BufferInfo{
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.size = p_Size,
			.usage = p_Usage,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 0,
			.pQueueFamilyIndices = nullptr,
		};
		return createBuffer(p_DeviceData, l_BufferInfo, p_AllocInfo);
	}

	void destroyBuffer(const device::DeviceData& p_DeviceData, BufferData& p_Buffer)
	{
		if (p_Buffer.buffer != VK_NULL_HANDLE)
		{
			vmaDestroyBuffer(p_DeviceData.allocator, p_Buffer.buffer, p_Buffer.alloc);
			p_Buffer.buffer = VK_NULL_HANDLE;
			p_Buffer.alloc = VK_NULL_HANDLE;
		}
	}

	void uploadBuffer(device::DeviceData& p_DeviceData, const VkCommandPool p_Pool, const VkQueue p_Queue, const BufferData& p_Target, const void* p_Data, const VkDeviceSize p_Size, const VkDeviceSize p_Offset)
	{
		constexpr VmaAllocationCreateInfo l_StagingAllocInfo{
			.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
			.usage = VMA_MEMORY_USAGE_AUTO,
		};
		const BufferData l_Staging = createBuffer(p_DeviceData, p_Size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, l_StagingAllocInfo);

		void* l_Mapped = nullptr;
		vmaMapMemory(p_DeviceData.allocator, l_Staging.alloc, &l_Mapped);
		std::memcpy(l_Mapped, p_Data, p_Size);
		vmaFlushAllocation(p_DeviceData.allocator, l_Staging.alloc, 0, VK_WHOLE_SIZE);
		vmaUnmapMemory(p_DeviceData.allocator, l_Staging.alloc);

		const VkBufferCopy l_Copy{
			.srcOffset = 0,
			.dstOffset = p_Offset,
			.size = p_Size,
		};
		cmd::immediateSubmitScope(p_DeviceData, p_DeviceData.device, p_Pool, p_Queue, [&](const VkCommandBuffer p_Cb)
		{
			p_DeviceData.deviceTable.vkCmdCopyBuffer(p_Cb, l_Staging.buffer, p_Target.buffer, 1, &l_Copy);
		});

		vmaDestroyBuffer(p_DeviceData.allocator, l_Staging.buffer, l_Staging.alloc);
	}
}
