#include <gtest/gtest.h>
#include <Graphics/Vulkan/VulkanDescriptorPool.h>
#include <Graphics/Vulkan/VulkanGraphicsContext.h>

#include "VulkanTestContext.h"

using namespace Elixir::Vulkan;

namespace
{
    class VulkanDescriptorPoolTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            Context = static_cast<VulkanGraphicsContext*>(VulkanTestContext::Get().GetGraphicsContext());
            VkDescriptorSetLayoutBinding binding{};
            binding.binding = 0;
            binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            binding.descriptorCount = 1;
            binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            VkDescriptorSetLayoutCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            info.bindingCount = 1;
            info.pBindings = &binding;
            ASSERT_EQ(vkCreateDescriptorSetLayout(Context->GetDevice(), &info, nullptr, &Layout), VK_SUCCESS);
        }

        void TearDown() override
        {
            vkDestroyDescriptorSetLayout(Context->GetDevice(), Layout, nullptr);
        }

        VulkanGraphicsContext* Context = nullptr;
        VkDescriptorSetLayout Layout = VK_NULL_HANDLE;
    };

    TEST_F(VulkanDescriptorPoolTest, GrowsAndFreesSetsFromTheirOwningPools)
    {
        std::array<SDescriptorPoolSizeRatio, 1> ratios{{ { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1.0f } }};
        VulkanDescriptorPool pool(*Context, 2, ratios);
        std::vector<VkDescriptorSet> sets;
        for (int i = 0; i < 7; ++i)
        {
            const auto set = pool.Allocate(Layout);
            ASSERT_NE(set, VK_NULL_HANDLE);
            sets.push_back(set);
        }
        EXPECT_EQ(pool.GetPoolCount(), 4u);
        // Free a mixed list spanning all pools, then reuse those pools repeatedly.
        pool.FreeDescriptorSets(sets);
        for (int cycle = 0; cycle < 20; ++cycle)
        {
            sets.clear();
            for (int i = 0; i < 7; ++i)
            {
                const auto set = pool.Allocate(Layout);
                ASSERT_NE(set, VK_NULL_HANDLE);
                sets.push_back(set);
            }
            pool.FreeDescriptorSets(sets);
            EXPECT_EQ(pool.GetPoolCount(), 4u);
        }
    }

    TEST_F(VulkanDescriptorPoolTest, ResetReusesAllPools)
    {
        std::array<SDescriptorPoolSizeRatio, 1> ratios{{ { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1.0f } }};
        VulkanDescriptorPool pool(*Context, 1, ratios);
        ASSERT_NE(pool.Allocate(Layout), VK_NULL_HANDLE);
        ASSERT_NE(pool.Allocate(Layout), VK_NULL_HANDLE);
        ASSERT_EQ(pool.GetPoolCount(), 2u);
        pool.Reset();
        const auto first = pool.Allocate(Layout);
        const auto second = pool.Allocate(Layout);
        EXPECT_NE(first, VK_NULL_HANDLE);
        EXPECT_NE(second, VK_NULL_HANDLE);
        EXPECT_EQ(pool.GetPoolCount(), 2u);
        pool.FreeDescriptorSets({ first, second });
    }
}
