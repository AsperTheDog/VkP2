#pragma once

#include <memory>

#include "vkp2/command_buffer.hpp"
#include "vkp2/dyn/extra/vector.hpp"

namespace vkp::dyn
{
	template<typename Allocator = std::allocator<void>>
	class BarrierStorage
	{
	public:
		using MemoryAllocator = std::allocator_traits<Allocator>::template rebind_alloc<VkMemoryBarrier2>;
		using BufferAllocator = std::allocator_traits<Allocator>::template rebind_alloc<VkBufferMemoryBarrier2>;
		using ImageAllocator = std::allocator_traits<Allocator>::template rebind_alloc<VkImageMemoryBarrier2>;

		BarrierStorage() = default;

		explicit BarrierStorage(const Allocator& p_Allocator)
			: m_Memory(MemoryAllocator(p_Allocator))
			, m_Buffer(BufferAllocator(p_Allocator))
			, m_Image(ImageAllocator(p_Allocator))
		{
		}

		[[nodiscard]] VkMemoryBarrier2& emplaceMemory() { return m_Memory.emplace_back(); }
		[[nodiscard]] VkBufferMemoryBarrier2& emplaceBuffer() { return m_Buffer.emplace_back(); }
		[[nodiscard]] VkImageMemoryBarrier2& emplaceImage() { return m_Image.emplace_back(); }

		[[nodiscard]] const VkMemoryBarrier2* memoryData() const { return m_Memory.empty() ? nullptr : m_Memory.data(); }
		[[nodiscard]] const VkBufferMemoryBarrier2* bufferData() const { return m_Buffer.empty() ? nullptr : m_Buffer.data(); }
		[[nodiscard]] const VkImageMemoryBarrier2* imageData() const { return m_Image.empty() ? nullptr : m_Image.data(); }

		[[nodiscard]] uint32_t memoryCount() const { return static_cast<uint32_t>(m_Memory.size()); }
		[[nodiscard]] uint32_t bufferCount() const { return static_cast<uint32_t>(m_Buffer.size()); }
		[[nodiscard]] uint32_t imageCount() const { return static_cast<uint32_t>(m_Image.size()); }

		void clear()
		{
			m_Memory.clear();
			m_Buffer.clear();
			m_Image.clear();
		}

	private:
		Vector<VkMemoryBarrier2, MemoryAllocator> m_Memory;
		Vector<VkBufferMemoryBarrier2, BufferAllocator> m_Buffer;
		Vector<VkImageMemoryBarrier2, ImageAllocator> m_Image;
	};

	template<typename Allocator = std::allocator<void>>
	using BarrierBuilder = cmd::BarrierBuilder<BarrierStorage<Allocator>>;

	template<typename Allocator>
	[[nodiscard]] BarrierBuilder<Allocator> makeBarrierBuilder(const Allocator& p_Allocator)
	{
		return BarrierBuilder<Allocator>(BarrierStorage<Allocator>(p_Allocator));
	}
}
