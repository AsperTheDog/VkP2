#pragma once

#include <array>
#include <optional>

#include "extra/small_vector.hpp"

namespace vkp::cmd
{
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

        recordingScope(p_DeviceData, l_Cb, true, std::forward<F>(p_Lambda));

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
    VKP_FORCEINLINE void pushConstants(const device::DeviceData& p_DeviceData, const VkCommandBuffer p_Cb, const VkPipelineLayout p_Layout, const VkShaderStageFlags p_StageFlags, const uint32_t p_Offset, const uint32_t p_Size, const void* p_Values)
    {
        p_DeviceData.deviceTable.vkCmdPushConstants(p_Cb, p_Layout, p_StageFlags, p_Offset, p_Size, p_Values);
    }
	namespace detail
	{
        inline StageAccess2 srcStageAccess2(const VkImageLayout p_Layout)
        {
            switch (p_Layout)
            {
            case VK_IMAGE_LAYOUT_UNDEFINED:
            case VK_IMAGE_LAYOUT_PREINITIALIZED:
                return { .stage = VK_PIPELINE_STAGE_2_NONE, .access = VK_ACCESS_2_NONE };
            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                return { .stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT, .access = VK_ACCESS_2_TRANSFER_READ_BIT };
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                return { .stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT, .access = VK_ACCESS_2_TRANSFER_WRITE_BIT };
            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                return { .stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, .access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT };
            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                return { .stage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, .access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT };
            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                return { .stage = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, .access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT };
            default:
                return { .stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, .access = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT };
            }
        }
        inline StageAccess2 dstStageAccess2(const VkImageLayout p_Layout)
        {
            switch (p_Layout)
            {
            case VK_IMAGE_LAYOUT_UNDEFINED:
            case VK_IMAGE_LAYOUT_PREINITIALIZED:
                return { .stage = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, .access = VK_ACCESS_2_NONE };
            case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
                return { .stage = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, .access = VK_ACCESS_2_NONE };
            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                return { .stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT, .access = VK_ACCESS_2_TRANSFER_READ_BIT };
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                return { .stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT, .access = VK_ACCESS_2_TRANSFER_WRITE_BIT };
            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                return { .stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, .access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT };
            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                return { .stage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, .access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT };
            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                return { .stage = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, .access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT };
            default:
                return { .stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, .access = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT };
            }
        }
        inline StageAccess srcStageAccess(const VkImageLayout p_Layout)
        {
            switch (p_Layout)
            {
            case VK_IMAGE_LAYOUT_UNDEFINED:
            case VK_IMAGE_LAYOUT_PREINITIALIZED:
                return { .stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, .access = 0 };
            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                return { .stage = VK_PIPELINE_STAGE_TRANSFER_BIT, .access = VK_ACCESS_TRANSFER_READ_BIT };
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                return { .stage = VK_PIPELINE_STAGE_TRANSFER_BIT, .access = VK_ACCESS_TRANSFER_WRITE_BIT };
            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                return { .stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, .access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT };
            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                return { .stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, .access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT };
            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                return { .stage = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, .access = VK_ACCESS_SHADER_READ_BIT };
            default:
                return { .stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, .access = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT };
            }
        }
        inline StageAccess dstStageAccess(const VkImageLayout p_Layout)
        {
            switch (p_Layout)
            {
            case VK_IMAGE_LAYOUT_UNDEFINED:
            case VK_IMAGE_LAYOUT_PREINITIALIZED:
                return { .stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, .access = 0 };
            case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
                return { .stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, .access = 0 };
            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                return { .stage = VK_PIPELINE_STAGE_TRANSFER_BIT, .access = VK_ACCESS_TRANSFER_READ_BIT };
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                return { .stage = VK_PIPELINE_STAGE_TRANSFER_BIT, .access = VK_ACCESS_TRANSFER_WRITE_BIT };
            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                return { .stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, .access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT };
            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                return { .stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, .access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT };
            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                return { .stage = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, .access = VK_ACCESS_SHADER_READ_BIT };
            default:
                return { .stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, .access = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT };
            }
        }
	}
    VKP_FORCEINLINE void transitionImage(const device::DeviceData& p_DeviceData, const VkCommandBuffer p_Cb, const VkImage p_Image, const VkImageLayout p_InitialLayout, const VkImageLayout p_FinalLayout, const VkImageAspectFlags p_Aspect = VK_IMAGE_ASPECT_COLOR_BIT, const SyncMode p_Mode = SyncMode::Sync2)
    {
        const VkImageSubresourceRange l_Range{
            .aspectMask = p_Aspect,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        };

        if (p_Mode == SyncMode::Sync2)
        {
            const detail::StageAccess2 l_Src = detail::srcStageAccess2(p_InitialLayout);
            const detail::StageAccess2 l_Dst = detail::dstStageAccess2(p_FinalLayout);
            const VkImageMemoryBarrier2 l_Barrier{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .pNext = nullptr,
                .srcStageMask = l_Src.stage,
                .srcAccessMask = l_Src.access,
                .dstStageMask = l_Dst.stage,
                .dstAccessMask = l_Dst.access,
                .oldLayout = p_InitialLayout,
                .newLayout = p_FinalLayout,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = p_Image,
                .subresourceRange = l_Range,
            };
            const VkDependencyInfo l_Dependency{
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .pNext = nullptr,
                .dependencyFlags = 0,
                .memoryBarrierCount = 0,
                .pMemoryBarriers = nullptr,
                .bufferMemoryBarrierCount = 0,
                .pBufferMemoryBarriers = nullptr,
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = &l_Barrier,
            };
            p_DeviceData.deviceTable.vkCmdPipelineBarrier2(p_Cb, &l_Dependency);
        }
        else
        {
            const detail::StageAccess l_Src = detail::srcStageAccess(p_InitialLayout);
            const detail::StageAccess l_Dst = detail::dstStageAccess(p_FinalLayout);
            const VkImageMemoryBarrier l_Barrier{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = nullptr,
                .srcAccessMask = l_Src.access,
                .dstAccessMask = l_Dst.access,
                .oldLayout = p_InitialLayout,
                .newLayout = p_FinalLayout,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = p_Image,
                .subresourceRange = l_Range,
            };
            p_DeviceData.deviceTable.vkCmdPipelineBarrier(p_Cb, l_Src.stage, l_Dst.stage, 0, 0, nullptr, 0, nullptr, 1, &l_Barrier);
        }
    }
    VKP_FORCEINLINE void submit2(const device::DeviceData& p_DeviceData, const VkQueue p_Queue, const Submit2Info& p_Info, const VkFence p_Fence)
    {
        VkSemaphoreSubmitInfo l_Wait{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext = nullptr,
            .semaphore = p_Info.waitSemaphore,
            .value = 0,
            .stageMask = p_Info.waitStage,
            .deviceIndex = 0,
        };
        VkSemaphoreSubmitInfo l_Signal{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext = nullptr,
            .semaphore = p_Info.signalSemaphore,
            .value = 0,
            .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .deviceIndex = 0,
        };
        const VkCommandBufferSubmitInfo l_CommandBuffer{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .pNext = nullptr,
            .commandBuffer = p_Info.commandBuffer,
            .deviceMask = 0,
        };
        const VkSubmitInfo2 l_Submit{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .pNext = nullptr,
            .flags = 0,
            .waitSemaphoreInfoCount = 1,
            .pWaitSemaphoreInfos = &l_Wait,
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &l_CommandBuffer,
            .signalSemaphoreInfoCount = 1,
            .pSignalSemaphoreInfos = &l_Signal,
        };
        VULKAN_TRY(p_DeviceData.deviceTable.vkQueueSubmit2(p_Queue, 1, &l_Submit, p_Fence));
    }
    template<SyncMode Mode = SyncMode::Sync2, typename F> requires std::invocable<F, VkCommandBuffer>
    void frameRenderScope(device::DeviceData& p_DeviceData, const VkCommandBuffer p_Cb, const FrameSpec& p_Spec, F&& p_Lambda)
    {
        constexpr VkImageLayout kColorLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        constexpr VkImageLayout kDepthLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        for (const AttachmentSpec& l_Color : p_Spec.colors)
        {
            transitionImage(p_DeviceData, p_Cb, l_Color.image, l_Color.initialLayout, kColorLayout, VK_IMAGE_ASPECT_COLOR_BIT, Mode);
        }
        if (p_Spec.depth)
        {
            transitionImage(p_DeviceData, p_Cb, p_Spec.depth->image, p_Spec.depth->initialLayout, kDepthLayout, VK_IMAGE_ASPECT_DEPTH_BIT, Mode);
        }

        std::array<VkRenderingAttachmentInfo, 8> l_ColorInfos{};
        for (uint32_t i = 0; i < p_Spec.colors.size(); ++i)
        {
			const AttachmentSpec& l_Color = p_Spec.colors[i];
            l_ColorInfos[i] = {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .pNext = nullptr,
                .imageView = l_Color.view,
                .imageLayout = kColorLayout,
                .resolveMode = VK_RESOLVE_MODE_NONE,
                .resolveImageView = VK_NULL_HANDLE,
                .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .loadOp = l_Color.loadOp,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .clearValue = l_Color.clearValue,
            };
        }

        std::optional<VkRenderingAttachmentInfo> l_DepthInfo;
        if (p_Spec.depth)
        {
            l_DepthInfo = VkRenderingAttachmentInfo{
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .pNext = nullptr,
                .imageView = p_Spec.depth->view,
                .imageLayout = kDepthLayout,
                .resolveMode = VK_RESOLVE_MODE_NONE,
                .resolveImageView = VK_NULL_HANDLE,
                .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .loadOp = p_Spec.depth->loadOp,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .clearValue = p_Spec.depth->clearValue,
            };
        }

        const VkRenderingInfo l_RenderingInfo{
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderArea = VkRect2D{ .offset = { .x = 0, .y = 0 }, .extent = p_Spec.extent },
            .layerCount = 1,
            .viewMask = 0,
            .colorAttachmentCount = static_cast<uint32_t>(p_Spec.colors.size()),
            .pColorAttachments = l_ColorInfos.data(),
            .pDepthAttachment = p_Spec.depth ? &*l_DepthInfo : nullptr,
            .pStencilAttachment = nullptr,
        };

        renderScope(p_DeviceData, p_Cb, l_RenderingInfo, std::forward<F>(p_Lambda));

        for (const AttachmentSpec& l_Color : p_Spec.colors)
        {
            transitionImage(p_DeviceData, p_Cb, l_Color.image, kColorLayout, l_Color.finalLayout, VK_IMAGE_ASPECT_COLOR_BIT, Mode);
        }
        if (p_Spec.depth)
        {
            transitionImage(p_DeviceData, p_Cb, p_Spec.depth->image, kDepthLayout, p_Spec.depth->finalLayout, VK_IMAGE_ASPECT_DEPTH_BIT, Mode);
        }
    }

