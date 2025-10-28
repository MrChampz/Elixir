#pragma once

#include <gmock/gmock.h>

#include <Engine/Core/Malloc.h>

class MockMalloc final : public Malloc
{
  public:
    MockMalloc() = default;
    ~MockMalloc() override = default;

    MOCK_METHOD((std::tuple<Byte*, size_t>), Alloc, (size_t, uint32_t), (override));
    MOCK_METHOD((std::tuple<Byte*, size_t>), AllocZeroed, (size_t, uint32_t), (override));
    MOCK_METHOD((std::tuple<Byte*, size_t>), Realloc, (Byte*, size_t, uint32_t), (override));
    MOCK_METHOD(void, Free, (Byte*), (override));
};