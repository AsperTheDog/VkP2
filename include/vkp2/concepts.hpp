#pragma once

#include <concepts>
#include <cstddef>
#include <memory>

namespace vkp
{
	template<typename T>
	concept SequenceElement = std::destructible<T> && std::assignable_from<T&, T>;

	template<typename T, typename TIterator>
	concept SequenceAssign = requires(T& p_Sequence, TIterator p_First, TIterator p_Last)
	{
		p_Sequence.assign(p_First, p_Last);
	};

	template<typename T>
	concept Sequence = requires(T& p_Sequence, const T& p_ConstSequence, const size_t p_Index, const typename T::value_type& p_Value)
	{
		typename T::value_type;
		p_Sequence.push_back(p_Value);
		p_Sequence.clear();
		p_Sequence.reserve(p_Index);
		p_Sequence.resize(p_Index, p_Value);
		{ p_ConstSequence.size() } -> std::convertible_to<size_t>;
		{ p_ConstSequence.empty() } -> std::convertible_to<bool>;
		{ p_ConstSequence.data() } -> std::convertible_to<const typename T::value_type*>;
		{ p_Sequence[p_Index] };
		{ p_Sequence.begin() };
		{ p_Sequence.end() };
	} && SequenceAssign<T, const typename T::value_type*>;

	template<typename TAllocator, typename T>
	concept AllocatorFor = requires
	{
		typename TAllocator::value_type;
	}
	&& requires(TAllocator& p_Allocator, const size_t p_Count, T* p_Ptr)
	{
		{ p_Allocator.allocate(p_Count) } -> std::convertible_to<T*>;
		{ p_Allocator.deallocate(p_Ptr, p_Count) };
	}
	&& std::same_as<typename TAllocator::value_type, T>;
}
