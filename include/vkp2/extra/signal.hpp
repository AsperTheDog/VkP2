#pragma once

#include <cstdint>
#include <deque>
#include <functional>

template<typename... Args>
class Signal
{
public:
	using Func = std::function<void(Args...)>;
	using Id = uint64_t;

	static constexpr Id kInvalidId = 0;

	Signal() = default;

	Id connect(const Func& p_Func)
	{
		return add(p_Func);
	}

	template <typename T>
	Id connect(T* p_Instance, void (T::* p_Method)(Args...))
	{
		return add([p_Instance, p_Method](Args... p_Args) { (p_Instance->*p_Method)(p_Args...); });
	}

	void disconnect(const Id p_Id)
	{
		if (p_Id == kInvalidId)
		{
			return;
		}
		for (auto l_It = m_Slots.begin(); l_It != m_Slots.end(); ++l_It)
		{
			if (l_It->id != p_Id)
			{
				continue;
			}
			if (m_EmitDepth > 0)
			{
				l_It->dead = true;
			}
			else
			{
				m_Slots.erase(l_It);
			}
			return;
		}
	}

	void emit(Args... p_Args)
	{
		++m_EmitDepth;
		const size_t l_Count = m_Slots.size();
		for (size_t i = 0; i < l_Count && i < m_Slots.size(); ++i)
		{
			if (!m_Slots[i].dead)
			{
				m_Slots[i].func(p_Args...);
			}
		}
		--m_EmitDepth;

		if (m_EmitDepth == 0)
		{
			for (auto l_It = m_Slots.begin(); l_It != m_Slots.end(); )
			{
				l_It = l_It->dead ? m_Slots.erase(l_It) : l_It + 1;
			}
		}
	}

private:
	struct Slot
	{
		Id id = kInvalidId;
		Func func;
		bool dead = false;
	};

	[[nodiscard]] Id add(const Func& p_Func)
	{
		const Id l_Id = m_NextId++;
		m_Slots.push_back(Slot{ .id = l_Id, .func = p_Func });
		return l_Id;
	}

	std::deque<Slot> m_Slots;
	Id m_NextId = 1;
	uint32_t m_EmitDepth = 0;
};
