#pragma once

namespace Elixir
{
    class ELIXIR_API Malloc
    {
      public:
        virtual ~Malloc() = default;

        virtual std::tuple<void*, size_t> Alloc(size_t size, uint32_t alignment = 0) = 0;
        virtual std::tuple<void*, size_t> AllocZeroed(size_t size, uint32_t alignment = 0);
        virtual std::tuple<void*, size_t> Realloc(void* ptr, size_t newSize, uint32_t alignment = 0) = 0;
        virtual void Free(void* ptr) = 0;

        /**
         * If the caller doesn't request a specific alignment (i.e., alignment ≤ 0),
         * apply:
         *
         * Alignment = 16 if size ≥ 16 bytes
         * Alignment = 8  if size < 16 bytes
         *
         * If a non-zero valid alignment is passed, respect it.
         */
        static uint32_t GetAlignment(size_t size, uint32_t alignment);

        /**
         * Pad size to a multiple of alignment.
         */
        static size_t AlignSize(size_t size, uint32_t alignment);
    };

    class ELIXIR_API SystemMalloc final : public Malloc
    {
      public:
        SystemMalloc() = default;
        ~SystemMalloc() override = default;

        std::tuple<void*, size_t> Alloc(size_t size, uint32_t alignment = 0) override;
        std::tuple<void*, size_t> Realloc(void* ptr, size_t newSize, uint32_t alignment = 0) override;
        void Free(void* ptr) override;
    };
}