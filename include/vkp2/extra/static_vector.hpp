#pragma once

#include <array>

#include "concepts.hpp"
#include <stdexcept>
#include <utility>

namespace vkp
{
	template<SequenceElement T, size_t N>
	class StaticVector
	{
	public:
		using value_type = T;
		using size_type = size_t;
		using reference = T&;
		using const_reference = const T&;
		using pointer = T*;
		using const_pointer = const T*;
		using iterator = T*;
		using const_iterator = const T*;

		StaticVector() = default;

		template<typename... Args>
		reference emplace_back(Args&&... p_Args)
		{
			if (m_Size == N)
			{
				throw std::runtime_error("vkp: StaticVector capacity exceeded");
			}
			m_Data[m_Size] = T(std::forward<Args>(p_Args)...);
			return m_Data[m_Size++];
		}

		void push_back(const T& p_Value) { emplace_back(p_Value); }
		void push_back(T&& p_Value) { emplace_back(std::move(p_Value)); }

		void clear() { m_Size = 0; }

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
			if (p_NewSize > N)
			{
				throw std::runtime_error("vkp: StaticVector capacity exceeded");
			}
			while (m_Size < p_NewSize)
			{
				m_Data[m_Size++] = p_Value;
			}
			m_Size = p_NewSize;
		}

		void reserve(const size_t p_Capacity)
		{
			if (p_Capacity > N)
			{
				throw std::runtime_error("vkp: StaticVector capacity exceeded");
			}
		}

		[[nodiscard]] pointer data() { return m_Data.data(); }
		[[nodiscard]] const_pointer data() const { return m_Data.data(); }
		[[nodiscard]] size_t size() const { return m_Size; }
		[[nodiscard]] bool empty() const { return m_Size == 0; }
		[[nodiscard]] static constexpr size_t capacity() { return N; }

		reference operator[](const size_t p_Index) { return m_Data[p_Index]; }
		const_reference operator[](const size_t p_Index) const { return m_Data[p_Index]; }

		iterator begin() { return m_Data.data(); }
		iterator end() { return m_Data.data() + m_Size; }
		const_iterator begin() const { return m_Data.data(); }
		const_iterator end() const { return m_Data.data() + m_Size; }

	private:
		std::array<T, N> m_Data;
		size_t m_Size = 0;
	};
}
