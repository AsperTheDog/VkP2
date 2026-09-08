#pragma once
#include <span>
#include <vector>
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

    enum class SyncMode
    {
        Sync2,
        Legacy,
    };

    namespace detail
    {
        struct StageAccess2
        {
            VkPipelineStageFlags2 stage;
            VkAccessFlags2 access;
        };

        struct StageAccess
        {
            VkPipelineStageFlags stage;
            VkAccessFlags access;
        };

    }

    struct Submit2Info
    {
        VkCommandBuffer commandBuffer;
        VkSemaphore waitSemaphore;
        VkPipelineStageFlags2 waitStage;
        VkSemaphore signalSemaphore;
    };

    struct AttachmentSpec
    {
        VkImageView view;
        VkImage image;
        VkImageLayout initialLayout;
        VkImageLayout finalLayout;
        VkAttachmentLoadOp loadOp;
        VkClearValue clearValue;
    };

    struct FrameSpec
    {
        std::span<const AttachmentSpec> colors;
        const AttachmentSpec* depth = nullptr;
        VkExtent2D extent{};
    };

    struct SemaphoreSubmit
    {
        VkSemaphore semaphore;
        VkPipelineStageFlags2 stageMask;
        uint64_t value;
    };

    template<typename F> requires std::invocable<F, VkCommandBuffer>
    VKP_FORCEINLINE void recordingScope(device::DeviceData& p_DeviceData, VkCommandBuffer p_Cb, bool p_OneTime, F&& p_Lambda);
    
	template<bool AutoViewport, typename F> requires std::invocable<F, VkCommandBuffer>
    VKP_FORCEINLINE void renderScope(device::DeviceData& p_DeviceData, VkCommandBuffer p_Cb, const VkRenderingInfo& p_RenderInfo, F&& p_Lambda);
    
	template<typename F> requires std::invocable<F, VkCommandBuffer>
    VKP_FORCEINLINE void debugScope(VkCommandBuffer p_Cb, const char* p_Name, const float p_Color[4], F&& p_Lambda);
    
	template<typename F> requires std::invocable<F, VkCommandBuffer>
    void immediateSubmitScope(device::DeviceData& p_DeviceData, VkDevice p_Device, VkCommandPool p_Pool, VkQueue p_Queue, F&& p_Lambda, VkFence p_UserFence);
    
	template<typename F> requires std::invocable<F, VkCommandBuffer>
    VKP_FORCEINLINE void timestampScope(device::DeviceData& p_DeviceData, VkCommandBuffer p_Cb, VkQueryPool p_Pool, uint32_t p_StartQueryIdx, VkPipelineStageFlagBits p_Stage, F&& p_Lambda);
    
	VKP_FORCEINLINE void pushConstants(const device::DeviceData& p_DeviceData, VkCommandBuffer p_Cb, VkPipelineLayout p_Layout, VkShaderStageFlags p_StageFlags, uint32_t p_Offset, uint32_t p_Size, const void* p_Values);
    VKP_FORCEINLINE void transitionImage(const device::DeviceData& p_DeviceData, VkCommandBuffer p_Cb, VkImage p_Image, VkImageLayout p_InitialLayout, VkImageLayout p_FinalLayout, VkImageAspectFlags p_Aspect, SyncMode p_Mode);
    VKP_FORCEINLINE void submit2(const device::DeviceData& p_DeviceData, VkQueue p_Queue, const Submit2Info& p_Info, VkFence p_Fence);
    
	template<SyncMode Mode, typename F> requires std::invocable<F, VkCommandBuffer>
    void frameRenderScope(device::DeviceData& p_DeviceData, VkCommandBuffer p_Cb, const FrameSpec& p_Spec, F&& p_Lambda);
    
	VKP_FORCEINLINE void bufferBarrier2(const device::DeviceData& p_DeviceData, VkCommandBuffer p_Cb, VkBuffer p_Buffer, VkDeviceSize p_Offset, VkDeviceSize p_Size, VkPipelineStageFlags2 p_SrcStage, VkAccessFlags2 p_SrcAccess, VkPipelineStageFlags2 p_DstStage, VkAccessFlags2 p_DstAccess);
    VKP_FORCEINLINE void bufferBarrier(const device::DeviceData& p_DeviceData, VkCommandBuffer p_Cb, VkBuffer p_Buffer, VkDeviceSize p_Offset, VkDeviceSize p_Size, VkPipelineStageFlags p_SrcStage, VkAccessFlags p_SrcAccess, VkPipelineStageFlags p_DstStage, VkAccessFlags p_DstAccess);
    VKP_FORCEINLINE void memoryBarrier2(const device::DeviceData& p_DeviceData, VkCommandBuffer p_Cb, VkPipelineStageFlags2 p_SrcStage, VkAccessFlags2 p_SrcAccess, VkPipelineStageFlags2 p_DstStage, VkAccessFlags2 p_DstAccess);
    VKP_FORCEINLINE void memoryBarrier(const device::DeviceData& p_DeviceData, VkCommandBuffer p_Cb, VkPipelineStageFlags p_SrcStage, VkAccessFlags p_SrcAccess, VkPipelineStageFlags p_DstStage, VkAccessFlags p_DstAccess);
    VKP_FORCEINLINE void bufferOwnershipTransfer2(const device::DeviceData& p_DeviceData, VkCommandBuffer p_Cb, VkBuffer p_Buffer, VkDeviceSize p_Offset, VkDeviceSize p_Size, uint32_t p_SrcQueueFamily, uint32_t p_DstQueueFamily, VkPipelineStageFlags2 p_SrcStage, VkPipelineStageFlags2 p_DstStage);
    VKP_FORCEINLINE void bufferOwnershipTransfer(const device::DeviceData& p_DeviceData, VkCommandBuffer p_Cb, VkBuffer p_Buffer, VkDeviceSize p_Offset, VkDeviceSize p_Size, uint32_t p_SrcQueueFamily, uint32_t p_DstQueueFamily, VkPipelineStageFlags p_SrcStage, VkPipelineStageFlags p_DstStage);
    VKP_FORCEINLINE void imageOwnershipTransfer2(const device::DeviceData& p_DeviceData, VkCommandBuffer p_Cb, VkImage p_Image, VkImageLayout p_InitialLayout, VkImageLayout p_FinalLayout, uint32_t p_SrcQueueFamily, uint32_t p_DstQueueFamily);
    VKP_FORCEINLINE void imageOwnershipTransfer(const device::DeviceData& p_DeviceData, VkCommandBuffer p_Cb, VkImage p_Image, VkImageLayout p_InitialLayout, VkImageLayout p_FinalLayout, uint32_t p_SrcQueueFamily, uint32_t p_DstQueueFamily);
    VKP_FORCEINLINE void imageBarrier2(const device::DeviceData& p_DeviceData, VkCommandBuffer p_Cb, VkImage p_Image, VkImageLayout p_InitialLayout, VkImageLayout p_FinalLayout, VkImageSubresourceRange p_SubresourceRange, VkPipelineStageFlags2 p_SrcStage, VkAccessFlags2 p_SrcAccess, VkPipelineStageFlags2 p_DstStage, VkAccessFlags2 p_DstAccess);
    VKP_FORCEINLINE void imageBarrier(const device::DeviceData& p_DeviceData, VkCommandBuffer p_Cb, VkImage p_Image, VkImageLayout p_InitialLayout, VkImageLayout p_FinalLayout, VkImageSubresourceRange p_SubresourceRange, VkPipelineStageFlags p_SrcStage, VkAccessFlags p_SrcAccess, VkPipelineStageFlags p_DstStage, VkAccessFlags p_DstAccess);
    
	template<typename Allocator>
    VKP_FORCEINLINE void submit2(const device::DeviceData& p_DeviceData, VkQueue p_Queue, std::span<const VkCommandBuffer> p_CommandBuffers, std::span<const SemaphoreSubmit> p_Waits, std::span<const SemaphoreSubmit> p_Signals, VkFence p_Fence, const Allocator& p_Allocator);
    
	VKP_FORCEINLINE void waitTimeline(const device::DeviceData& p_DeviceData, VkSemaphore p_Timeline, uint64_t p_Value, uint64_t p_Timeout);
    VKP_FORCEINLINE void signalTimeline(const device::DeviceData& p_DeviceData, VkQueue p_Queue, VkSemaphore p_Timeline, uint64_t p_Value);

}

#include "inline/command_buffer.inl"