    VKP_FORCEINLINE void bufferBarrier2(const device::DeviceData& p_DeviceData, const VkCommandBuffer p_Cb, const VkBuffer p_Buffer, const VkDeviceSize p_Offset, const VkDeviceSize p_Size, const VkPipelineStageFlags2 p_SrcStage, const VkAccessFlags2 p_SrcAccess, const VkPipelineStageFlags2 p_DstStage, const VkAccessFlags2 p_DstAccess)
    {
        const VkBufferMemoryBarrier2 l_Barrier{
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcStageMask = p_SrcStage,
            .srcAccessMask = p_SrcAccess,
            .dstStageMask = p_DstStage,
            .dstAccessMask = p_DstAccess,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = p_Buffer,
            .offset = p_Offset,
            .size = p_Size,
        };
        const VkDependencyInfo l_Dependency{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext = nullptr,
            .dependencyFlags = 0,
            .memoryBarrierCount = 0,
            .pMemoryBarriers = nullptr,
            .bufferMemoryBarrierCount = 1,
            .pBufferMemoryBarriers = &l_Barrier,
            .imageMemoryBarrierCount = 0,
            .pImageMemoryBarriers = nullptr,
        };
        p_DeviceData.deviceTable.vkCmdPipelineBarrier2(p_Cb, &l_Dependency);
    }

