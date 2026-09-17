#pragma once
#include <array>
#include <span>
#include <stdexcept>
#include <vector>
#include <volk.h>

#include "device.hpp"
#include "image.hpp"
#include "extra/static_vector.hpp"

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

    struct SemaphoreSubmit
    {
        VkSemaphore semaphore;
        VkPipelineStageFlags2 stageMask;
        uint64_t value;
    };

    template<uint32_t MemoryCount, uint32_t BufferCount, uint32_t ImageCount>
    class ArrayBarrierStorage
    {
    public:
        ArrayBarrierStorage() = default;

        [[nodiscard]] VKP_FORCEINLINE VkMemoryBarrier2& emplaceMemory();
        [[nodiscard]] VKP_FORCEINLINE VkBufferMemoryBarrier2& emplaceBuffer();
        [[nodiscard]] VKP_FORCEINLINE VkImageMemoryBarrier2& emplaceImage();

        [[nodiscard]] VKP_FORCEINLINE const VkMemoryBarrier2* memoryData() const;
        [[nodiscard]] VKP_FORCEINLINE const VkBufferMemoryBarrier2* bufferData() const;
        [[nodiscard]] VKP_FORCEINLINE const VkImageMemoryBarrier2* imageData() const;

        [[nodiscard]] VKP_FORCEINLINE uint32_t memoryCount() const;
        [[nodiscard]] VKP_FORCEINLINE uint32_t bufferCount() const;
        [[nodiscard]] VKP_FORCEINLINE uint32_t imageCount() const;

        VKP_FORCEINLINE void clear();

    private:
        std::array<VkMemoryBarrier2, MemoryCount> m_Memory{};
        std::array<VkBufferMemoryBarrier2, BufferCount> m_Buffer{};
        std::array<VkImageMemoryBarrier2, ImageCount> m_Image{};

        uint32_t m_MemoryCount = 0;
        uint32_t m_BufferCount = 0;
        uint32_t m_ImageCount = 0;
    };

    template<typename T>
    concept BarrierStoragePolicy = requires(T& p_Storage, const T& p_ConstStorage)
    {
        { p_Storage.emplaceMemory() } -> std::same_as<VkMemoryBarrier2&>;
        { p_Storage.emplaceBuffer() } -> std::same_as<VkBufferMemoryBarrier2&>;
        { p_Storage.emplaceImage() } -> std::same_as<VkImageMemoryBarrier2&>;
        { p_ConstStorage.memoryCount() } -> std::convertible_to<uint32_t>;
        { p_ConstStorage.bufferCount() } -> std::convertible_to<uint32_t>;
        { p_ConstStorage.imageCount() } -> std::convertible_to<uint32_t>;
        { p_ConstStorage.memoryData() } -> std::convertible_to<const VkMemoryBarrier2*>;
        { p_ConstStorage.bufferData() } -> std::convertible_to<const VkBufferMemoryBarrier2*>;
        { p_ConstStorage.imageData() } -> std::convertible_to<const VkImageMemoryBarrier2*>;
        p_Storage.clear();
    };

    template<BarrierStoragePolicy Storage>
    class BarrierBuilder
    {
    public:
        struct ImageBarrierData
        {
            VkPipelineStageFlags2 srcStage;
            VkAccessFlags2 srcAccess;
            VkPipelineStageFlags2 dstStage;
            VkAccessFlags2 dstAccess;
            const uint32_t srcQueueFamily = VK_QUEUE_FAMILY_IGNORED;
            const uint32_t dstQueueFamily = VK_QUEUE_FAMILY_IGNORED;
        };

        BarrierBuilder() = default;

        explicit BarrierBuilder(Storage p_Storage) : m_Storage(std::move(p_Storage)) {}

        BarrierBuilder& memory(VkPipelineStageFlags2 p_SrcStage, VkAccessFlags2 p_SrcAccess, VkPipelineStageFlags2 p_DstStage, VkAccessFlags2 p_DstAccess);
        BarrierBuilder& buffer(VkBuffer p_Buffer, VkDeviceSize p_Offset, VkDeviceSize p_Size, VkPipelineStageFlags2 p_SrcStage, VkAccessFlags2 p_SrcAccess, VkPipelineStageFlags2 p_DstStage, VkAccessFlags2 p_DstAccess, uint32_t p_SrcQueueFamily = VK_QUEUE_FAMILY_IGNORED, uint32_t p_DstQueueFamily = VK_QUEUE_FAMILY_IGNORED);
        BarrierBuilder& image(VkImage p_Image, const VkImageSubresourceRange& p_Range, ImageBarrierData p_ImageData);
        BarrierBuilder& image(VkImage p_Image, const ImageProperties& p_Properties, ImageBarrierData p_ImageData);
        BarrierBuilder& image(const Image& p_Image, ImageBarrierData p_ImageData);

        BarrierBuilder& setDependencyFlags(VkDependencyFlags p_Flags);
        BarrierBuilder& clear();

        [[nodiscard]] VkDependencyInfo build() const;
        VKP_FORCEINLINE void record(const device::DeviceData& p_DeviceData, VkCommandBuffer p_Cb) const;

    private:
        Storage m_Storage{};
        VkDependencyFlags m_DependencyFlags = 0;
    };

    template<uint32_t MemoryCount, uint32_t BufferCount, uint32_t ImageCount>
    using BasicBarrierBuilder = BarrierBuilder<ArrayBarrierStorage<MemoryCount, BufferCount, ImageCount>>;

    class RenderingInfoBuilder
    {
    public:
        RenderingInfoBuilder() = default;

        RenderingInfoBuilder& setRenderArea(VkRect2D p_Area);
        RenderingInfoBuilder& setRenderArea(VkExtent2D p_Extent);

        RenderingInfoBuilder& setLayers(uint32_t p_LayerCount);
        RenderingInfoBuilder& setViewMask(uint32_t p_ViewMask);
        RenderingInfoBuilder& setFlags(VkRenderingFlags p_Flags);

        RenderingInfoBuilder& color(VkRenderingAttachmentInfo p_Attachment);
        RenderingInfoBuilder& depth(VkRenderingAttachmentInfo p_Attachment);
        RenderingInfoBuilder& stencil(VkRenderingAttachmentInfo p_Attachment);

        RenderingInfoBuilder& clear();

        [[nodiscard]] VkRenderingInfo build() const;

    private:
        static constexpr uint32_t kMaxColorAttachments = 8;

        std::array<VkRenderingAttachmentInfo, kMaxColorAttachments> m_ColorAttachments{};
        uint32_t m_ColorCount = 0;
        VkRenderingAttachmentInfo m_DepthAttachment{};
        bool m_HasDepth = false;
        VkRenderingAttachmentInfo m_StencilAttachment{};
        bool m_HasStencil = false;
        VkRect2D m_RenderArea{};
        uint32_t m_LayerCount = 1;
        uint32_t m_ViewMask = 0;
        VkRenderingFlags m_Flags = 0;
    };

    template<typename F> requires std::invocable<F, VkCommandBuffer>
    VKP_FORCEINLINE void recordingScope(device::DeviceData& p_DeviceData, VkCommandBuffer p_Cb, bool p_OneTime, F&& p_Lambda);
    
	template<bool AutoViewport = true, typename F> requires std::invocable<F, VkCommandBuffer>
    VKP_FORCEINLINE void renderScope(device::DeviceData& p_DeviceData, VkCommandBuffer p_Cb, const VkRenderingInfo& p_RenderInfo, F&& p_Lambda);
    
	template<typename F> requires std::invocable<F, VkCommandBuffer>
    VKP_FORCEINLINE void debugScope(VkCommandBuffer p_Cb, const char* p_Name, const float p_Color[4], F&& p_Lambda);
    
	template<typename F> requires std::invocable<F, VkCommandBuffer>
    void immediateSubmitScope(device::DeviceData& p_DeviceData, VkDevice p_Device, VkCommandPool p_Pool, VkQueue p_Queue, F&& p_Lambda, VkFence p_UserFence = VK_NULL_HANDLE);
    
	template<typename F> requires std::invocable<F, VkCommandBuffer>
    VKP_FORCEINLINE void timestampScope(device::DeviceData& p_DeviceData, VkCommandBuffer p_Cb, VkQueryPool p_Pool, uint32_t p_StartQueryIdx, VkPipelineStageFlags2 p_Stage, F&& p_Lambda);
    
	VKP_FORCEINLINE void pushConstants(const device::DeviceData& p_DeviceData, VkCommandBuffer p_Cb, VkPipelineLayout p_Layout, VkShaderStageFlags p_StageFlags, uint32_t p_Offset, uint32_t p_Size, const void* p_Values);

	template<uint32_t MaxCommandBuffers, uint32_t MaxWaits, uint32_t MaxSignals>
    VKP_FORCEINLINE void submit2(const device::DeviceData& p_DeviceData, VkQueue p_Queue, std::span<const VkCommandBuffer> p_CommandBuffers, std::span<const SemaphoreSubmit> p_Waits, std::span<const SemaphoreSubmit> p_Signals, VkFence p_Fence);
    
	VKP_FORCEINLINE void waitTimeline(const device::DeviceData& p_DeviceData, VkSemaphore p_Timeline, uint64_t p_Value, uint64_t p_Timeout = UINT64_MAX);
    VKP_FORCEINLINE void signalTimeline(const device::DeviceData& p_DeviceData, VkQueue p_Queue, VkSemaphore p_Timeline, uint64_t p_Value);
}

#include "inline/command_buffer.inl"
