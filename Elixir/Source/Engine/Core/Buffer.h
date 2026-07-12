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

        SBuffer(void* data, const size_t size) : SBuffer((Byte*)data, size) {}
        SBuffer(const void* data, const size_t size) : SBuffer((const Byte*)data, size) {}

        SBuffer(Byte* data, const size_t size) : Data(data), Size(size)
        {
            EE_PROFILE_ZONE_SCOPED()
        }

        SBuffer(const Byte* data, const size_t size) : SBuffer(size)
        {
            data ? Copy(data, size) : Allocate(size);
        }

        SBuffer(const SBuffer& other) : SBuffer()
        {
            if (other.Data)
                Copy(other.Data, other.Size);
            else
                Allocate(other.Size);
        }

        SBuffer(SBuffer&& other) noexcept
            : Data(std::exchange(other.Data, nullptr)), Size(std::exchange(other.Size, 0))
        {
        }

        SBuffer& operator=(const SBuffer& other)
        {
            if (this == &other)
                return *this;

            if (other.Data)
                Copy(other.Data, other.Size);
            else
                Allocate(other.Size);
            return *this;
        }

        SBuffer& operator=(SBuffer&& other) noexcept
        {
            if (this == &other)
                return *this;

            FreeInternalData();
            Data = std::exchange(other.Data, nullptr);
            Size = std::exchange(other.Size, 0);
            return *this;
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
            Data = ptr;
            Size = allocatedSize;
        }

        void Copy(const Byte* data, const size_t size)
        {
            EE_PROFILE_ZONE_SCOPED()

            if (size == 0)
            {
                FreeInternalData();
                return;
            }

            EE_CORE_ASSERT(data, "Cannot copy from null data!")

            Allocate(size);
            Memory::Memcpy(Data, data, size);
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

        explicit operator bool() const
        {
            return Data != nullptr && Size > 0;
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

            Size = 0;
        }
    };
}