    VKP_FORCEINLINE void bufferBarrier(const device::DeviceData& p_DeviceData, const VkCommandBuffer p_Cb, const VkBuffer p_Buffer, const VkDeviceSize p_Offset, const VkDeviceSize p_Size, const VkPipelineStageFlags p_SrcStage, const VkAccessFlags p_SrcAccess, const VkPipelineStageFlags p_DstStage, const VkAccessFlags p_DstAccess)
    {
        const VkBufferMemoryBarrier l_Barrier{
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = p_SrcAccess,
            .dstAccessMask = p_DstAccess,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = p_Buffer,
            .offset = p_Offset,
            .size = p_Size,
        };
        p_DeviceData.deviceTable.vkCmdPipelineBarrier(p_Cb, p_SrcStage, p_DstStage, 0, 0, nullptr, 1, &l_Barrier, 0, nullptr);
    }

    VKP_FORCEINLINE void memoryBarrier2(const device::DeviceData& p_DeviceData, const VkCommandBuffer p_Cb, const VkPipelineStageFlags2 p_SrcStage, const VkAccessFlags2 p_SrcAccess, const VkPipelineStageFlags2 p_DstStage, const VkAccessFlags2 p_DstAccess)
    {
        const VkMemoryBarrier2 l_Barrier{
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcStageMask = p_SrcStage,
            .srcAccessMask = p_SrcAccess,
            .dstStageMask = p_DstStage,
            .dstAccessMask = p_DstAccess,
        };
        const VkDependencyInfo l_Dependency{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext = nullptr,
            .dependencyFlags = 0,
            .memoryBarrierCount = 1,
            .pMemoryBarriers = &l_Barrier,
            .bufferMemoryBarrierCount = 0,
            .pBufferMemoryBarriers = nullptr,
            .imageMemoryBarrierCount = 0,
            .pImageMemoryBarriers = nullptr,
        };
        p_DeviceData.deviceTable.vkCmdPipelineBarrier2(p_Cb, &l_Dependency);
    }

