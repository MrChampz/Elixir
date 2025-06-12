#include "epch.h"
#include "Malloc.h"

#include "spdlog/fmt/bundled/format.h"

#if defined(__APPLE__)
#include <malloc/malloc.h>
#elif defined(__linux__)
#include <malloc.h>
#endif

namespace Elixir
{
    /* Malloc */

    std::tuple<void*, size_t> Malloc::AllocZeroed(const size_t size, const uint32_t alignment)
    {
        EE_PROFILE_ZONE_SCOPED()
        auto [ptr, allocatedSize] = Alloc(size, alignment);
        Memory::Memzero(ptr, allocatedSize);
        return {ptr, allocatedSize};
    }

    uint32_t Malloc::GetAlignment(const size_t size, const uint32_t alignment)
    {
        if (alignment != 0 && (alignment & (alignment - 1)) == 0)
        {
            return alignment;
        }

        return (size >= 16) ? 16 : 8;
    }

    size_t Malloc::AlignSize(const size_t size, const uint32_t alignment)
    {
        return (size + (alignment - 1)) & ~(alignment - 1);
    }

    /* SystemMalloc */

    std::tuple<void*, size_t> SystemMalloc::Alloc(const size_t size, const uint32_t alignment)
    {
        EE_PROFILE_ZONE_SCOPED()

        const uint32_t memoryAlignment = GetAlignment(size, alignment);
        const size_t alignedSize = AlignSize(size, memoryAlignment);

        void* ptr;
        // TODO: Abstract away platform specific functions (PlatformMemory)
#if defined(_MSC_VER)
        ptr = _aligned_malloc(alignedSize, memoryAlignment);
#else
        ptr = aligned_alloc(memoryAlignment, alignedSize);
#endif

        EE_CORE_ASSERT(ptr != NULL, "Memory allocation failed!")

        return { ptr, alignedSize };
    }

    std::tuple<void*, size_t> SystemMalloc::Realloc(
        void* ptr,
        const size_t newSize,
        const uint32_t alignment
    )
    {
        EE_PROFILE_ZONE_SCOPED()

        if (newSize <= 0)
        {
            Free(ptr);
            return { nullptr, 0 };
        }

        if (!ptr)
        {
            return Alloc(newSize, alignment);
        }

        if (alignment <= Memory::MaxAlignment)
        {
            void* newPtr = realloc(ptr, newSize);
            EE_CORE_ASSERT(newPtr != NULL, "Memory re-allocation failed!")
            return { newPtr, newSize };
        }

#if defined(_MSC_VER)
        void* newPtr = _aligned_realloc(ptr, newSize, alignment);
        EE_CORE_ASSERT(newPtr != NULL, "Memory re-allocation failed!")
        return { newPtr, newSize };
#endif
        auto [newPtr, allocatedSize] = Alloc(newSize, alignment);
        EE_CORE_ASSERT(newPtr != NULL, "Memory re-allocation failed!")

        size_t usableSize = allocatedSize;
#if defined(__APPLE__)
        usableSize = malloc_size(ptr);
#else
        usableSize = malloc_usable_size(ptr);
#endif

        Memory::Memcpy(newPtr, ptr, std::min(newSize, usableSize));
        free(ptr);

        return { newPtr, allocatedSize };
    }

    void SystemMalloc::Free(void* ptr)
    {
        EE_PROFILE_ZONE_SCOPED()
#if defined(_MSC_VER)
        _aligned_free(ptr);
#else
        free(ptr);
#endif
    }
}