#pragma once
#include <cstddef>
#include <memory>
#include <utility>
#include <stdexcept>

template <typename T, size_t N, typename Allocator = std::allocator<T>>
class SmallVector 
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

    explicit SmallVector(const Allocator& p_Alloc = Allocator())
        : m_Data(reinterpret_cast<T*>(m_StackStorage)), m_Size(0), m_Capacity(N), m_Allocator(p_Alloc) {}

    ~SmallVector() 
    {
        clear();
        if (isHeapAllocated()) 
        {
            std::allocator_traits<Allocator>::deallocate(m_Allocator, m_Data, m_Capacity);
        }
    }

    SmallVector(const SmallVector&) = delete;
    SmallVector& operator=(const SmallVector&) = delete;

    SmallVector(SmallVector&& p_Other) noexcept
        : m_Size(p_Other.m_Size), m_Capacity(p_Other.m_Capacity), m_Allocator(std::move(p_Other.m_Allocator)) 
    {
        if (p_Other.isHeapAllocated()) 
        {
            m_Data = p_Other.m_Data;
            p_Other.m_Data = reinterpret_cast<T*>(p_Other.m_StackStorage);
            p_Other.m_Size = 0;
            p_Other.m_Capacity = N;
        }
        else 
        {
            m_Data = reinterpret_cast<T*>(m_StackStorage);
            for (size_t i = 0; i < m_Size; ++i) 
            {
                new (&m_Data[i]) T(std::move(p_Other.m_Data[i]));
                p_Other.m_Data[i].~T();
            }
            p_Other.m_Size = 0;
        }
    }

    SmallVector& operator=(SmallVector&& p_Other) noexcept 
    {
        if (this == &p_Other) return *this;

        clear();
        if (isHeapAllocated()) 
        {
            std::allocator_traits<Allocator>::deallocate(m_Allocator, m_Data, m_Capacity);
        }

        m_Allocator = std::move(p_Other.m_Allocator);
        m_Size = p_Other.m_Size;
        m_Capacity = p_Other.m_Capacity;

        if (p_Other.isHeapAllocated()) 
        {
            m_Data = p_Other.m_Data;

            p_Other.m_Data = reinterpret_cast<T*>(p_Other.m_StackStorage);
            p_Other.m_Size = 0;
            p_Other.m_Capacity = N;
        }
        else 
        {
            m_Data = reinterpret_cast<T*>(m_StackStorage);
            for (size_t i = 0; i < m_Size; ++i) 
            {
                new (&m_Data[i]) T(std::move(p_Other.m_Data[i]));
                p_Other.m_Data[i].~T();
            }
            p_Other.m_Size = 0;
        }

        return *this;
    }

    void reserve(size_t p_NewCapacity) 
    {
        if (p_NewCapacity <= m_Capacity) return;

        T* l_NewData = std::allocator_traits<Allocator>::allocate(m_Allocator, p_NewCapacity);

        for (size_t i = 0; i < m_Size; ++i) 
        {
            new (&l_NewData[i]) T(std::move(m_Data[i]));
            m_Data[i].~T();
        }

        if (isHeapAllocated()) 
        {
            std::allocator_traits<Allocator>::deallocate(m_Allocator, m_Data, m_Capacity);
        }

        m_Data = l_NewData;
        m_Capacity = p_NewCapacity;
    }

    template <typename... Args>
    reference emplace_back(Args&&... p_Args) 
    {
        if (m_Size == m_Capacity) 
        {
            reserve(m_Capacity == 0 ? 1 : m_Capacity * 2);
        }
        T* l_Ptr = &m_Data[m_Size];
        new (l_Ptr) T(std::forward<Args>(p_Args)...);
        ++m_Size;
        return *l_Ptr;
    }

    void push_back(const T& p_Value) 
    {
        emplace_back(p_Value);
    }

    void push_back(T&& p_Value) 
    {
        emplace_back(std::move(p_Value));
    }

    void pop_back() 
    {
        if (m_Size > 0) 
        {
            --m_Size;
            m_Data[m_Size].~T();
        }
    }

    void clear() 
    {
        for (size_t i = 0; i < m_Size; ++i) 
        {
            m_Data[i].~T();
        }
        m_Size = 0;
    }

    template <typename InputIt>
    void assign(InputIt p_First, InputIt p_Last) 
    {
        clear();

        const size_t l_Count = static_cast<size_t>(std::distance(p_First, p_Last));

        reserve(l_Count);

        for (auto l_It = p_First; l_It != p_Last; ++l_It) 
        {
            emplace_back(*l_It);
        }
    }

    void resize(const size_t p_NewSize, const T& p_Value = T{}) 
    {
        if (p_NewSize < m_Size) 
        {
            for (size_t i = p_NewSize; i < m_Size; ++i) 
            {
                m_Data[i].~T();
            }
        }
        else if (p_NewSize > m_Size) 
        {
            reserve(p_NewSize);
            for (size_t i = m_Size; i < p_NewSize; ++i) 
            {
                new (&m_Data[i]) T(p_Value);
            }
        }
        m_Size = p_NewSize;
    }

    [[nodiscard]] pointer data() { return m_Data; }
    [[nodiscard]] const_pointer data() const { return m_Data; }
    [[nodiscard]] size_t size() const { return m_Size; }
    [[nodiscard]] size_t capacity() const { return m_Capacity; }
    [[nodiscard]] bool empty() const { return m_Size == 0; }

    reference operator[](size_t p_Index) { return m_Data[p_Index]; }
    const_reference operator[](size_t p_Index) const { return m_Data[p_Index]; }

    iterator begin() { return m_Data; }
    iterator end() { return m_Data + m_Size; }
    const_iterator begin() const { return m_Data; }
    const_iterator end() const { return m_Data + m_Size; }

private:
    [[nodiscard]] bool isHeapAllocated() const 
    {
        return m_Data != reinterpret_cast<const T*>(m_StackStorage);
    }

    alignas(T) std::byte m_StackStorage[N * sizeof(T)]{};
    T* m_Data;
    size_t m_Size;
    size_t m_Capacity;
    [[no_unique_address]] Allocator m_Allocator;
};