    VKP_FORCEINLINE void memoryBarrier(const device::DeviceData& p_DeviceData, const VkCommandBuffer p_Cb, const VkPipelineStageFlags p_SrcStage, const VkAccessFlags p_SrcAccess, const VkPipelineStageFlags p_DstStage, const VkAccessFlags p_DstAccess)
    {
        const VkMemoryBarrier l_Barrier{
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = p_SrcAccess,
            .dstAccessMask = p_DstAccess,
        };
        p_DeviceData.deviceTable.vkCmdPipelineBarrier(p_Cb, p_SrcStage, p_DstStage, 0, 1, &l_Barrier, 0, nullptr, 0, nullptr);
    }

    VKP_FORCEINLINE void bufferOwnershipTransfer2(const device::DeviceData& p_DeviceData, const VkCommandBuffer p_Cb, const VkBuffer p_Buffer, const VkDeviceSize p_Offset, const VkDeviceSize p_Size, const uint32_t p_SrcQueueFamily, const uint32_t p_DstQueueFamily, const VkPipelineStageFlags2 p_SrcStage, const VkPipelineStageFlags2 p_DstStage)
    {
        const VkBufferMemoryBarrier2 l_Barrier{
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcStageMask = p_SrcStage,
            .srcAccessMask = VK_ACCESS_2_NONE,
            .dstStageMask = p_DstStage,
            .dstAccessMask = VK_ACCESS_2_NONE,
            .srcQueueFamilyIndex = p_SrcQueueFamily,
            .dstQueueFamilyIndex = p_DstQueueFamily,
            .buffer = p_Buffer,
            .offset = p_Offset,
            .size = p_Size,
        };
        const VkDependencyInfo l_Dependency{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext = nullptr,
            .dependencyFlags = 0,
            .memoryBarrierCount = 0,
            .pMemoryBarriers = nullptr,
            .bufferMemoryBarrierCount = 1,
            .pBufferMemoryBarriers = &l_Barrier,
            .imageMemoryBarrierCount = 0,
            .pImageMemoryBarriers = nullptr,
        };
        p_DeviceData.deviceTable.vkCmdPipelineBarrier2(p_Cb, &l_Dependency);
    }

