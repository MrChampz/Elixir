#include "epch.h"
#include "GeometryAllocation.h"

#include <Engine/Mesh/GeometryPool.h>

namespace Elixir
{
    GeometryAllocation::~GeometryAllocation()
    {
        Reset();
    }

    GeometryAllocation::GeometryAllocation(GeometryAllocation&& other) noexcept
      : m_Pool(other.m_Pool),
        m_Handle(other.m_Handle)
    {
        other.m_Pool = nullptr;
        other.m_Handle = {};
    }

    GeometryAllocation& GeometryAllocation::operator=(GeometryAllocation&& other) noexcept
    {
        if (this == &other)
            return *this;

        Reset();

        m_Pool = other.m_Pool;
        m_Handle = other.m_Handle;

        other.m_Pool = nullptr;
        other.m_Handle = {};

        return *this;
    }

    void GeometryAllocation::Reset()
    {
        if (m_Pool && m_Handle.IsValid())
            m_Pool->Free(m_Handle);

        m_Pool = nullptr;
        m_Handle = {};
    }

    bool GeometryAllocation::IsValid() const
    {
        return m_Pool && m_Pool->IsValid(m_Handle);
    }

    const SGeometry* GeometryAllocation::Get() const
    {
        return m_Pool ? m_Pool->Get(m_Handle) : nullptr;
    }

    GeometryAllocation::GeometryAllocation(GeometryPool* pool, const SHandle<SGeometry> handle)
      : m_Pool(pool),
        m_Handle(handle) {}
}
