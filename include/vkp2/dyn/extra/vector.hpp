#pragma once

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstring>
#include <memory>
#include <type_traits>
#include <utility>

#include "vkp2/concepts.hpp"

namespace vkp::dyn
{
	template<typename Allocator>
	concept GrowableAllocator = requires(Allocator& p_Allocator, void* p_Ptr, const size_t p_OldBytes, const size_t p_NewBytes)
	{
		{ p_Allocator.tryGrowInPlace(p_Ptr, p_OldBytes, p_NewBytes) } -> std::same_as<bool>;
	};

	template<typename Allocator>
	concept GenerationTrackedAllocator = requires(const Allocator& p_Allocator)
	{
		{ p_Allocator.generation() } -> std::convertible_to<uint64_t>;
	};

	template<SequenceElement T, AllocatorFor<T> Allocator = std::allocator<T>>
	class Vector
	{

	public:
		using value_type = T;
		using allocator_type = Allocator;
		using size_type = size_t;
		using reference = T&;
		using const_reference = const T&;
		using pointer = T*;
		using const_pointer = const T*;
		using iterator = T*;
		using const_iterator = const T*;

		Vector() = default;
		explicit Vector(const Allocator& p_Allocator) noexcept : m_Allocator(p_Allocator) {}

		Vector(const Vector& p_Other) : m_Allocator(p_Other.m_Allocator)
		{
			reserve(p_Other.m_Size);
			for (const T& l_Value : p_Other)
			{
				emplace_back(l_Value);
			}
		}

		Vector(Vector&& p_Other) noexcept
			: m_Data(std::exchange(p_Other.m_Data, nullptr))
			, m_Size(std::exchange(p_Other.m_Size, size_t{ 0 }))
			, m_Capacity(std::exchange(p_Other.m_Capacity, size_t{ 0 }))
			, m_Generation(p_Other.m_Generation)
			, m_Allocator(p_Other.m_Allocator)
		{
		}

		Vector& operator=(const Vector& p_Other)
		{
			if (this != &p_Other)
			{
				clear();
				reserve(p_Other.m_Size);
				for (const T& l_Value : p_Other)
				{
					emplace_back(l_Value);
				}
			}
			return *this;
		}

		Vector& operator=(Vector&& p_Other) noexcept
		{
			if (this != &p_Other)
			{
				release();
				m_Data = std::exchange(p_Other.m_Data, nullptr);
				m_Size = std::exchange(p_Other.m_Size, size_t{ 0 });
				m_Capacity = std::exchange(p_Other.m_Capacity, size_t{ 0 });
				m_Allocator = p_Other.m_Allocator;
				m_Generation = p_Other.m_Generation;
			}
			return *this;
		}

		~Vector() { release(); }

		template<typename... Args>
		reference emplace_back(Args&&... p_Args)
		{
			checkLive();
			if (m_Size == m_Capacity)
			{
				grow(m_Size + 1);
			}
			T* l_Slot = m_Data + m_Size;
			::new (static_cast<void*>(l_Slot)) T(std::forward<Args>(p_Args)...);
			++m_Size;
			return *l_Slot;
		}

		void push_back(const T& p_Value) { emplace_back(p_Value); }
		void push_back(T&& p_Value) { emplace_back(std::move(p_Value)); }

		void clear() noexcept
		{
			if constexpr (!std::is_trivially_destructible_v<T>)
			{
				for (size_t i = 0; i < m_Size; ++i)
				{
					m_Data[i].~T();
				}
			}
			m_Size = 0;
		}

		template<typename InputIt>
		void assign(InputIt p_First, InputIt p_Last)
		{
			clear();
			for (auto l_It = p_First; l_It != p_Last; ++l_It)
			{
				emplace_back(*l_It);
			}
		}

