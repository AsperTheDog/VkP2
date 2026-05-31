#pragma once
#include <span>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace vkp::cmd 
{
    struct CommandPool {
        VkCommandPool handle = VK_NULL_HANDLE;

        void init(VkDevice p_Device, uint32_t p_QueueFamilyIndex, VkCommandPoolCreateFlags p_Flags = 0);
        void destroy(VkDevice p_Device);
        void reset(VkDevice p_Device, VkCommandPoolResetFlags p_Flags = 0) const;

        void allocate(VkDevice p_Device, std::span<VkCommandBuffer> p_OutBuffers, VkCommandBufferLevel p_Level = VK_COMMAND_BUFFER_LEVEL_PRIMARY) const;

        [[nodiscard]] VkCommandBuffer allocate(VkDevice p_Device, VkCommandBufferLevel p_Level = VK_COMMAND_BUFFER_LEVEL_PRIMARY) const;
    
        static void createPools(std::span<CommandPool> p_OutPools, VkDevice p_Device, uint32_t p_QueueFamilyIndex, VkCommandPoolCreateFlags p_Flags = 0);
    };
}
