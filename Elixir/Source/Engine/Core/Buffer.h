#pragma once

#include <Engine/Core/Core.h>
#include <Engine/Instrumentation/Profiler.h>

namespace Elixir
{
    struct SBuffer
    {
        void* Data;
        size_t Size;

        SBuffer() : SBuffer(nullptr, 0) {}
        SBuffer(void* data, const size_t size) : Data(data), Size(size)
        {
            EE_PROFILE_ZONE_SCOPED()
        }

        ~SBuffer()
        {
            EE_PROFILE_ZONE_SCOPED()
            delete[] Data;
            //Memory::Free(Data);
        }

        void Allocate(const size_t size)
        {
            EE_PROFILE_ZONE_SCOPED()
            delete[] Data;
            Data = nullptr;
            // if (Data)
            // {
            //     Memory::Free(Data);
            //     Data = nullptr;
            // }

            if (size == 0) return;

            Data = new Byte[size];
            Size = size;
            // auto [ptr, allocatedSize] = Memory::Alloc(size);
            // Data = ptr;
            // Size = allocatedSize;
        }

        void Copy(const void* data, const size_t size)
        {
            EE_PROFILE_ZONE_SCOPED()
            Allocate(size);
            if (data) Memory::Memcpy(Data, data, size);
        }

        void ZeroInitialize() const
        {
            EE_PROFILE_ZONE_SCOPED()
            if (Data) Memory::Memzero(Data, Size);
        }

        void Write(const void* data, const size_t size, const size_t offset = 0) const
        {
            EE_PROFILE_ZONE_SCOPED()
            EE_CORE_ASSERT(offset + size <= Size, "Buffer overflow!")
            //Memory::Memcpy(Data + offset, data, size);
        }

        template <typename T>
        T* As() { return static_cast<T*>(Data); }

        static SBuffer FromData(const void* data, const size_t size)
        {
            SBuffer buffer;
            buffer.Copy(data, size);
            return buffer;
        }
    };
}