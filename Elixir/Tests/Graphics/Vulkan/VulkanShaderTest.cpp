#include <gtest/gtest.h>

#include <Graphics/Vulkan/VulkanDescriptorPool.h>
#include <Graphics/Vulkan/VulkanShader.h>
#include "VulkanTestContext.h"

#include <cstring>

using namespace Elixir;
using namespace Elixir::Vulkan;

namespace
{
    // A minimal compute module keeps these tests independent of the shader compiler.
    SShaderCreateInfo MakeShaderInfo()
    {
        constexpr uint32_t bytecode[] = {
            0x07230203, 0x00010000, 0, 5, 0,
            0x00020011, 1,                         // OpCapability Shader
            0x0003000e, 0, 1,                      // OpMemoryModel Logical GLSL450
            0x0005000f, 5, 3, 0x6e69616d, 0,        // OpEntryPoint GLCompute %3 "main"
            0x00060010, 3, 17, 1, 1, 1,            // OpExecutionMode LocalSize 1 1 1
            0x00020013, 1,                         // OpTypeVoid %1
            0x00030021, 2, 1,                      // OpTypeFunction %2 %1
            0x00050036, 1, 3, 0, 2,                // OpFunction
            0x000200f8, 4,                         // OpLabel
            0x000100fd,                            // OpReturn
            0x00010038,                            // OpFunctionEnd
        };
        SShaderCreateInfo info;
        info.Name = "Vulkan shader validation";
        info.Compute = CreateScope<SShaderModuleCreateInfo>();
        info.Compute->Entrypoint = "main";
        info.Compute->Bytecode.resize(sizeof(bytecode));
        std::memcpy(info.Compute->Bytecode.data(), bytecode, sizeof(bytecode));
        return info;
    }

    TEST(VulkanShaderTest, CreatesPipelineLayoutWithoutDescriptors)
    {
        const auto context = VulkanTestContext::Get().GetGraphicsContext();
        const auto shader = CreateRef<VulkanShader>(context, MakeShaderInfo());
        EXPECT_TRUE(shader->IsValid());
        EXPECT_NE(shader->GetPipelineLayout(), VK_NULL_HANDLE);
        EXPECT_TRUE(shader->GetDescriptorSets().empty());
    }

    TEST(VulkanShaderTest, AllocatesSparseSetsAndReleasesThemOnDestruction)
    {
        const auto context = static_cast<VulkanGraphicsContext*>(VulkanTestContext::Get().GetGraphicsContext());
        size_t poolCount = 0;
        // Exceed a pool's capacity across cycles so even one leaked set per shader is detected.
        const auto cycles = context->GetDescriptorPool()->GetMaxSets() + 1;
        for (uint32_t cycle = 0; cycle < cycles; ++cycle)
        {
            auto info = MakeShaderInfo();
            info.Compute->ConstantBuffers.emplace_back("Constants", 2, 0);
            const auto shader = CreateRef<VulkanShader>(context, std::move(info));
            ASSERT_TRUE(shader->IsValid());
            ASSERT_EQ(shader->GetDescriptorSetLayouts().size(), 3u);
            const auto sets = shader->GetDescriptorSets();
            ASSERT_EQ(sets.size(), 3u);
            for (const auto set : sets) EXPECT_NE(set, VK_NULL_HANDLE);
            if (cycle == 0) poolCount = context->GetDescriptorPool()->GetPoolCount();
            EXPECT_EQ(context->GetDescriptorPool()->GetPoolCount(), poolCount);
        }
    }

    TEST(VulkanShaderTest, RejectsSetIndexOutsideDeviceLimit)
    {
        const auto context = static_cast<VulkanGraphicsContext*>(VulkanTestContext::Get().GetGraphicsContext());
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(context->GetGPU(), &properties);
        auto info = MakeShaderInfo();
        info.Compute->ConstantBuffers.emplace_back("Constants", properties.limits.maxBoundDescriptorSets, 0);
        const auto shader = CreateRef<VulkanShader>(context, std::move(info));
        EXPECT_FALSE(shader->IsValid());
        EXPECT_EQ(shader->GetPipelineLayout(), VK_NULL_HANDLE);
        EXPECT_TRUE(shader->GetDescriptorSets().empty());
        // Binding must stop before accessing command buffers or invalid native handles.
        shader->Bind(nullptr, nullptr);

        auto factoryInfo = MakeShaderInfo();
        factoryInfo.Compute->ConstantBuffers.emplace_back("Constants", properties.limits.maxBoundDescriptorSets, 0);
        EXPECT_EQ(Shader::Create(context, std::move(factoryInfo)), nullptr);
        EXPECT_NE(Shader::Create(context, MakeShaderInfo()), nullptr);
    }
}
