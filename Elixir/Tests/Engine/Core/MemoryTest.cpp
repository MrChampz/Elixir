#include "MockMalloc.h"

#include <gtest/gtest.h>
using namespace testing;

#include <Engine/Core/Memory.h>
using namespace Elixir;

class MemoryTest : public Test
{
  protected:
    void SetUp() override
    {
        Memory::s_Malloc = Scope<Malloc>(&MockedMalloc);
    }

    void TearDown() override
    {
        Memory::s_Malloc.reset();
    }

    MockMalloc MockedMalloc;
};

TEST_F(MemoryTest, AllocDelegatesToMalloc)
{
    EXPECT_CALL(MockedMalloc, Alloc(128, 16))
        .Times(1)
        .WillOnce(Return(std::make_tuple(nullptr, 128)));

    auto [_, size] = Memory::Alloc(128, 16);
    EXPECT_EQ(128u, size);
}

TEST_F(MemoryTest, AllocZeroedDelegatesToMalloc)
{
    EXPECT_CALL(MockedMalloc, AllocZeroed(128, 16))
        .Times(1)
        .WillOnce(Return(std::make_tuple(nullptr, 128)));

    auto [_, size] = Memory::AllocZeroed(128, 16);
    EXPECT_EQ(128u, size);
}

TEST_F(MemoryTest, ReallocDelegatesToMalloc)
{
    EXPECT_CALL(MockedMalloc, Realloc(nullptr, 128, 16))
        .Times(1)
        .WillOnce(Return(std::make_tuple(nullptr, 128)));

    auto [x, size] = Memory::Realloc(nullptr, 128, 16);
    EXPECT_EQ(128u, size);
}

TEST_F(MemoryTest, FreeDelegatesToMalloc)
{
    EXPECT_CALL(MockedMalloc, Free(nullptr))
        .Times(1)
        .WillOnce(Return());

    Memory::Free(nullptr);
}

TEST_F(MemoryTest, MemcpyWorksAsExpected)
{
    constexpr char src[4] = { 'a', 'b', 'c', 'd' };
    char dst[4] = {};
    Memory::Memcpy(dst, src, 4);
    EXPECT_EQ(std::memcmp(src, dst, 4), 0);
}

TEST_F(MemoryTest, MemsetFillsBufferCorrectly)
{
    uint8_t buffer[16];

    Memory::Memset(buffer, 0xAB, sizeof(buffer));

    for (size_t i = 0; i < sizeof(buffer); ++i)
        EXPECT_EQ(buffer[i], 0xAB) << "Byte " << i << " was not set correctly.";
}

TEST_F(MemoryTest, MemzeroSetsAllBytesToZero)
{
    char data[16] = { 'x', 'x', 'x', 'x' };
    Memory::Memzero(data, 16);
    for (int i = 0; i < 16; ++i)
        EXPECT_EQ(data[i], 0);
}