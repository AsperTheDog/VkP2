#pragma once

#include <algorithm>

namespace vkp::cmd
{
    template<uint32_t MemoryCount, uint32_t BufferCount, uint32_t ImageCount>
    VKP_FORCEINLINE VkMemoryBarrier2& ArrayBarrierStorage<MemoryCount, BufferCount, ImageCount>::emplaceMemory()
    {
        if (m_MemoryCount == MemoryCount)
        {
            throw std::runtime_error("vkp::cmd: no room for another memory barrier in this storage");
        }
        return m_Memory[m_MemoryCount++];
    }

    template<uint32_t MemoryCount, uint32_t BufferCount, uint32_t ImageCount>
    VKP_FORCEINLINE VkBufferMemoryBarrier2& ArrayBarrierStorage<MemoryCount, BufferCount, ImageCount>::emplaceBuffer()
    {
        if (m_BufferCount == BufferCount)
        {
            throw std::runtime_error("vkp::cmd: no room for another buffer barrier in this storage");
        }
        return m_Buffer[m_BufferCount++];
    }

    template<uint32_t MemoryCount, uint32_t BufferCount, uint32_t ImageCount>
    VKP_FORCEINLINE VkImageMemoryBarrier2& ArrayBarrierStorage<MemoryCount, BufferCount, ImageCount>::emplaceImage()
    {
        if (m_ImageCount == ImageCount)
        {
            throw std::runtime_error("vkp::cmd: no room for another image barrier in this storage");
        }
        return m_Image[m_ImageCount++];
    }

    template<uint32_t MemoryCount, uint32_t BufferCount, uint32_t ImageCount>
    VKP_FORCEINLINE const VkMemoryBarrier2* ArrayBarrierStorage<MemoryCount, BufferCount, ImageCount>::memoryData() const
    {
        return m_MemoryCount == 0 ? nullptr : m_Memory.data();
    }

    template<uint32_t MemoryCount, uint32_t BufferCount, uint32_t ImageCount>
    VKP_FORCEINLINE const VkBufferMemoryBarrier2* ArrayBarrierStorage<MemoryCount, BufferCount, ImageCount>::bufferData() const
    {
        return m_BufferCount == 0 ? nullptr : m_Buffer.data();
    }

    template<uint32_t MemoryCount, uint32_t BufferCount, uint32_t ImageCount>
    VKP_FORCEINLINE const VkImageMemoryBarrier2* ArrayBarrierStorage<MemoryCount, BufferCount, ImageCount>::imageData() const
    {
        return m_ImageCount == 0 ? nullptr : m_Image.data();
    }

    template<uint32_t MemoryCount, uint32_t BufferCount, uint32_t ImageCount>
    VKP_FORCEINLINE uint32_t ArrayBarrierStorage<MemoryCount, BufferCount, ImageCount>::memoryCount() const
    {
        return m_MemoryCount;
    }

    template<uint32_t MemoryCount, uint32_t BufferCount, uint32_t ImageCount>
    VKP_FORCEINLINE uint32_t ArrayBarrierStorage<MemoryCount, BufferCount, ImageCount>::bufferCount() const
    {
        return m_BufferCount;
    }

    template<uint32_t MemoryCount, uint32_t BufferCount, uint32_t ImageCount>
    VKP_FORCEINLINE uint32_t ArrayBarrierStorage<MemoryCount, BufferCount, ImageCount>::imageCount() const
    {
        return m_ImageCount;
    }

    template<uint32_t MemoryCount, uint32_t BufferCount, uint32_t ImageCount>
    VKP_FORCEINLINE void ArrayBarrierStorage<MemoryCount, BufferCount, ImageCount>::clear()
    {
        m_MemoryCount = 0;
        m_BufferCount = 0;
        m_ImageCount = 0;
    }

    template<BarrierStoragePolicy Storage>
    VKP_FORCEINLINE BarrierBuilder<Storage>& BarrierBuilder<Storage>::memory(const VkPipelineStageFlags2 p_SrcStage, const VkAccessFlags2 p_SrcAccess, const VkPipelineStageFlags2 p_DstStage, const VkAccessFlags2 p_DstAccess)
    {
        m_Storage.emplaceMemory() = VkMemoryBarrier2{
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcStageMask = p_SrcStage,
            .srcAccessMask = p_SrcAccess,
            .dstStageMask = p_DstStage,
            .dstAccessMask = p_DstAccess,
        };
        return *this;
    }

    template<BarrierStoragePolicy Storage>
    VKP_FORCEINLINE BarrierBuilder<Storage>& BarrierBuilder<Storage>::buffer(const VkBuffer p_Buffer, const VkDeviceSize p_Offset, const VkDeviceSize p_Size, const VkPipelineStageFlags2 p_SrcStage, const VkAccessFlags2 p_SrcAccess, const VkPipelineStageFlags2 p_DstStage, const VkAccessFlags2 p_DstAccess, const uint32_t p_SrcQueueFamily, const uint32_t p_DstQueueFamily)
    {
        m_Storage.emplaceBuffer() = VkBufferMemoryBarrier2{
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcStageMask = p_SrcStage,
            .srcAccessMask = p_SrcAccess,
            .dstStageMask = p_DstStage,
            .dstAccessMask = p_DstAccess,
            .srcQueueFamilyIndex = p_SrcQueueFamily,
            .dstQueueFamilyIndex = p_DstQueueFamily,
            .buffer = p_Buffer,
            .offset = p_Offset,
            .size = p_Size,
        };
        return *this;
    }