		void resize(const size_t p_NewSize, const T& p_Value = T{})
		{
			checkLive();
			reserve(p_NewSize);
			if constexpr (!std::is_trivially_destructible_v<T>)
			{
				for (size_t i = p_NewSize; i < m_Size; ++i)
				{
					m_Data[i].~T();
				}
			}
			for (size_t i = m_Size; i < p_NewSize; ++i)
			{
				::new (static_cast<void*>(m_Data + i)) T(p_Value);
			}
			m_Size = p_NewSize;
		}

		void reserve(const size_t p_Capacity)
		{
			if (p_Capacity > m_Capacity)
			{
				grow(p_Capacity);
			}
		}

		void grow(const size_t p_MinCapacity)
		{
			size_t l_NewCapacity = m_Capacity == 0 ? kMinCapacity : m_Capacity * 2;
			l_NewCapacity = std::max(l_NewCapacity, p_MinCapacity);

			if constexpr (GrowableAllocator<Allocator>)
			{
				if (m_Data != nullptr && m_Allocator.tryGrowInPlace(m_Data, m_Capacity * sizeof(T), l_NewCapacity * sizeof(T)))
				{
					m_Capacity = l_NewCapacity;
					m_Generation = currentGeneration();
					return;
				}
			}

			T* l_NewData = std::allocator_traits<Allocator>::allocate(m_Allocator, l_NewCapacity);
			if (m_Size > 0)
			{
				if constexpr (std::is_trivially_copyable_v<T>)
				{
					std::memcpy(static_cast<void*>(l_NewData), static_cast<const void*>(m_Data), m_Size * sizeof(T));
				}
				else
				{
					for (size_t i = 0; i < m_Size; ++i)
					{
						::new (static_cast<void*>(l_NewData + i)) T(std::move(m_Data[i]));
						m_Data[i].~T();
					}
				}
			}
			if (m_Data != nullptr)
			{
				std::allocator_traits<Allocator>::deallocate(m_Allocator, m_Data, m_Capacity);
			}
			m_Data = l_NewData;
			m_Capacity = l_NewCapacity;
			m_Generation = currentGeneration();
		}

		[[nodiscard]] pointer data() noexcept { checkLive(); return m_Data; }
		[[nodiscard]] const_pointer data() const noexcept { return m_Data; }
		[[nodiscard]] size_t size() const noexcept { return m_Size; }
		[[nodiscard]] bool empty() const noexcept { return m_Size == 0; }
		[[nodiscard]] size_t capacity() const noexcept { return m_Capacity; }
		[[nodiscard]] Allocator& allocator() noexcept { return m_Allocator; }
		[[nodiscard]] const Allocator& allocator() const noexcept { return m_Allocator; }

		reference operator[](const size_t p_Index) noexcept { checkLive(); return m_Data[p_Index]; }
		const_reference operator[](const size_t p_Index) const noexcept { return m_Data[p_Index]; }

		iterator begin() noexcept { checkLive(); return m_Data; }
		iterator end() noexcept { checkLive(); return m_Data + m_Size; }
		const_iterator begin() const noexcept { return m_Data; }
		const_iterator end() const noexcept { return m_Data + m_Size; }

	private:
		[[nodiscard]] uint64_t currentGeneration() const noexcept
		{
			if constexpr (GenerationTrackedAllocator<Allocator>)
			{
				return m_Allocator.generation();
			}
			else
			{
				return 0;
			}
		}

		void checkLive() const noexcept
		{
			if constexpr (GenerationTrackedAllocator<Allocator>)
			{
				assert(m_Data == nullptr || m_Generation == m_Allocator.generation()
					&& "vkp::dyn::Vector: the arena was reset while this container was still alive");
			}
		}

		void release() noexcept
		{
			clear();
			if (m_Data != nullptr)
			{
				std::allocator_traits<Allocator>::deallocate(m_Allocator, m_Data, m_Capacity);
				m_Data = nullptr;
			}
			m_Capacity = 0;
		}

		static constexpr size_t kMinCapacity = 4;

		T* m_Data = nullptr;
		size_t m_Size = 0;
		size_t m_Capacity = 0;
		uint64_t m_Generation = 0;
		[[no_unique_address]] Allocator m_Allocator{};
	};
}
