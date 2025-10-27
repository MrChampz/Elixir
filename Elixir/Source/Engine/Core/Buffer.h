#pragma once

#include <Engine/Core/Core.h>
#include <Engine/Core/Memory.h>
#include <Engine/Instrumentation/Profiler.h>

namespace Elixir
{
    struct SBuffer
    {
        Byte* Data;
        size_t Size;

        SBuffer() : SBuffer((Byte*)nullptr, 0) {}
        explicit SBuffer(const size_t size) : SBuffer((Byte*)nullptr, size) {}

        // SBuffer(void* data, const size_t size) : SBuffer((std::byte*)data, size) {}
        // SBuffer(const void* data, const size_t size) : SBuffer((const std::byte*)data, size) {}

        SBuffer(Byte* data, const size_t size) : Data(data), Size(size)
        {
            EE_PROFILE_ZONE_SCOPED()
        }

        SBuffer(const Byte* data, const size_t size) : SBuffer(size)
        {
            Copy(data, size);
        }

        ~SBuffer()
        {
            EE_PROFILE_ZONE_SCOPED()
            FreeInternalData();
        }

        void Allocate(const size_t size)
        {
            EE_PROFILE_ZONE_SCOPED()

            FreeInternalData();

            if (size == 0) return;

            auto [ptr, allocatedSize] = Memory::Alloc(size);
            Data = (Byte*)ptr;
            Size = allocatedSize;
        }

        void Copy(const Byte* data, const size_t size)
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

        void Write(const Byte* data, const size_t size, const size_t offset = 0) const
        {
            EE_PROFILE_ZONE_SCOPED()
            EE_CORE_ASSERT(offset + size <= Size, "Buffer overflow!")
            Memory::Memcpy(Data + offset, data, size);
        }

        template <typename T>
        T* As() { return static_cast<T*>(Data); }

        static SBuffer FromData(const Byte* data, const size_t size)
        {
            SBuffer buffer;
            buffer.Copy(data, size);
            return buffer;
        }

    private:
        void FreeInternalData()
        {
            EE_PROFILE_ZONE_SCOPED()
            if (Data)
            {
                Memory::Free(Data);
                Data = nullptr;
            }
        }
    };
}