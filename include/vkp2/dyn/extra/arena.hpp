#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>

namespace vkp::dyn
{
	class FrameArena;

	template<typename T>
	class ArenaAllocator
	{
		template<typename>
		friend class ArenaAllocator;

	public:
		using value_type = T;
		using propagate_on_container_copy_assignment = std::false_type;
		using propagate_on_container_move_assignment = std::true_type;
		using propagate_on_container_swap = std::false_type;
		using is_always_equal = std::false_type;

		ArenaAllocator() = default;

		explicit ArenaAllocator(FrameArena& p_Arena) noexcept : m_Arena(&p_Arena) {}

		template<typename U>
		explicit ArenaAllocator(const ArenaAllocator<U>& p_Other) noexcept : m_Arena(p_Other.m_Arena) {}

		[[nodiscard]] T* allocate(const size_t p_Count)
		{
			return static_cast<T*>(m_Arena->allocate(p_Count * sizeof(T), alignof(T)));
		}

		void deallocate(T* p_Ptr, const size_t p_Count) noexcept
		{
			m_Arena->deallocate(p_Ptr, p_Count * sizeof(T));
		}

		[[nodiscard]] bool tryGrowInPlace(void* p_Ptr, const size_t p_OldBytes, const size_t p_NewBytes) noexcept
		{
			return m_Arena->tryGrowInPlace(p_Ptr, p_OldBytes, p_NewBytes);
		}

		[[nodiscard]] FrameArena& arena() const noexcept { return *m_Arena; }

		[[nodiscard]] uint64_t generation() const noexcept;

		friend bool operator==(const ArenaAllocator&, const ArenaAllocator&) = default;

	private:
		FrameArena* m_Arena = nullptr;
	};

	class FrameArena
	{
	public:
		using Allocator = ArenaAllocator<void>;
		using GrowHandler = void (*)(size_t p_RequestedBytes, size_t p_ChunkBytes, void* p_UserData);

		struct ChunkSource
		{
			void* (*allocate)(size_t p_Bytes, void* p_UserData) = nullptr;
			void (*release)(void* p_Chunk, void* p_UserData) = nullptr;
			void* userData = nullptr;
		};

		FrameArena(std::byte* p_Buffer, size_t p_Capacity) noexcept;
		FrameArena() noexcept = default;
		~FrameArena();

		FrameArena(const FrameArena&) = delete;
		FrameArena& operator=(const FrameArena&) = delete;

		void setChunkSource(const ChunkSource& p_Source) noexcept { m_Source = p_Source; }
		void setGrowHandler(GrowHandler p_Handler, void* p_UserData = nullptr) noexcept;

		[[nodiscard]] void* allocate(size_t p_Bytes, size_t p_Alignment = alignof(std::max_align_t));
		void deallocate(void* p_Ptr, size_t p_Bytes) noexcept;

		[[nodiscard]] bool tryGrowInPlace(void* p_Ptr, size_t p_OldBytes, size_t p_NewBytes) noexcept;

		void reset() noexcept;

		template<typename T>
		[[nodiscard]] ArenaAllocator<T> allocator() noexcept { return ArenaAllocator<T>(*this); }

		[[nodiscard]] size_t used() const noexcept;
		[[nodiscard]] size_t capacity() const noexcept { return m_TotalCapacity; }
		[[nodiscard]] size_t budget() const noexcept { return m_InitialChunk.capacity; }
		[[nodiscard]] size_t highWater() const noexcept { return m_HighWater; }
		[[nodiscard]] uint32_t growCount() const noexcept { return m_GrowCount; }
		[[nodiscard]] size_t growBytes() const noexcept { return m_GrowBytes; }
		[[nodiscard]] uint64_t generation() const noexcept { return m_Generation; }

	private:
		struct Chunk
		{
			Chunk* next = nullptr;
			std::byte* begin = nullptr;
			std::byte* end = nullptr;
			size_t capacity = 0;
		};

		void acquireChunk(size_t p_MinBytes);
		void releaseChunks() noexcept;

		[[nodiscard]] static std::byte* alignUp(std::byte* p_Ptr, const size_t p_Alignment) noexcept
		{
			const uintptr_t l_Mask = p_Alignment - 1;
			return reinterpret_cast<std::byte*>((reinterpret_cast<uintptr_t>(p_Ptr) + l_Mask) & ~l_Mask);
		}

		[[nodiscard]] size_t remaining(const std::byte* p_From) const noexcept
		{
			return p_From >= m_CurrentChunk->end ? 0 : static_cast<size_t>(m_CurrentChunk->end - p_From);
		}

		Chunk m_InitialChunk;
		Chunk* m_CurrentChunk = &m_InitialChunk;
		std::byte* m_Current = nullptr;

		size_t m_UsedBeforeChunk = 0;
		size_t m_TotalCapacity = 0;
		size_t m_HighWater = 0;
		size_t m_GrowBytes = 0;
		uint32_t m_GrowCount = 0;
		uint64_t m_Generation = 0;

		ChunkSource m_Source;
		GrowHandler m_GrowHandler = nullptr;
		void* m_GrowUserData = nullptr;
	};

	template<typename T>
	uint64_t ArenaAllocator<T>::generation() const noexcept
	{
		return m_Arena->generation();
	}
}

#include "../inline/arena.inl"
