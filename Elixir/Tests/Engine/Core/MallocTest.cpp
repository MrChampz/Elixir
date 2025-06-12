#include <gtest/gtest.h>

#include <Engine/Core/Malloc.h>
using namespace Elixir;

TEST(MallocTest, GetAlignmentReturnsExpectedValues)
{
    EXPECT_EQ(Malloc::GetAlignment(10, 0),  8u);   // size < 16
    EXPECT_EQ(Malloc::GetAlignment(32, 0),  16u);  // size ≥ 16
    EXPECT_EQ(Malloc::GetAlignment(64, 64), 64u);  // valid power-of-two alignment
    EXPECT_EQ(Malloc::GetAlignment(64, 15), 16u);  // invalid (not power-of-two), fallback
}

TEST(MallocTest, AlignSizePadsCorrectly)
{
    EXPECT_EQ(Malloc::AlignSize(10, 8),  16u);
    EXPECT_EQ(Malloc::AlignSize(64, 16), 64u);
    EXPECT_EQ(Malloc::AlignSize(65, 16), 80u);
}

class SystemMallocTest : public ::testing::Test
{
  protected:
    SystemMalloc Malloc;
};

TEST_F(SystemMallocTest, AllocReturnsNonNull)
{
    auto [ptr, size] = Malloc.Alloc(64);

    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(size, 64u);

    Malloc.Free(ptr);
}

TEST_F(SystemMallocTest, AllocReturnsAlignedPointer)
{
    constexpr uint32_t alignment = 32;
    auto [ptr, allocatedSize] = Malloc.Alloc(128, alignment);

    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(allocatedSize, 128u);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % alignment, 0u);

    Malloc.Free(ptr);
}

TEST_F(SystemMallocTest, AllocDefaultsToExpectedAlignment)
{
    auto [ptr1, allocatedSize1] = Malloc.Alloc(10, 0);
    ASSERT_NE(ptr1, nullptr);
    EXPECT_EQ(allocatedSize1, 16); // size is padded to multiple of alignment (8)
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr1) % 8, 0u);  // 8 is default for < 16

    auto [ptr2, allocatedSize2] = Malloc.Alloc(20, 0);
    ASSERT_NE(ptr2, nullptr);
    EXPECT_EQ(allocatedSize2, 32); // size is padded to multiple of alignment (16)
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr2) % 16, 0u);  // 16 is default for >= 16

    Malloc.Free(ptr1);
    Malloc.Free(ptr2);
}

TEST_F(SystemMallocTest, AllocZeroedReturnsZeroedMemory)
{
    auto [ptr, size] = Malloc.AllocZeroed(32);
    ASSERT_NE(ptr, nullptr);
    ASSERT_EQ(size, 32u);

    const auto* data = static_cast<uint8_t*>(ptr);
    for (size_t i = 0; i < size; ++i)
    {
        EXPECT_EQ(data[i], 0u) << "Byte at index " << i << " not zeroed";
    }

    Malloc.Free(ptr);
}

TEST_F(SystemMallocTest, ReallocExpandsAndPreservesData)
{
    auto [ptr, oldSize] = Malloc.Alloc(64);
    ASSERT_NE(ptr, nullptr);
    std:memset(ptr, 0xCD, oldSize);

    auto [newPtr, newSize] = Malloc.Realloc(ptr, 128);
    ASSERT_NE(newPtr, nullptr);
    ASSERT_EQ(newSize, 128u);

    const auto* data = static_cast<uint8_t*>(newPtr);
    for (size_t i = 0; i < oldSize; ++i)
    {
        EXPECT_EQ(data[i], 0xCD) << "Byte at index " << i << " not preserved after realloc";
    }

    Malloc.Free(newPtr);
}

TEST_F(SystemMallocTest, ReallocWithNullptrBehavesLikeAlloc)
{
    auto [ptr, size] = Malloc.Realloc(nullptr, 48);

    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(size, 48u);

    Malloc.Free(ptr);
}

TEST_F(SystemMallocTest, ReallocWithZeroSizeFreesMemory)
{
    auto [ptr, size] = Malloc.Alloc(32);
    EXPECT_NE(ptr, nullptr);

    auto [newPtr, newSize] = Malloc.Realloc(ptr, 0);
    EXPECT_EQ(newPtr, nullptr);
    EXPECT_EQ(newSize, 0u);
}

TEST_F(SystemMallocTest, FreeAcceptsNullptr)
{
    EXPECT_NO_THROW(Malloc.Free(nullptr));
}