#pragma once

#include <gmock/gmock.h>

#include <Engine/Core/Malloc.h>

class MockMalloc final : public Malloc
{
  public:
    MockMalloc() = default;
    ~MockMalloc() override = default;

    MOCK_METHOD((std::tuple<void*, size_t>), Alloc, (size_t, uint32_t), (override));
    MOCK_METHOD((std::tuple<void*, size_t>), AllocZeroed, (size_t, uint32_t), (override));
    MOCK_METHOD((std::tuple<void*, size_t>), Realloc, (void*, size_t, uint32_t), (override));
    MOCK_METHOD(void, Free, (void*), (override));
};