    VKP_FORCEINLINE void bufferOwnershipTransfer(const device::DeviceData& p_DeviceData, const VkCommandBuffer p_Cb, const VkBuffer p_Buffer, const VkDeviceSize p_Offset, const VkDeviceSize p_Size, const uint32_t p_SrcQueueFamily, const uint32_t p_DstQueueFamily, const VkPipelineStageFlags p_SrcStage, const VkPipelineStageFlags p_DstStage)
    {
        const VkBufferMemoryBarrier l_Barrier{
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = 0,
            .dstAccessMask = 0,
            .srcQueueFamilyIndex = p_SrcQueueFamily,
            .dstQueueFamilyIndex = p_DstQueueFamily,
            .buffer = p_Buffer,
            .offset = p_Offset,
            .size = p_Size,
        };
        p_DeviceData.deviceTable.vkCmdPipelineBarrier(p_Cb, p_SrcStage, p_DstStage, 0, 0, nullptr, 1, &l_Barrier, 0, nullptr);
    }

    VKP_FORCEINLINE void imageOwnershipTransfer2(const device::DeviceData& p_DeviceData, const VkCommandBuffer p_Cb, const VkImage p_Image, const VkImageLayout p_InitialLayout, const VkImageLayout p_FinalLayout, const uint32_t p_SrcQueueFamily, const uint32_t p_DstQueueFamily)
    {
        const detail::StageAccess2 l_Src = detail::srcStageAccess2(p_InitialLayout);
        const detail::StageAccess2 l_Dst = detail::dstStageAccess2(p_FinalLayout);
        const VkImageMemoryBarrier2 l_Barrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcStageMask = l_Src.stage,
            .srcAccessMask = VK_ACCESS_2_NONE,
            .dstStageMask = l_Dst.stage,
            .dstAccessMask = VK_ACCESS_2_NONE,
            .oldLayout = p_InitialLayout,
            .newLayout = p_FinalLayout,
            .srcQueueFamilyIndex = p_SrcQueueFamily,
            .dstQueueFamilyIndex = p_DstQueueFamily,
            .image = p_Image,
            .subresourceRange = VkImageSubresourceRange{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 },
        };
        const VkDependencyInfo l_Dependency{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext = nullptr,
            .dependencyFlags = 0,
            .memoryBarrierCount = 0,
            .pMemoryBarriers = nullptr,
            .bufferMemoryBarrierCount = 0,
            .pBufferMemoryBarriers = nullptr,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &l_Barrier,
        };
        p_DeviceData.deviceTable.vkCmdPipelineBarrier2(p_Cb, &l_Dependency);
    }

    VKP_FORCEINLINE void imageOwnershipTransfer(const device::DeviceData& p_DeviceData, const VkCommandBuffer p_Cb, const VkImage p_Image, const VkImageLayout p_InitialLayout, const VkImageLayout p_FinalLayout, const uint32_t p_SrcQueueFamily, const uint32_t p_DstQueueFamily)
    {
        const detail::StageAccess l_Src = detail::srcStageAccess(p_InitialLayout);
        const detail::StageAccess l_Dst = detail::dstStageAccess(p_FinalLayout);
        const VkImageMemoryBarrier l_Barrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = 0,
            .dstAccessMask = 0,
            .oldLayout = p_InitialLayout,
            .newLayout = p_FinalLayout,
            .srcQueueFamilyIndex = p_SrcQueueFamily,
            .dstQueueFamilyIndex = p_DstQueueFamily,
            .image = p_Image,
            .subresourceRange = VkImageSubresourceRange{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 },
        };
        p_DeviceData.deviceTable.vkCmdPipelineBarrier(p_Cb, l_Src.stage, l_Dst.stage, 0, 0, nullptr, 0, nullptr, 1, &l_Barrier);
    }

    VKP_FORCEINLINE void imageBarrier2(const device::DeviceData& p_DeviceData, const VkCommandBuffer p_Cb, const VkImage p_Image, const VkImageLayout p_InitialLayout, const VkImageLayout p_FinalLayout, const VkImageSubresourceRange p_SubresourceRange, const VkPipelineStageFlags2 p_SrcStage, const VkAccessFlags2 p_SrcAccess, const VkPipelineStageFlags2 p_DstStage, const VkAccessFlags2 p_DstAccess)
    {
        const VkImageMemoryBarrier2 l_Barrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcStageMask = p_SrcStage,
            .srcAccessMask = p_SrcAccess,
            .dstStageMask = p_DstStage,
            .dstAccessMask = p_DstAccess,
            .oldLayout = p_InitialLayout,
            .newLayout = p_FinalLayout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = p_Image,
            .subresourceRange = p_SubresourceRange,
        };
        const VkDependencyInfo l_Dependency{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext = nullptr,
            .dependencyFlags = 0,
            .memoryBarrierCount = 0,
            .pMemoryBarriers = nullptr,
            .bufferMemoryBarrierCount = 0,
            .pBufferMemoryBarriers = nullptr,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &l_Barrier,
        };
        p_DeviceData.deviceTable.vkCmdPipelineBarrier2(p_Cb, &l_Dependency);
    }

    VKP_FORCEINLINE void imageBarrier(const device::DeviceData& p_DeviceData, const VkCommandBuffer p_Cb, const VkImage p_Image, const VkImageLayout p_InitialLayout, const VkImageLayout p_FinalLayout, const VkImageSubresourceRange p_SubresourceRange, const VkPipelineStageFlags p_SrcStage, const VkAccessFlags p_SrcAccess, const VkPipelineStageFlags p_DstStage, const VkAccessFlags p_DstAccess)
    {
        const VkImageMemoryBarrier l_Barrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = p_SrcAccess,
            .dstAccessMask = p_DstAccess,
            .oldLayout = p_InitialLayout,
            .newLayout = p_FinalLayout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = p_Image,
            .subresourceRange = p_SubresourceRange,
        };
        p_DeviceData.deviceTable.vkCmdPipelineBarrier(p_Cb, p_SrcStage, p_DstStage, 0, 0, nullptr, 0, nullptr, 1, &l_Barrier);
    }

    template<typename Allocator = std::allocator<void>>
    VKP_FORCEINLINE void submit2(const device::DeviceData& p_DeviceData, const VkQueue p_Queue, const std::span<const VkCommandBuffer> p_CommandBuffers, const std::span<const SemaphoreSubmit> p_Waits, const std::span<const SemaphoreSubmit> p_Signals, const VkFence p_Fence, const Allocator& p_Allocator = {})
    {
        using SemaphoreInfoAlloc = std::allocator_traits<Allocator>::template rebind_alloc<VkSemaphoreSubmitInfo>;
        using CommandBufferInfoAlloc = std::allocator_traits<Allocator>::template rebind_alloc<VkCommandBufferSubmitInfo>;

    	SmallVector<VkSemaphoreSubmitInfo, 8, SemaphoreInfoAlloc> l_WaitInfos(p_Allocator);
        for (const SemaphoreSubmit& l_Wait : p_Waits)
        {
            l_WaitInfos.push_back({
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .pNext = nullptr,
                .semaphore = l_Wait.semaphore,
                .value = l_Wait.value,
                .stageMask = l_Wait.stageMask,
                .deviceIndex = 0,
                });
        }

        SmallVector<VkSemaphoreSubmitInfo, 8, SemaphoreInfoAlloc> l_SignalInfos(p_Allocator);
        for (const SemaphoreSubmit& l_Signal : p_Signals)
        {
            l_SignalInfos.push_back({
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .pNext = nullptr,
                .semaphore = l_Signal.semaphore,
                .value = l_Signal.value,
                .stageMask = l_Signal.stageMask,
                .deviceIndex = 0,
                });
        }

        SmallVector<VkCommandBufferSubmitInfo, 8, CommandBufferInfoAlloc> l_CbInfos(p_Allocator);
        for (const VkCommandBuffer l_Cb : p_CommandBuffers)
        {
            l_CbInfos.push_back({
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .pNext = nullptr,
                .commandBuffer = l_Cb,
                .deviceMask = 0,
                });
        }

        const VkSubmitInfo2 l_Submit{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .pNext = nullptr,
            .flags = 0,
            .waitSemaphoreInfoCount = static_cast<uint32_t>(l_WaitInfos.size()),
            .pWaitSemaphoreInfos = l_WaitInfos.data(),
            .commandBufferInfoCount = static_cast<uint32_t>(l_CbInfos.size()),
            .pCommandBufferInfos = l_CbInfos.data(),
            .signalSemaphoreInfoCount = static_cast<uint32_t>(l_SignalInfos.size()),
            .pSignalSemaphoreInfos = l_SignalInfos.data(),
        };
        VULKAN_TRY(p_DeviceData.deviceTable.vkQueueSubmit2(p_Queue, 1, &l_Submit, p_Fence));
    }

    VKP_FORCEINLINE void waitTimeline(const device::DeviceData& p_DeviceData, const VkSemaphore p_Timeline, const uint64_t p_Value, const uint64_t p_Timeout = UINT64_MAX)
    {
        const VkSemaphoreWaitInfo l_Wait{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
            .pNext = nullptr,
            .flags = 0,
            .semaphoreCount = 1,
            .pSemaphores = &p_Timeline,
            .pValues = &p_Value,
        };
        VULKAN_TRY(p_DeviceData.deviceTable.vkWaitSemaphores(p_DeviceData.device, &l_Wait, p_Timeout));
    }

    VKP_FORCEINLINE void signalTimeline(const device::DeviceData& p_DeviceData, const VkQueue p_Queue, const VkSemaphore p_Timeline, const uint64_t p_Value)
    {
        const VkSemaphoreSubmitInfo l_Signal{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext = nullptr,
            .semaphore = p_Timeline,
            .value = p_Value,
            .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .deviceIndex = 0,
        };
        const VkSubmitInfo2 l_Submit{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .pNext = nullptr,
            .flags = 0,
            .waitSemaphoreInfoCount = 0,
            .pWaitSemaphoreInfos = nullptr,
            .commandBufferInfoCount = 0,
            .pCommandBufferInfos = nullptr,
            .signalSemaphoreInfoCount = 1,
            .pSignalSemaphoreInfos = &l_Signal,
        };
        VULKAN_TRY(p_DeviceData.deviceTable.vkQueueSubmit2(p_Queue, 1, &l_Submit, VK_NULL_HANDLE));
    }
}
