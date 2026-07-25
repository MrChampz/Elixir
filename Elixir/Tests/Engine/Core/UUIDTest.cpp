#include <gtest/gtest.h>

#include <Engine/Core/UUID.h>

using namespace Elixir;

TEST(UUIDTest, HashSupportsEquivalentCopiedValues)
{
    const UUID id;
    const UUID copy{ id };

    std::unordered_set<UUID> ids;
    ids.insert(id);

    EXPECT_TRUE(ids.contains(copy));
}