    template<BarrierStoragePolicy Storage>
    VKP_FORCEINLINE BarrierBuilder<Storage>& BarrierBuilder<Storage>::image(const VkImage p_Image, const VkImageSubresourceRange& p_Range, const ImageBarrierData& p_ImageData)
    {
        m_Storage.emplaceImage() = VkImageMemoryBarrier2{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcStageMask = p_ImageData.srcStage,
            .srcAccessMask = p_ImageData.srcAccess,
            .dstStageMask = p_ImageData.dstStage,
            .dstAccessMask = p_ImageData.dstAccess,
            .oldLayout = p_ImageData.oldLayout,
            .newLayout = p_ImageData.newLayout,
            .srcQueueFamilyIndex = p_ImageData.srcQueueFamily,
            .dstQueueFamilyIndex = p_ImageData.dstQueueFamily,
            .image = p_Image,
            .subresourceRange = p_Range,
        };
        return *this;
    }

    template<BarrierStoragePolicy Storage>
    VKP_FORCEINLINE BarrierBuilder<Storage>& BarrierBuilder<Storage>::image(const VkImage p_Image, const ImageProperties& p_Properties, const ImageBarrierData& p_ImageData)
    {
        return image(p_Image, p_Properties.subresourceRange(), p_ImageData);
    }

    template<BarrierStoragePolicy Storage>
    VKP_FORCEINLINE BarrierBuilder<Storage>& BarrierBuilder<Storage>::image(const Image& p_Image, const ImageBarrierData& p_ImageData)
    {
        return image(p_Image.data.image, p_Image.properties, p_ImageData);
    }

    template<BarrierStoragePolicy Storage>
    VKP_FORCEINLINE BarrierBuilder<Storage>& BarrierBuilder<Storage>::setDependencyFlags(const VkDependencyFlags p_Flags)
    {
        m_DependencyFlags = p_Flags;
        return *this;
    }

    template<BarrierStoragePolicy Storage>
    VKP_FORCEINLINE BarrierBuilder<Storage>& BarrierBuilder<Storage>::clear()
    {
        m_Storage.clear();
        m_DependencyFlags = 0;
        return *this;
    }

    template<BarrierStoragePolicy Storage>
    VKP_FORCEINLINE VkDependencyInfo BarrierBuilder<Storage>::build() const
    {
        return VkDependencyInfo{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext = nullptr,
            .dependencyFlags = m_DependencyFlags,
            .memoryBarrierCount = m_Storage.memoryCount(),
            .pMemoryBarriers = m_Storage.memoryData(),
            .bufferMemoryBarrierCount = m_Storage.bufferCount(),
            .pBufferMemoryBarriers = m_Storage.bufferData(),
            .imageMemoryBarrierCount = m_Storage.imageCount(),
            .pImageMemoryBarriers = m_Storage.imageData(),
        };
    }

    template<BarrierStoragePolicy Storage>
    VKP_FORCEINLINE void BarrierBuilder<Storage>::record(const device::DeviceData& p_DeviceData, const VkCommandBuffer p_Cb) const
    {
        const VkDependencyInfo l_Dependency = build();
        p_DeviceData.deviceTable.vkCmdPipelineBarrier2(p_Cb, &l_Dependency);
    }

    VKP_FORCEINLINE RenderingInfoBuilder& RenderingInfoBuilder::setRenderArea(const VkRect2D p_Area)
    {
        m_RenderArea = p_Area;
        return *this;
    }

    VKP_FORCEINLINE RenderingInfoBuilder& RenderingInfoBuilder::setRenderArea(const VkExtent2D p_Extent)
    {
        return setRenderArea(VkRect2D{ .offset = { .x = 0, .y = 0 }, .extent = p_Extent });
    }

    VKP_FORCEINLINE RenderingInfoBuilder& RenderingInfoBuilder::setLayers(const uint32_t p_LayerCount)
    {
        m_LayerCount = p_LayerCount;
        return *this;
    }

    VKP_FORCEINLINE RenderingInfoBuilder& RenderingInfoBuilder::setViewMask(const uint32_t p_ViewMask)
    {
        m_ViewMask = p_ViewMask;
        return *this;
    }

    VKP_FORCEINLINE RenderingInfoBuilder& RenderingInfoBuilder::setFlags(const VkRenderingFlags p_Flags)
    {
        m_Flags = p_Flags;
        return *this;
    }

    VKP_FORCEINLINE RenderingInfoBuilder& RenderingInfoBuilder::color(VkRenderingAttachmentInfo p_Attachment)
    {
        if (m_ColorCount == kMaxColorAttachments)
        {
            throw std::runtime_error("vkp::cmd: no room for another color attachment");
        }
        p_Attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        m_ColorAttachments[m_ColorCount++] = p_Attachment;
        return *this;
    }

