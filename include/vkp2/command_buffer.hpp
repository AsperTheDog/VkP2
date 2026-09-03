#pragma once
#include <span>
#include <volk.h>

#include "device.hpp"

#if defined(_MSC_VER)
#define VKP_FORCEINLINE __forceinline
#elif defined(__clang__) || defined(__GNUC__)
#define VKP_FORCEINLINE inline __attribute__((always_inline))
#else
#define VKP_FORCEINLINE inline
#endif

namespace vkp::cmd 
{
    struct CommandPool 
	{
        VkCommandPool handle = VK_NULL_HANDLE;

        void init(VkDevice p_Device, uint32_t p_QueueFamilyIndex, VkCommandPoolCreateFlags p_Flags = 0);
        void destroy(VkDevice p_Device);
        void reset(VkDevice p_Device, VkCommandPoolResetFlags p_Flags = 0) const;

        void allocate(VkDevice p_Device, std::span<VkCommandBuffer> p_OutBuffers, VkCommandBufferLevel p_Level = VK_COMMAND_BUFFER_LEVEL_PRIMARY) const;

        [[nodiscard]] VkCommandBuffer allocate(VkDevice p_Device, VkCommandBufferLevel p_Level = VK_COMMAND_BUFFER_LEVEL_PRIMARY) const;
    
        static void createPools(std::span<CommandPool> p_OutPools, VkDevice p_Device, uint32_t p_QueueFamilyIndex, VkCommandPoolCreateFlags p_Flags = 0);
    };

    template<typename F> requires std::invocable<F, VkCommandBuffer>
    VKP_FORCEINLINE void recordingScope(device::DeviceData& p_DeviceData, VkCommandBuffer p_Cb, const bool p_OneTime, F&& p_Lambda)
    {
        const VkCommandBufferBeginInfo l_BeginInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = p_OneTime ? VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT : 0u
        };

        p_DeviceData.deviceTable.vkBeginCommandBuffer(p_Cb, &l_BeginInfo);
		std::forward<F>(p_Lambda)(p_Cb);
        p_DeviceData.deviceTable.vkEndCommandBuffer(p_Cb);
    }

    template<bool AutoViewport = true, typename F> requires std::invocable<F, VkCommandBuffer>
    VKP_FORCEINLINE void renderScope(device::DeviceData& p_DeviceData, VkCommandBuffer p_Cb, const VkRenderingInfo& p_RenderInfo, F&& p_Lambda)
	{
        p_DeviceData.deviceTable.vkCmdBeginRendering(p_Cb, &p_RenderInfo);

        if constexpr (AutoViewport)
        {
            const VkViewport l_Viewport{
                .x = static_cast<float>(p_RenderInfo.renderArea.offset.x),
                .y = static_cast<float>(p_RenderInfo.renderArea.offset.y),
                .width = static_cast<float>(p_RenderInfo.renderArea.extent.width),
                .height = static_cast<float>(p_RenderInfo.renderArea.extent.height),
                .minDepth = 0.0f,
                .maxDepth = 1.0f
            };
            const VkRect2D l_Scissor = p_RenderInfo.renderArea;

            p_DeviceData.deviceTable.vkCmdSetViewport(p_Cb, 0, 1, &l_Viewport);
            p_DeviceData.deviceTable.vkCmdSetScissor(p_Cb, 0, 1, &l_Scissor);
        }

        std::forward<F>(p_Lambda)(p_Cb);

        p_DeviceData.deviceTable.vkCmdEndRendering(p_Cb);
    }

    template<typename F> requires std::invocable<F, VkCommandBuffer>
    VKP_FORCEINLINE void debugScope(VkCommandBuffer p_Cb, const char* p_Name, const float p_Color[4], F&& p_Lambda)
    {
        VkDebugUtilsLabelEXT l_Label{
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
            .pLabelName = p_Name
        };

        std::copy_n(p_Color, 4, l_Label.color);

        vkCmdBeginDebugUtilsLabelEXT(p_Cb, &l_Label);
        std::forward<F>(p_Lambda)(p_Cb);
        vkCmdEndDebugUtilsLabelEXT(p_Cb);
    }

    template<typename F> requires std::invocable<F, VkCommandBuffer>
    void immediateSubmitScope(device::DeviceData& p_DeviceData, const VkDevice p_Device, const VkCommandPool p_Pool, const VkQueue p_Queue, F&& p_Lambda, const VkFence p_UserFence = VK_NULL_HANDLE)
    {
        VkFence l_Fence = p_UserFence;
        bool l_OwnedFence = false;

        if (l_Fence == VK_NULL_HANDLE)
        {
	        constexpr VkFenceCreateInfo l_FenceCreateInfo{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
            p_DeviceData.deviceTable.vkCreateFence(p_Device, &l_FenceCreateInfo, nullptr, &l_Fence);
            l_OwnedFence = true;
        }
        else
        {
            p_DeviceData.deviceTable.vkResetFences(p_Device, 1, &l_Fence);
        }

        const VkCommandBufferAllocateInfo l_AllocInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = p_Pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };

        VkCommandBuffer l_Cb = VK_NULL_HANDLE;
        p_DeviceData.deviceTable.vkAllocateCommandBuffers(p_Device, &l_AllocInfo, &l_Cb);

        recordScope(l_Cb, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, std::forward<F>(p_Lambda));

        const VkSubmitInfo l_SubmitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &l_Cb
        };

        p_DeviceData.deviceTable.vkQueueSubmit(p_Queue, 1, &l_SubmitInfo, l_Fence);
        p_DeviceData.deviceTable.vkWaitForFences(p_Device, 1, &l_Fence, VK_TRUE, UINT64_MAX);
        p_DeviceData.deviceTable.vkFreeCommandBuffers(p_Device, p_Pool, 1, &l_Cb);

        if (l_OwnedFence)
        {
            p_DeviceData.deviceTable.vkDestroyFence(p_Device, l_Fence, nullptr);
        }
    }

    template<typename F> requires std::invocable<F, VkCommandBuffer>
    VKP_FORCEINLINE void timestampScope(device::DeviceData& p_DeviceData, VkCommandBuffer p_Cb, const VkQueryPool p_Pool, const uint32_t p_StartQueryIdx, const VkPipelineStageFlagBits p_Stage, F&& p_Lambda)
    {
        p_DeviceData.deviceTable.vkCmdWriteTimestamp(p_Cb, p_Stage, p_Pool, p_StartQueryIdx);
        std::forward<F>(p_Lambda)(p_Cb);
        p_DeviceData.deviceTable.vkCmdWriteTimestamp(p_Cb, p_Stage, p_Pool, p_StartQueryIdx + 1);
    }
}
