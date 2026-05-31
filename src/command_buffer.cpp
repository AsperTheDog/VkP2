#include "command_buffer.hpp"

#include <volk.h>
namespace vkp {
	void cmd::CommandPool::init(const VkDevice p_Device, const uint32_t p_QueueFamilyIndex, const VkCommandPoolCreateFlags p_Flags)
	{
		VkCommandPoolCreateInfo l_PoolInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = p_Flags,
			.queueFamilyIndex = p_QueueFamilyIndex
		};
		vkCreateCommandPool(p_Device, &l_PoolInfo, nullptr, &handle);
	}

	void cmd::CommandPool::destroy(const VkDevice p_Device)
	{
		if (handle) 
		{
			vkDestroyCommandPool(p_Device, handle, nullptr);
			handle = VK_NULL_HANDLE;
		}
	}

	void cmd::CommandPool::reset(const VkDevice p_Device, const VkCommandPoolResetFlags p_Flags) const
	{
		vkResetCommandPool(p_Device, handle, p_Flags);
	}

	void cmd::CommandPool::allocate(const VkDevice p_Device, std::span<VkCommandBuffer> p_OutBuffers,
		const VkCommandBufferLevel p_Level) const
	{
		VkCommandBufferAllocateInfo l_AllocInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = handle,
			.level = p_Level,
			.commandBufferCount = static_cast<uint32_t>(p_OutBuffers.size())
		};
		vkAllocateCommandBuffers(p_Device, &l_AllocInfo, p_OutBuffers.data());
	}

	VkCommandBuffer cmd::CommandPool::allocate(const VkDevice p_Device, const VkCommandBufferLevel p_Level) const
	{
		VkCommandBuffer l_Buffer = VK_NULL_HANDLE;
		allocate(p_Device, { &l_Buffer, 1 }, p_Level);
		return l_Buffer;
	}

	void cmd::CommandPool::createPools(std::span<CommandPool> p_OutPools, const VkDevice p_Device, const uint32_t p_QueueFamilyIndex, const VkCommandPoolCreateFlags p_Flags)
	{
		for (CommandPool& l_Pool : p_OutPools)
		{
			l_Pool.init(p_Device, p_QueueFamilyIndex, p_Flags);
		}
	}
}
