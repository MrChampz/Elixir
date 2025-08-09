#pragma once

namespace Elixir
{
    template <typename TIndexType>
    class ELIXIR_API Handle
    {
      public:
        static const Handle NULLs;

        Handle() = default;

        explicit Handle(TIndexType index)
        {
            check(index <= NULL_INDEX);
            m_Index = index;
        }

        bool IsNull() const { return m_Index == NULL_INDEX; }
        bool IsValid() const { return m_Index != NULL_INDEX; }

        TIndexType GetIndex() const { check(IsValid()); return m_Index; }
        TIndexType GetIndexUnchecked() const { return m_Index; }

        bool operator==(Handle other) const { return m_Index == other.m_Index; }
        bool operator!=(Handle other) const { return m_Index != other.m_Index; }
        bool operator<=(Handle other) const { check(IsValid() && other.IsValid()); return m_Index <= other.m_Index; }
        bool operator>=(Handle other) const { check(IsValid() && other.IsValid()); return m_Index >= other.m_Index; }
        bool operator<(Handle other) const { check(IsValid() && other.IsValid()); return m_Index < other.m_Index; }
        bool operator>(Handle other) const { check(IsValid() && other.IsValid()); return m_Index > other.m_Index; }

        Handle& operator+=(TIndexType increment)
        {
            check(m_Index + increment <= NULL_INDEX);
            m_Index += increment;
            return *this;
        }

        Handle& operator-=(TIndexType decrement)
        {
            check((m_Index - decrement) > 0);
            m_Index -= decrement;
            return *this;
        }

        Handle& operator+(TIndexType add)
        {
            *this += add;
            return *this;
        }

        Handle& operator-(TIndexType subtract)
        {
            *this -= subtract;
            return *this;
        }

        Handle& operator++()
        {
            check(IsValid());
            ++m_Index;
            return *this;
        }

        Handle& operator--()
        {
            check(IsValid());
            --m_Index;
            return *this;
        }

        static Handle Min(Handle a, Handle b)
        {
            return a.m_Index < b.m_Index ? a : b;
        }

        static Handle Max(Handle a, Handle b)
        {
            // Avoid 0 > 0 comparison, adding 1 for both sides.
            return (TIndexType)(a.m_Index + 1) > (TIndexType)(b.m_Index + 1) ? a : b;
        }

      private:
        static const TIndexType NULL_INDEX = std::numeric_limits<TIndexType>::max();

        TIndexType m_Index = NULL_INDEX;
    };
}

template <typename TIndexType>
struct std::hash<Handle<TIndexType>>
{
    size_t operator()(const Handle<TIndexType>& handle) const noexcept
    {
        return std::hash<TIndexType>()(handle.GetIndex());
    }
};