    VKP_FORCEINLINE RenderingInfoBuilder& RenderingInfoBuilder::depth(VkRenderingAttachmentInfo p_Attachment)
    {
        p_Attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        m_DepthAttachment = p_Attachment;
        m_HasDepth = true;
        return *this;
    }

    VKP_FORCEINLINE RenderingInfoBuilder& RenderingInfoBuilder::stencil(VkRenderingAttachmentInfo p_Attachment)
    {
        p_Attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        m_StencilAttachment = p_Attachment;
        m_HasStencil = true;
        return *this;
    }

    VKP_FORCEINLINE RenderingInfoBuilder& RenderingInfoBuilder::clear()
    {
        m_ColorCount = 0;
        m_HasDepth = false;
        m_HasStencil = false;
        return *this;
    }

    VKP_FORCEINLINE VkRenderingInfo RenderingInfoBuilder::build() const
    {
        return VkRenderingInfo{
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .pNext = nullptr,
            .flags = m_Flags,
            .renderArea = m_RenderArea,
            .layerCount = m_LayerCount,
            .viewMask = m_ViewMask,
            .colorAttachmentCount = m_ColorCount,
            .pColorAttachments = m_ColorCount == 0 ? nullptr : m_ColorAttachments.data(),
            .pDepthAttachment = m_HasDepth ? &m_DepthAttachment : nullptr,
            .pStencilAttachment = m_HasStencil ? &m_StencilAttachment : nullptr,
        };
    }

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

    template<bool AutoViewport, typename F> requires std::invocable<F, VkCommandBuffer>
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
    void immediateSubmitScope(device::DeviceData& p_DeviceData, const VkDevice p_Device, const VkCommandPool p_Pool, const VkQueue p_Queue, F&& p_Lambda, const VkFence p_UserFence)
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

        const VkCommandBufferSubmitInfo l_CommandBuffer{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .pNext = nullptr,
            .commandBuffer = l_Cb,
            .deviceMask = 0,
        };
        const VkSubmitInfo2 l_SubmitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .pNext = nullptr,
            .flags = 0,
            .waitSemaphoreInfoCount = 0,
            .pWaitSemaphoreInfos = nullptr,
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &l_CommandBuffer,
            .signalSemaphoreInfoCount = 0,
            .pSignalSemaphoreInfos = nullptr,
        };

        VULKAN_TRY(p_DeviceData.deviceTable.vkQueueSubmit2(p_Queue, 1, &l_SubmitInfo, l_Fence));
        p_DeviceData.deviceTable.vkWaitForFences(p_Device, 1, &l_Fence, VK_TRUE, UINT64_MAX);
        p_DeviceData.deviceTable.vkFreeCommandBuffers(p_Device, p_Pool, 1, &l_Cb);

        if (l_OwnedFence)
        {
            p_DeviceData.deviceTable.vkDestroyFence(p_Device, l_Fence, nullptr);
        }
    }

    template<typename F> requires std::invocable<F, VkCommandBuffer>
    VKP_FORCEINLINE void timestampScope(device::DeviceData& p_DeviceData, VkCommandBuffer p_Cb, const VkQueryPool p_Pool, const uint32_t p_StartQueryIdx, const VkPipelineStageFlags2 p_Stage, F&& p_Lambda)
    {
        p_DeviceData.deviceTable.vkCmdWriteTimestamp2(p_Cb, p_Stage, p_Pool, p_StartQueryIdx);
        std::forward<F>(p_Lambda)(p_Cb);
        p_DeviceData.deviceTable.vkCmdWriteTimestamp2(p_Cb, p_Stage, p_Pool, p_StartQueryIdx + 1);
    }

    VKP_FORCEINLINE void pushConstants(const device::DeviceData& p_DeviceData, const VkCommandBuffer p_Cb, const VkPipelineLayout p_Layout, const VkShaderStageFlags p_StageFlags, const uint32_t p_Offset, const uint32_t p_Size, const void* p_Values)
    {
        p_DeviceData.deviceTable.vkCmdPushConstants(p_Cb, p_Layout, p_StageFlags, p_Offset, p_Size, p_Values);
    }

    template<uint32_t MaxCommandBuffers, uint32_t MaxWaits, uint32_t MaxSignals>
    VKP_FORCEINLINE void submit2(const device::DeviceData& p_DeviceData, const VkQueue p_Queue, const std::span<const VkCommandBuffer> p_CommandBuffers, const std::span<const SemaphoreSubmit> p_Waits, const std::span<const SemaphoreSubmit> p_Signals, const VkFence p_Fence)
    {
        StaticVector<VkSemaphoreSubmitInfo, MaxWaits> l_WaitInfos;
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

        StaticVector<VkSemaphoreSubmitInfo, MaxSignals> l_SignalInfos;
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

        StaticVector<VkCommandBufferSubmitInfo, MaxCommandBuffers> l_CbInfos;
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

    VKP_FORCEINLINE void waitTimeline(const device::DeviceData& p_DeviceData, const VkSemaphore p_Timeline, const uint64_t p_Value, const uint64_t p_Timeout)
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
