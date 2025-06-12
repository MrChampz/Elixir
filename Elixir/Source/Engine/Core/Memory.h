#pragma once

#include <Engine/Core/Core.h>
#include <Engine/Core/Malloc.h>
#include <Engine/Instrumentation/Profiler.h>

#include <cstring>

namespace Elixir
{
    class ELIXIR_API Memory
    {
      public:
        /**
         * Default memory alignment, as not specified (zero), is resolved by Malloc.
         */
        constexpr static uint32_t DefaultAlignment = 0;

        /**
         * Minimum allocator alignment.
         */
        constexpr static uint32_t MinAlignment = 8;

        /**
         * Max allocator alignment, defined by platform.
         */
        constexpr static uint32_t MaxAlignment = alignof(max_align_t);

        /**
         * Try to allocate memory, asserts if valid result.
         * @returns a tuple containing the ptr and the size of allocated memory.
         */
        static std::tuple<void*, size_t> Alloc(const size_t size, const uint32_t alignment = DefaultAlignment)
        {
            auto [ptr, allocatedSize] = s_Malloc->Alloc(size, alignment);
            EE_PROFILE_MEMORY_ALLOC(ptr, allocatedSize)
            return { ptr, allocatedSize };
        }

        /**
         * Try to allocate zeroed memory, asserts if valid result.
         * @returns a tuple containing the ptr and the size of allocated memory.
         */
        static std::tuple<void*, size_t> AllocZeroed(const size_t size, const uint32_t alignment = DefaultAlignment)
        {
            auto [ptr, allocatedSize] = s_Malloc->AllocZeroed(size, alignment);
            EE_PROFILE_MEMORY_ALLOC(ptr, allocatedSize)
            return { ptr, allocatedSize };
        }

        /**
         * Try to re-allocate memory, asserts if valid result.
         * If pointer is invalid, allocate new memory.
         * The final allocated size will be padded to alignment if unaligned size is provided.
         * @returns a tuple containing the ptr and the size of re-allocated memory.
         */
        static std::tuple<void*, size_t> Realloc(void* ptr, const size_t newSize, const  uint32_t alignment = DefaultAlignment)
        {
            EE_PROFILE_MEMORY_FREE(ptr);
            auto [newPtr, allocatedSize] = s_Malloc->Realloc(ptr, newSize, alignment);
            EE_PROFILE_MEMORY_ALLOC(newPtr, allocatedSize)
            return { newPtr, allocatedSize };
        }

        /**
         * Free allocated memory.
         */
        static void Free(void* ptr)
        {
            EE_PROFILE_MEMORY_FREE(ptr)
            s_Malloc->Free(ptr);
        }

        static void Memcpy(void* dst, const void* src, const size_t size)
        {
            EE_PROFILE_ZONE_SCOPED()
            memcpy(dst, src, size);
        }

        static void Memset(void* dst, const uint8_t value, const size_t size)
        {
            EE_PROFILE_ZONE_SCOPED()
            memset(dst, (int)value, size);
        }

        static void Memzero(void* dst, const size_t size)
        {
            EE_PROFILE_ZONE_SCOPED()
            Memset(dst, 0, size);
        }

        static Scope<Malloc> s_Malloc;
    };
}