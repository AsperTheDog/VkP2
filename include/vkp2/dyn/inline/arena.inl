#pragma once

#include <algorithm>
#include <cassert>
#include <new>

namespace vkp::dyn
{
	namespace detail
	{
		inline constexpr size_t kMinChunkBytes = 16 * 1024;

		[[nodiscard]] inline void* heapChunkAllocate(const size_t p_Bytes, void*)
		{
			return ::operator new(p_Bytes);
		}

		inline void heapChunkRelease(void* p_Chunk, void*)
		{
			::operator delete(p_Chunk);
		}
	}

	inline FrameArena::FrameArena(std::byte* p_Buffer, const size_t p_Capacity) noexcept
	{
		assert((p_Buffer == nullptr || reinterpret_cast<uintptr_t>(p_Buffer) % alignof(std::max_align_t) == 0)
			&& "vkp::dyn::FrameArena: the backing buffer has to be suitably aligned");

		m_InitialChunk.begin = p_Buffer;
		m_InitialChunk.end = p_Buffer + p_Capacity;
		m_InitialChunk.capacity = p_Capacity;
		m_Current = p_Buffer;
		m_TotalCapacity = p_Capacity;

		m_Source.allocate = detail::heapChunkAllocate;
		m_Source.release = detail::heapChunkRelease;
	}

	inline FrameArena::~FrameArena()
	{
		releaseChunks();
	}

	inline void FrameArena::setGrowHandler(const GrowHandler p_Handler, void* p_UserData) noexcept
	{
		m_GrowHandler = p_Handler;
		m_GrowUserData = p_UserData;
	}

	inline void FrameArena::acquireChunk(const size_t p_MinBytes)
	{
		Chunk* l_Next = m_CurrentChunk->next;
		if (l_Next == nullptr || l_Next->capacity < p_MinBytes)
		{
			const size_t l_Size = std::max({ p_MinBytes, detail::kMinChunkBytes, m_TotalCapacity });
			const size_t l_Total = sizeof(Chunk) + l_Size + alignof(std::max_align_t);
			void* const l_Memory = m_Source.allocate != nullptr ? m_Source.allocate(l_Total, m_Source.userData) : nullptr;
			if (l_Memory == nullptr)
			{
				throw std::bad_alloc();
			}

			Chunk* const l_Chunk = static_cast<Chunk*>(l_Memory);
			std::byte* const l_Begin = alignUp(reinterpret_cast<std::byte*>(l_Chunk + 1), alignof(std::max_align_t));
			l_Chunk->begin = l_Begin;
			l_Chunk->end = l_Begin + l_Size;
			l_Chunk->capacity = l_Size;
			l_Chunk->next = m_CurrentChunk->next;
			m_CurrentChunk->next = l_Chunk;

			m_TotalCapacity += l_Size;
			++m_GrowCount;
			m_GrowBytes += l_Size;

			if (m_GrowHandler != nullptr)
			{
				m_GrowHandler(p_MinBytes, l_Size, m_GrowUserData);
			}

			l_Next = l_Chunk;
		}

		m_UsedBeforeChunk += m_CurrentChunk->capacity;
		m_CurrentChunk = l_Next;
		m_Current = l_Next->begin;
	}

	inline void FrameArena::releaseChunks() noexcept
	{
		Chunk* l_Chunk = m_InitialChunk.next;
		while (l_Chunk != nullptr)
		{
			Chunk* const l_Next = l_Chunk->next;
			if (m_Source.release != nullptr)
			{
				m_Source.release(l_Chunk, m_Source.userData);
			}
			l_Chunk = l_Next;
		}
		m_InitialChunk.next = nullptr;
	}

	inline void* FrameArena::allocate(const size_t p_Bytes, const size_t p_Alignment)
	{
		if (p_Bytes == 0)
		{
			return m_Current;
		}

		std::byte* l_Aligned = alignUp(m_Current, p_Alignment);
		if (l_Aligned > m_CurrentChunk->end || remaining(l_Aligned) < p_Bytes)
		{
			acquireChunk(p_Bytes + p_Alignment);
			l_Aligned = alignUp(m_Current, p_Alignment);
		}

		m_Current = l_Aligned + p_Bytes;
		const size_t l_Used = used();
		m_HighWater = m_HighWater > l_Used ? m_HighWater : l_Used;
		return l_Aligned;
	}

	inline void FrameArena::deallocate(void* p_Ptr, const size_t p_Bytes) noexcept
	{
		std::byte* const l_Ptr = static_cast<std::byte*>(p_Ptr);
		if (l_Ptr != nullptr && l_Ptr >= m_CurrentChunk->begin && l_Ptr + p_Bytes == m_Current)
		{
			m_Current = l_Ptr;
		}
	}

	inline bool FrameArena::tryGrowInPlace(void* p_Ptr, const size_t p_OldBytes, const size_t p_NewBytes) noexcept
	{
		if (p_NewBytes <= p_OldBytes)
		{
			return true;
		}

		const std::byte* const l_Ptr = static_cast<std::byte*>(p_Ptr);
		if (l_Ptr < m_CurrentChunk->begin || l_Ptr >= m_CurrentChunk->end || l_Ptr + p_OldBytes != m_Current)
		{
			return false;
		}

		const size_t l_Extra = p_NewBytes - p_OldBytes;
		if (remaining(m_Current) < l_Extra)
		{
			return false;
		}

		m_Current += l_Extra;
		const size_t l_Used = used();
		m_HighWater = m_HighWater > l_Used ? m_HighWater : l_Used;
		return true;
	}

	inline void FrameArena::reset() noexcept
	{
		m_CurrentChunk = &m_InitialChunk;
		m_Current = m_InitialChunk.begin;
		m_UsedBeforeChunk = 0;
		m_HighWater = 0;
		m_GrowCount = 0;
		m_GrowBytes = 0;
		++m_Generation;
	}

	inline size_t FrameArena::used() const noexcept
	{
		if (m_Current == nullptr)
		{
			return m_UsedBeforeChunk;
		}
		return m_UsedBeforeChunk + static_cast<size_t>(m_Current - m_CurrentChunk->begin);
	}
}
