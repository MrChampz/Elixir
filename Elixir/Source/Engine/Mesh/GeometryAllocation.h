#pragma once

#include <Engine/Core/Handle.h>

namespace Elixir
{
    class GeometryPool;
    struct SGeometry;

    /**
     * @brief Owns one geometry range allocated from a GeometryPool.
     *
     * Destroying or resetting this object returns its range to the pool. The pool
     * must outlive every allocation created from it.
     */
    class ELIXIR_API GeometryAllocation final
    {
        friend class GeometryPool;

    public:
        /** @brief Create an empty allocation. */
        GeometryAllocation() = default;

        /** @brief Return the owned geometry range to its pool. */
        ~GeometryAllocation();

        GeometryAllocation(const GeometryAllocation&) = delete;
        GeometryAllocation& operator=(const GeometryAllocation&) = delete;

        /** @brief Transfer ownership from another allocation. */
        GeometryAllocation(GeometryAllocation&&) noexcept;

        /** @brief Release the current range and take ownership from another allocation. */
        GeometryAllocation& operator=(GeometryAllocation&&) noexcept;

        /** @brief Return the geometry range to the pool and make this allocation empty. */
        void Reset();

        /**
         * @brief Check whether this allocation still identifies live pool geometry.
         * @return True when the pool recognizes the allocation handle.
         */
        bool IsValid() const;

        /** @brief Get metadata for the owned geometry range, when it remains valid. */
        const SGeometry* Get() const;

        /** @brief Get the handle owned by this allocation. */
        SHandle<SGeometry> GetHandle() const { return m_Handle; }

    private:
        GeometryAllocation(GeometryPool* pool, SHandle<SGeometry> handle);

        GeometryPool* m_Pool = nullptr;
        SHandle<SGeometry> m_Handle;
    };
}
