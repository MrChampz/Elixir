#include <gtest/gtest.h>
#include <Graphics/Vulkan/VulkanDescriptorPool.h>
#include <Graphics/Vulkan/VulkanShader.h>
#include "VulkanTestContext.h"
#include <cstring>
#include <Engine/Graphics/CommandBuffer.h>
#include <Engine/Graphics/Shader/ShaderLoader.h>
#include <Engine/Aether/Core/ParticleStateLayout.h>
#include <Engine/Aether/Effect/MaterialFactory.h>
#include <Engine/Materials/Compilation/Compiler.h>
#include <glm/gtc/matrix_transform.hpp>
#include <bit>
#include <Graphics/Vulkan/VulkanCommandBuffer.h>
#include <Engine/Aether/Manager.h>
#include <Engine/Aether/Simulation/Simulator.h>
#include <Engine/Aether/System.h>
#include <Engine/Aether/Modules/KillOutsideBounds.h>
#include <Engine/Aether/Modules/SizeOverLife.h>
#include <Engine/Aether/Modules/ApplyCurlNoise.h>
#include <Engine/Aether/Modules/ApplyGravity.h>
#include <Engine/Materials/MaterialSystem.h>
#include <Engine/Materials/MaterialRegistry.h>
#include <Engine/Camera/PerspectiveCamera.h>
#include <Engine/Core/Timer.h>



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

// Execute the actual strand shader and read its persistent state back from coherent memory.

namespace
{
    struct SStrandTestConstants
    {
        glm::mat4 RootTransform{1.0f};
        uint32_t ParticleOffset = 0;
        uint32_t PointOffset = 0;
        uint32_t PointCount = 32;
        uint32_t StrandCount = 1;
        float SegmentLength = 0.04f;
        float Stiffness = 1.0f;
        float Damping = 0.96f;
        uint32_t Generation = 1;
        uint32_t SolverIterations = 16;
        uint32_t InstanceIndex = 0;
        uint32_t EmitterIndex = 0;
        float GrowthSpeed = 0;
        float GrowthMaxLength = 0;
        uint32_t BranchCount = 0;
        uint32_t BranchInterval = 8;
        float BranchLengthScale = 0.5f;
    };
    static_assert(sizeof(SStrandTestConstants) == 128);
    static_assert(offsetof(SStrandTestConstants, ParticleOffset) == 64);
    static_assert(offsetof(SStrandTestConstants, Generation) == 92);

    TEST(VulkanShaderTest, StrandSolverPinsRootsKeepsFiniteStateAndResetsOnRespawn)
    {
        const auto context = VulkanTestContext::Get().GetGraphicsContext();
        ShaderLoader loader(context);
        const auto shader = loader.LoadShader("./Shaders/Aether", "StrandsUpdate", EShaderStage::Compute);
        ASSERT_TRUE(shader);
        const auto pipeline = ComputePipeline::Create(context, {.Shader = shader});
        ASSERT_TRUE(pipeline);
        // CoreV1: position/size, velocity/age, transform, tangent/id, color, metadata.
        std::array<glm::vec4, 6> root{};
        root[0] = {0, 0, 0, 16};
        root[2].y = 1;
        root[4] = {1, 1, 1, 1};
        root[5] = {0, std::bit_cast<float>(7u), 30, 1};
        const auto particles = DynamicStorageBuffer::Create(context, sizeof(root), root.data());
        constexpr uint32_t maxPoints = 96;
        const std::vector<glm::vec4> zeros(maxPoints * 3);
        const auto points = DynamicStorageBuffer::Create(context, zeros.size() * sizeof(glm::vec4), zeros.data());
        std::array<glm::vec4, 2> time{};
        const auto params = UniformBuffer::Create(context, sizeof(time), time.data());
        using namespace Aether::Core;
        using namespace Aether::Modules;
        const std::vector<SParameter> authoredParameters{{"CurlStrength", EParameterType::Float, glm::vec4(0.5f, 0, 0, 0)}};
        const auto compiledParameters = CompileParameters(authoredParameters);
        auto words = PackParameterValues(compiledParameters);
        SCompiledModuleData program;
        ModuleCompileContext compile(program, compiledParameters, "Force", 1);
        ApplyGravity({0, -0.15f, 0}).Compile(compile);
        ApplyCurlNoise().BindParameters("CurlStrength", "", "", "").Compile(compile);
        std::array<uint32_t, 20> emitterData{};
        emitterData[17] = uint32_t(program.Operations.size());
        std::array<uint32_t, 15> instanceData{};
        const auto emitterBuffer = DynamicStorageBuffer::Create(context, sizeof(emitterData), emitterData.data());
        const auto instanceBuffer = DynamicStorageBuffer::Create(context, sizeof(instanceData), instanceData.data());
        const auto operationBuffer = DynamicStorageBuffer::Create(context, program.Operations.size() * sizeof(SGPUOperation), program.Operations.data());
        const auto operandBuffer = DynamicStorageBuffer::Create(context, program.Operands.size() * sizeof(uint32_t), program.Operands.data());
        const auto constantBuffer = DynamicStorageBuffer::Create(context, program.Constants.size() * sizeof(uint32_t), program.Constants.data());
        const auto parameterBuffer = DynamicStorageBuffer::Create(context, words.size() * sizeof(uint32_t), words.data());
        shader->BindStorageBuffer("emitters", emitterBuffer);
        shader->BindStorageBuffer("instances", instanceBuffer);
        shader->BindStorageBuffer("operations", operationBuffer);
        shader->BindStorageBuffer("operands", operandBuffer);
        shader->BindStorageBuffer("constants", constantBuffer);
        shader->BindStorageBuffer("parameters", parameterBuffer);
        std::array<glm::vec4, 26> surfaceData{};
        static_assert(sizeof(surfaceData) == 416);
        const auto surfaceBuffer = DynamicStorageBuffer::Create(context, sizeof(surfaceData), surfaceData.data());
        shader->BindStorageBuffer("strandSurfaces", surfaceBuffer);
        std::array<glm::vec4,2> growthData{};
        const auto growthBuffer = DynamicStorageBuffer::Create(context,sizeof(growthData),growthData.data());
        shader->BindStorageBuffer("strandGrowth",growthBuffer);
        glm::vec4 endAnchorData{};
        const auto endAnchorBuffer = DynamicStorageBuffer::Create(context, sizeof(endAnchorData), &endAnchorData);
        shader->BindStorageBuffer("strandEndAnchors", endAnchorBuffer);
        SStrandTestConstants pc;
        shader->SetPushConstant("pc", &pc, sizeof(pc));
        shader->BindStorageBuffer("particles", particles);
        shader->BindStorageBuffer("strandPoints", points);
        shader->BindConstantBuffer("cbParams", params);
        const auto dispatch = [&](float dt)
        {
            time[0].x = dt;
            time[0].y += dt;
            params->UpdateData(time.data(), sizeof(time));
            const auto cmd = context->GetUploadCommandBuffer();
            cmd->Begin();
            points->Barrier(cmd, EPipelineStage::ComputeShader, EPipelineAccess::ShaderRead | EPipelineAccess::ShaderWrite);
            pipeline->Bind(cmd);
            shader->SetPushConstant(cmd, "pc", &pc, sizeof(pc));
            cmd->Dispatch(1);
            const VkMemoryBarrier hostBarrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr,
                VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT};
            vkCmdPipelineBarrier(static_cast<VulkanCommandBuffer*>(cmd.get())->GetVulkanCommandBuffer(),
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0,
                1, &hostBarrier, 0, nullptr, 0, nullptr);
            cmd->End();
            cmd->Flush();
        };
        const auto read = [&]()
        {
            const auto* mapped = static_cast<const glm::vec4*>(points->Map());
            std::vector<glm::vec4> result(mapped, mapped + maxPoints * 3);
            return result;
        };
        dispatch(0);
        const auto initial = read();
        EXPECT_EQ(std::bit_cast<uint32_t>(initial[0].w), 7u);
        EXPECT_EQ(std::bit_cast<uint32_t>(initial[1].w), 1u);
        EXPECT_NEAR(glm::length(glm::vec3(initial[3]) - glm::vec3(initial[0])), pc.SegmentLength, 0.00001f);
        dispatch(0);
        EXPECT_EQ(read(), initial);
        pc.RootTransform = glm::translate(glm::mat4(1), glm::vec3(0.1f, 0.2f, 0.3f));
        for (int frame = 0; frame < 20; ++frame) dispatch(frame % 2 ? 1.0f / 30 : 1.0f / 120);
        const auto moved = read();
        EXPECT_NEAR(moved[0].x, 0.1f, 0.00001f);
        EXPECT_NEAR(moved[0].y, 0.2f, 0.00001f);
        EXPECT_NEAR(moved[0].z, 0.3f, 0.00001f);
        for (uint32_t point = 0; point < pc.PointCount; ++point)
        {
            for (int axis = 0; axis < 3; ++axis) EXPECT_TRUE(std::isfinite(moved[point * 3][axis]));
            EXPECT_NEAR(glm::length(glm::vec3(moved[point * 3 + 2])), 1, 0.0001f);
            if (point > 0)
                EXPECT_LT(glm::length(glm::vec3(moved[point * 3]) - glm::vec3(moved[(point - 1) * 3])), pc.SegmentLength * 1.5f);
        }
        const auto stamp = std::bit_cast<uint32_t>(moved[0].w);
        words[0] = std::bit_cast<uint32_t>(0.0f);
        parameterBuffer->UpdateData(words.data(), words.size() * sizeof(uint32_t));
        dispatch(1.0f / 60);
        const auto withoutCurl = read();
        EXPECT_EQ(std::bit_cast<uint32_t>(withoutCurl[0].w), stamp);
        // Compare identical initial chains with and without curl. Runtime bindings
        // change the force without changing the spawn marker or point allocation.
        pc.RootTransform = glm::mat4(1);
        points->UpdateData(initial.data(), initial.size() * sizeof(glm::vec4));
        time[0].y = 1;
        dispatch(0.1f);
        const auto gravityOnly = read();
        words[0] = std::bit_cast<uint32_t>(1.0f);
        parameterBuffer->UpdateData(words.data(), words.size() * sizeof(uint32_t));
        points->UpdateData(initial.data(), initial.size() * sizeof(glm::vec4));
        time[0].y = 1;
        dispatch(0.1f);
        const auto withCurl = read();
        EXPECT_GT(glm::length(glm::vec3(withCurl[30 * 3]) - glm::vec3(gravityOnly[30 * 3])), 0.000001f);
        EXPECT_EQ(std::bit_cast<uint32_t>(withCurl[0].w), stamp);
        // Removing the point operation stream removes both authored forces.
        emitterData[17] = 0;
        emitterBuffer->UpdateData(emitterData.data(), sizeof(emitterData));
        points->UpdateData(initial.data(), initial.size() * sizeof(glm::vec4));
        dispatch(0.1f);
        const auto noForces = read();
        EXPECT_NEAR(glm::length(glm::vec3(noForces[30 * 3]) - glm::vec3(initial[30 * 3])), 0, 0.0001f);
        pc.Generation = 2;
        dispatch(0);
        const auto reset = read();
        EXPECT_EQ(std::bit_cast<uint32_t>(reset[1].w), 2u);
        EXPECT_NEAR(glm::length(glm::vec3(reset[3]) - glm::vec3(reset[0])), pc.SegmentLength, 0.00001f);
        root[5].y = std::bit_cast<float>(8u);
        particles->UpdateData(root.data(), sizeof(root));
        dispatch(0);
        EXPECT_EQ(std::bit_cast<uint32_t>(read()[0].w), 8u);
        root[5].w = 0;
        particles->UpdateData(root.data(), sizeof(root));
        dispatch(0);
        EXPECT_EQ(std::bit_cast<uint32_t>(read()[1].w), 0u);
        // Guidance follows a plane and pins completed segments in world coordinates.
        surfaceData[0] = {1, 0.1f, 0.014f, 4};
        surfaceData[1] = {0, 1, 0, 0};
        surfaceData[2] = {0, 0, 0, 0};
        surfaceData[4] = {0, 0, 1, 0};
        surfaceBuffer->UpdateData(surfaceData.data(), sizeof(surfaceData));
        pc.PointCount = 24;
        pc.BranchCount = 0;
        pc.GrowthSpeed = 0.2f;
        pc.GrowthMaxLength = 0.6f;
        pc.RootTransform = glm::mat4(1);
        pc.Generation = 10;
        root[0] = {0, 0, 0.014f, 16};
        root[1].w = 0;
        root[5].w = 1;
        particles->UpdateData(root.data(), sizeof(root));
        dispatch(0);
        for (int frame = 1; frame <= 20; ++frame)
        {
            root[1].w = float(frame) * 0.1f;
            particles->UpdateData(root.data(), sizeof(root));
            dispatch(0.1f);
        }
        const auto climbing = read();
        EXPECT_GT(climbing[9 * 3].y, 0.2f);
        EXPECT_EQ(std::bit_cast<uint32_t>(climbing[2 * 3 + 1].w), 1u);
        for (uint32_t point = 1; point < 9; ++point)
            EXPECT_NEAR(climbing[point * 3].z, 0.014f, 0.0001f);
        pc.RootTransform = glm::translate(glm::mat4(1), glm::vec3(0.2f, 0.1f, 0));
        dispatch(0.1f);
        EXPECT_EQ(glm::vec3(read()[2 * 3]), glm::vec3(climbing[2 * 3]));
        dispatch(0);
        EXPECT_EQ(glm::vec3(read()[2 * 3]), glm::vec3(climbing[2 * 3]));
        pc.Generation++;
        root[1].w = 0;
        particles->UpdateData(root.data(), sizeof(root));
        dispatch(0);
        EXPECT_EQ(std::bit_cast<uint32_t>(read()[2 * 3 + 1].w), 0u);
        // Enable release, regrow the attachments, then pull the root beyond the stretch limit.
        pc.RootTransform = glm::mat4(1);
        surfaceData[1].w = 2.5f;
        surfaceBuffer->UpdateData(surfaceData.data(), sizeof(surfaceData));
        pc.Generation++;
        root[1].w = 0;
        particles->UpdateData(root.data(), sizeof(root));
        dispatch(0);
        for (int frame = 1; frame <= 20; ++frame)
        {
            root[1].w = float(frame) * 0.1f;
            particles->UpdateData(root.data(), sizeof(root));
            dispatch(0.1f);
        }
        EXPECT_EQ(std::bit_cast<uint32_t>(read()[3 + 1].w), 1u);
        pc.RootTransform = glm::translate(glm::mat4(1), glm::vec3(0.8f, 0, 0));
        dispatch(0.1f);
        const auto released = read();
        EXPECT_LT(released[3 + 1].w, 0);
        EXPECT_GT(released[3].x, 0.1f);
        dispatch(0);
        EXPECT_FLOAT_EQ(read()[3 + 1].w, released[3 + 1].w);
        dispatch(0.1f);
        EXPECT_LT(read()[3 + 1].w, 0); // No immediate reattachment while cooldown remains.
        pc.RootTransform = glm::mat4(1);
        for (int frame = 0; frame < 30; ++frame) dispatch(1.0f / 30);
        EXPECT_EQ(std::bit_cast<uint32_t>(read()[3 + 1].w), 1u); // Reattach after the cooldown and tension settle.

        surfaceData[1].w = 0;
        surfaceBuffer->UpdateData(surfaceData.data(), sizeof(surfaceData));
        // Grow around a sphere and a rounded box, including fallback tangents at their bottom poles.
        pc.RootTransform = glm::mat4(1);
        for (uint32_t shape = 1; shape <= 2; ++shape)
        {
            surfaceData[2] = {0, 0, 0, float(shape)};
            surfaceData[3] = {0.3f, 0.3f, 0.3f, 0.3f};
            surfaceData[4] = {0, 0, 1, shape == 2 ? 0.05f : 0};
            surfaceBuffer->UpdateData(surfaceData.data(), sizeof(surfaceData));
            pc.Generation++;
            root[0] = {0, -0.314f, 0, 16};
            root[1].w = 0;
            particles->UpdateData(root.data(), sizeof(root));
            dispatch(0);
            for (int frame = 1; frame <= 20; ++frame)
            {
                root[1].w = float(frame) * 0.1f;
                particles->UpdateData(root.data(), sizeof(root));
                dispatch(0.1f);
            }
            const auto wrapped = read();
            EXPECT_EQ(std::bit_cast<uint32_t>(wrapped[2 * 3 + 1].w), 1u);
            for (uint32_t point = 1; point < 9; ++point)
            {
                const auto position = glm::vec3(wrapped[point * 3]);
                for (int axis = 0; axis < 3; ++axis) EXPECT_TRUE(std::isfinite(position[axis]));
                if (shape == 1) EXPECT_NEAR(glm::length(position), 0.314f, 0.0001f);
                else
                {
                    const auto q = glm::abs(position) - glm::vec3(0.25f);
                    const float distance = glm::length(glm::max(q, glm::vec3(0))) +
                        std::min(std::max(q.x, std::max(q.y, q.z)), 0.0f) - 0.05f;
                    EXPECT_NEAR(distance, 0.014f, 0.0001f);
                }
            }
        }
        surfaceData[0].x = 0;
        surfaceBuffer->UpdateData(surfaceData.data(), sizeof(surfaceData));
        pc.GrowthSpeed = 0;
        pc.GrowthMaxLength = 0;
        root[0] = {0, 0, 0, 16};
        particles->UpdateData(root.data(), sizeof(root));
        // Exercise the maximum supported chain length after changing generation.
        pc.Generation++;
        pc.PointCount = 96;
        root[5].w = 1;
        particles->UpdateData(root.data(), sizeof(root));
        dispatch(1.0f / 60);
        dispatch(0.1f);
        const auto longest = read();
        EXPECT_TRUE(std::isfinite(longest[(96 - 1) * 3].x));
        // Reserve one main chain and two side chains in the same root invocation.
        pc.PointCount = 24;
        pc.BranchCount = 2;
        pc.BranchInterval = 6;
        pc.GrowthSpeed = 0.2f;
        pc.GrowthMaxLength = 0.6f;
        pc.BranchLengthScale = 0.5f;
        pc.Generation = 3;
        root[1].w = 0;
        particles->UpdateData(root.data(), sizeof(root));
        dispatch(0);
        const auto sprout = read();
        EXPECT_FLOAT_EQ(sprout[2].w, 0);
        for (uint32_t point = 0; point < 72; ++point)
            EXPECT_TRUE(std::isfinite(sprout[point * 3].x));
        root[1].w = 0.5f;
        particles->UpdateData(root.data(), sizeof(root));
        dispatch(0.1f);
        const auto growing = read();
        EXPECT_NEAR(growing[2].w, 2.5f, 0.00001f);
        EXPECT_FLOAT_EQ(growing[24 * 3 + 2].w, 0);
        for (uint32_t point = 4; point < 24; ++point)
            EXPECT_EQ(glm::vec3(growing[point * 3]), glm::vec3(growing[3 * 3]));
        root[1].w = 2;
        particles->UpdateData(root.data(), sizeof(root));
        dispatch(0.1f);
        const auto branched = read();
        EXPECT_NEAR(branched[2].w, 10, 0.0001f);
        EXPECT_GT(branched[24 * 3 + 2].w, 0);
        EXPECT_FLOAT_EQ(branched[48 * 3 + 2].w, 0);
        EXPECT_NEAR(glm::length(glm::vec3(branched[24 * 3]) - glm::vec3(branched[6 * 3])), 0, 0.00001f);
        root[1].w = 20;
        particles->UpdateData(root.data(), sizeof(root));
        dispatch(0.1f);
        const auto mature = read();
        for (uint32_t chain = 0; chain < 3; ++chain)
        {
            const uint32_t base = chain * 24;
            EXPECT_NEAR(mature[base * 3 + 2].w, 15, 0.0001f);
            EXPECT_GT(mature[(base + 1) * 3 + 2].w, 15);
            for (uint32_t point = 0; point < 24; ++point)
                for (int axis = 0; axis < 3; ++axis)
                    EXPECT_TRUE(std::isfinite(mature[(base + point) * 3][axis]));
        }
        // The development clock stops without stopping physics or accumulating particle age.
        growthData[0] = {1,0,std::bit_cast<float>(123u),2};
        growthData[1] = {.8f,.3f,0,0};
        growthBuffer->UpdateData(growthData.data(),sizeof(growthData));
        pc.Generation++;
        root[1].w = 100;
        particles->UpdateData(root.data(),sizeof(root));
        dispatch(.1f);
        EXPECT_NEAR(read()[2 * 3 + 2].w,.1f,.00001f);
        time[0].z = 1;
        pc.RootTransform = glm::translate(glm::mat4(1),glm::vec3(.15f,0,0));
        for (int frame=0; frame<5; ++frame) dispatch(.1f);
        EXPECT_NEAR(read()[2 * 3 + 2].w,.1f,.00001f);
        EXPECT_NEAR(read()[0].x,.15f,.00001f);
        time[0].z = 0;
        dispatch(.1f);
        EXPECT_NEAR(read()[2 * 3 + 2].w,.2f,.00001f);
        growthData[0].y = 1;
        growthBuffer->UpdateData(growthData.data(),sizeof(growthData));
        dispatch(.1f);
        EXPECT_NEAR(read()[2].w,15,.0001f);
        growthData[0].y = 2;
        growthBuffer->UpdateData(growthData.data(),sizeof(growthData));
        const float matureClock = read()[2 * 3 + 2].w;
        dispatch(.1f);
        EXPECT_FLOAT_EQ(read()[2 * 3 + 2].w,matureClock);
        growthData[0].x = 0;
        growthBuffer->UpdateData(growthData.data(),sizeof(growthData));
        pc.Generation++;
        pc.RootTransform = glm::mat4(1);
        root[1].w = 20;
        particles->UpdateData(root.data(),sizeof(root));
        dispatch(0);
        // A paused step preserves the final length and a dead root invalidates all branches.
        dispatch(0);
        EXPECT_FLOAT_EQ(read()[2].w, mature[2].w);
        // A two-ended cable has immutable endpoints and free interior points under forces.
        surfaceData.fill({});
        surfaceBuffer->UpdateData(surfaceData.data(), sizeof(surfaceData));
        pc.PointCount = 40; pc.BranchCount = 0; pc.GrowthSpeed = 0; pc.GrowthMaxLength = 0;
        pc.SegmentLength = .03f; pc.RootTransform = glm::mat4(1); ++pc.Generation;
        root[0] = {-.5f,.5f,0,16};
        particles->UpdateData(root.data(),sizeof(root));
        endAnchorData = {.5f,.5f,0,1};
        endAnchorBuffer->UpdateData(&endAnchorData,sizeof(endAnchorData));
        dispatch(0);
        for (int frame=0; frame<120; ++frame) dispatch(1.0f/60);
        auto cable = read();
        EXPECT_NEAR(cable[0].x,-.5f,.00001f);
        EXPECT_NEAR(cable[0].y,.5f,.00001f);
        EXPECT_NEAR(cable[39 * 3].x,.5f,.00001f);
        EXPECT_NEAR(cable[39 * 3].y,.5f,.00001f);
        EXPECT_LT(cable[20 * 3].y,.45f);
        for (uint32_t point=0; point<40; ++point)
        {
            for (int axis=0; axis<3; ++axis) EXPECT_TRUE(std::isfinite(cable[point * 3][axis]));
            if (point) EXPECT_LT(glm::distance(glm::vec3(cable[point * 3]),glm::vec3(cable[(point-1) * 3])),.04f);
        }
        // Simulation-space anchors follow translation, rotation and scale even at dt zero.
        pc.RootTransform = glm::translate(glm::mat4(1),glm::vec3(.2f,-.1f,.3f)) *
            glm::rotate(glm::mat4(1),.35f,glm::vec3(0,0,1)) * glm::scale(glm::mat4(1),glm::vec3(1.3f));
        dispatch(0);
        cable = read();
        auto expectedEnd = pc.RootTransform * glm::vec4(.5f,.5f,0,1);
        auto expectedRoot = pc.RootTransform * glm::vec4(-.5f,.5f,0,1);
        EXPECT_LT(glm::distance(glm::vec3(cable[0]),glm::vec3(expectedRoot)),.00001f);
        EXPECT_LT(glm::distance(glm::vec3(cable[39 * 3]),glm::vec3(expectedEnd)),.00001f);
        EXPECT_LT(glm::distance(glm::vec3(cable[39 * 3 + 1]),glm::vec3(expectedEnd)),.00001f);
        // World-space targets remain independent of the root transform and can move while paused.
        endAnchorData = {.8f,.6f,-.2f,2};
        endAnchorBuffer->UpdateData(&endAnchorData,sizeof(endAnchorData));
        dispatch(0);
        cable = read();
        EXPECT_LT(glm::distance(glm::vec3(cable[39 * 3]),glm::vec3(endAnchorData)),.00001f);
        for (int frame=0; frame<30; ++frame) dispatch(1.0f/60);
        EXPECT_LT(glm::distance(glm::vec3(read()[39 * 3]),glm::vec3(endAnchorData)),.00001f);
        // Respawning initializes the spanning curve again without detaching either end.
        ++pc.Generation;
        dispatch(0);
        EXPECT_LT(glm::distance(glm::vec3(read()[39 * 3]),glm::vec3(endAnchorData)),.00001f);
        endAnchorData = {};
        endAnchorBuffer->UpdateData(&endAnchorData,sizeof(endAnchorData));
        pc.PointCount = 24; pc.BranchCount = 2;
        root[5].w = 0;
        particles->UpdateData(root.data(), sizeof(root));
        dispatch(0);
        for (uint32_t chain = 0; chain < 3; ++chain)
            EXPECT_EQ(std::bit_cast<uint32_t>(read()[chain * 24 * 3 + 1].w), 0u);

    }

    TEST(VulkanShaderTest, CompilesStrandMaterialVariantWithParticleGraphBindings)
    {
        const auto context = VulkanTestContext::Get().GetGraphicsContext();
        ShaderLoader loader(context);
        Aether::Effect::SMaterialDescription desc;
        desc.BlendMode = Materials::EMaterialBlendMode::Opaque;
        const auto material = Aether::Effect::CreateMaterial("Strand test", desc);
        const auto compiled = Materials::Compilation::Compiler::Compile(&loader, *material);
        ASSERT_TRUE(compiled) << compiled.Diagnostics;
        ASSERT_TRUE(compiled.Material->ParticleStrandShader);
        EXPECT_TRUE(compiled.Material->ParticleStrandShader->HasBinding("particles"));
        EXPECT_TRUE(compiled.Material->ParticleStrandShader->HasBinding("strandPoints"));
        EXPECT_TRUE(compiled.Material->ParticleStrandShader->HasBinding("strandSurfaces"));
    }
}


TEST(VulkanShaderTest, StrandEffectSimulatesAndRecordsTubeDrawsThroughAether)
{
    const auto context = VulkanTestContext::Get().GetGraphicsContext();
    ShaderLoader loader(context);
    Materials::MaterialRegistry registry;
    Materials::MaterialSystem materials(context, {}, &loader,
        context->GetRenderTarget(), context->GetDepthStencilRenderTarget());
    class FrameObserver final : public Aether::Manager
    {
    public:
        using Manager::Manager;
        uint32_t ObservedFrames = 0;
        Ref<const Aether::Simulation::RenderFrame> LastFrame;
    protected:
        void OnRenderFrame(const Ref<const Aether::Simulation::RenderFrame>& frame,
            const Camera&, const Ref<Image>&) override
        {
            ++ObservedFrames;
            LastFrame = frame;
        }
    };
    FrameObserver manager(context, &loader, registry, materials);
    const auto assetRoot = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path().parent_path();
    const auto system = Aether::Manager::LoadEffect(assetRoot / "Assets/VFX/GPUStrands.json");
    ASSERT_TRUE(system);
    const auto instance = system->CreateInstance();
    ASSERT_TRUE(manager.Add(instance));
    PerspectiveCamera camera(45, 16.0f / 9, 0.01f, 100);
    camera.SetPosition({0, 0, 3});
    Materials::Rendering::SRenderResult result;
    for (int frame = 0; frame < 3; ++frame)
    {
        instance->SetWorldTransform(glm::translate(glm::mat4(1), glm::vec3(frame * 0.02f, 0, 0)));
        context->RenderFrame([&]()
        {
            materials.BeginFrame();
            manager.BeginFrame(Timestep(1.0f / 30));
            manager.Render(camera, context->GetRenderTarget());
            result = materials.RenderFrame();
        });
        ASSERT_TRUE(context->RunRenderTaskAndWait([] {}));
    }
    context->WaitDeviceIdle();
    EXPECT_EQ(manager.ObservedFrames, 3u);
    ASSERT_TRUE(manager.LastFrame);
    ASSERT_EQ(manager.LastFrame->GetItems().size(), 1u);
    ASSERT_FALSE(manager.LastFrame->GetResources().empty());
    EXPECT_TRUE(manager.LastFrame->GetResources()[0].StrandPointBuffer);
    EXPECT_EQ(result.DrawCount, 1u);
    EXPECT_EQ(result.MaterialCount, 1u);
    EXPECT_EQ(manager.GetLastSimulationMetrics().SubmittedEmitterCount, 1u);
    EXPECT_TRUE(manager.Remove(instance));
}

TEST(VulkanShaderTest, StrandSamplesCompileAndRenderWithMovingRoots)
{
    for (const auto* filename : {"StrandCables.json", "StrandSeaweed.json", "StrandEnergy.json", "StrandIvy.json", "StrandClimbingIvy.json", "StrandFern.json", "StrandWillow.json", "StrandPowerLines.json", "StrandBalloons.json"})
    {
        SCOPED_TRACE(filename);
        const auto context = VulkanTestContext::Get().GetGraphicsContext();
        ShaderLoader loader(context);
        Materials::MaterialRegistry registry;
        Materials::MaterialSystem materials(context, {}, &loader,
            context->GetRenderTarget(), context->GetDepthStencilRenderTarget());
        Aether::Manager manager(context, &loader, registry, materials);
        const auto assetRoot = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path().parent_path();
        const auto system = Aether::Manager::LoadEffect(assetRoot / std::filesystem::path("Assets/VFX") / filename);
        ASSERT_TRUE(system);
        const auto instance = system->CreateInstance();
        ASSERT_TRUE(manager.Add(instance));
        PerspectiveCamera camera(45, 16.0f / 9, 0.01f, 100);
        camera.SetPosition({0, 0, 3});
        Materials::Rendering::SRenderResult result;
        for (int frame = 0; frame < 12; ++frame)
        {
            instance->SetWorldTransform(glm::translate(glm::mat4(1), glm::vec3(frame * 0.02f, 0, 0)));
            context->RenderFrame([&]()
            {
                materials.BeginFrame();
                manager.BeginFrame(Timestep(0.1f));
                manager.Render(camera, context->GetRenderTarget());
                result = materials.RenderFrame();
            });
            ASSERT_TRUE(context->RunRenderTaskAndWait([] {}));
        }
        context->WaitDeviceIdle();
        EXPECT_EQ(result.DrawCount, system->GetEmitters().size());
        EXPECT_EQ(result.MaterialCount, system->GetEmitters().size());
        EXPECT_EQ(manager.GetLastSimulationMetrics().SubmittedEmitterCount, system->GetEmitters().size());
        EXPECT_TRUE(manager.Remove(instance));
    }
}

TEST(VulkanShaderTest, InitialBurstRunsOnceAndPeriodicAndTriggeredBurstsKeepTheirTiming)
{
    const auto context = VulkanTestContext::Get().GetGraphicsContext();
    ShaderLoader loader(context);
    const auto shader = loader.LoadShader("./Shaders/Aether", "ParticlesSchedulerScheduleEmitters", EShaderStage::Compute);
    ASSERT_TRUE(shader);
    const auto pipeline = ComputePipeline::Create(context, {.Shader = shader});
    ASSERT_TRUE(pipeline);
    // Mirror the emitter, instance and scheduler layouts with native four-byte words.
    std::array<uint32_t, 20> emitter{};
    emitter[1] = 64; // particle capacity
    emitter[12] = 64; // burst count
    std::array<uint32_t, 15> instance{};
    instance[10] = 64;
    instance[11] = 1;
    instance[12] = 1;
    instance[13] = 1;
    std::array<uint32_t, 5> state{};
    state[4] = 1;
    std::array<uint32_t, 4> request{};
    std::array<uint32_t, 3> scheduler{1, 0, 0};
    std::array<uint32_t, 4> queues{};
    std::array<uint32_t, 3> event{};
    std::array<glm::vec4, 2> time{};
    const auto emitters = DynamicStorageBuffer::Create(context, sizeof(emitter), emitter.data());
    const auto instances = DynamicStorageBuffer::Create(context, sizeof(instance), instance.data());
    const auto states = DynamicStorageBuffer::Create(context, sizeof(state), state.data());
    const auto requests = DynamicStorageBuffer::Create(context, sizeof(request), request.data());
    const auto schedulers = DynamicStorageBuffer::Create(context, sizeof(scheduler), scheduler.data());
    const auto queueBuffer = DynamicStorageBuffer::Create(context, sizeof(queues), queues.data());
    const auto eventsA = DynamicStorageBuffer::Create(context, sizeof(event), event.data());
    const auto eventsB = DynamicStorageBuffer::Create(context, sizeof(event), event.data());
    const auto targets = DynamicStorageBuffer::Create(context, sizeof(event), event.data());
    const auto params = UniformBuffer::Create(context, sizeof(time), time.data());
    uint32_t index = 0;
    shader->SetPushConstant("pc", &index, sizeof(index));
    shader->BindStorageBuffer("emitters", emitters);
    shader->BindStorageBuffer("instances", instances);
    shader->BindStorageBuffer("emitterStates", states);
    shader->BindStorageBuffer("spawnRequests", requests);
    shader->BindStorageBuffer("schedulerStates", schedulers);
    shader->BindStorageBuffer("triggerQueueStates", queueBuffer);
    shader->BindStorageBuffer("triggerEventsA", eventsA);
    shader->BindStorageBuffer("triggerEventsB", eventsB);
    shader->BindStorageBuffer("triggerTargets", targets);
    shader->BindConstantBuffer("cbParams", params);
    const auto dispatch = [&](float dt)
    {
        time[0].x = dt;
        params->UpdateData(time.data(), sizeof(time));
        const auto cmd = context->GetUploadCommandBuffer();
        cmd->Begin();
        states->Barrier(cmd, EPipelineStage::ComputeShader, EPipelineAccess::ShaderRead | EPipelineAccess::ShaderWrite);
        requests->Barrier(cmd, EPipelineStage::ComputeShader, EPipelineAccess::ShaderWrite);
        pipeline->Bind(cmd);
        cmd->Dispatch(1);
        const VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr,
            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT};
        vkCmdPipelineBarrier(static_cast<VulkanCommandBuffer*>(cmd.get())->GetVulkanCommandBuffer(),
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0,
            1, &barrier, 0, nullptr, 0, nullptr);
        cmd->End();
        cmd->Flush();
        std::memcpy(request.data(), requests->Map(), sizeof(request));
        requests->Unmap();
    };
    dispatch(0);
    EXPECT_EQ(request[1], 64u);
    EXPECT_EQ(request[2], 0u);
    // More than two seconds of simulation must not replace any root.
    for (int step = 0; step < 30; ++step)
    {
        dispatch(0.1f);
        EXPECT_EQ(request[1], 0u);
        EXPECT_EQ(request[2], 64u);
    }
    state = {};
    state[4] = instance[13] = 2;
    states->UpdateData(state.data(), sizeof(state));
    instances->UpdateData(instance.data(), sizeof(instance));
    dispatch(0);
    EXPECT_EQ(request[1], 64u); // reset starts a new initial burst
    EXPECT_EQ(request[3], 2u);

    // A positive interval still waits for the interval and repeats.
    state = {};
    state[4] = 2;
    states->UpdateData(state.data(), sizeof(state));
    emitter[11] = std::bit_cast<uint32_t>(0.5f);
    emitter[12] = 2;
    emitters->UpdateData(emitter.data(), sizeof(emitter));
    dispatch(0.25f);
    EXPECT_EQ(request[1], 0u);
    dispatch(0.25f);
    EXPECT_EQ(request[1], 2u);
    dispatch(0.5f);
    EXPECT_EQ(request[1], 2u);

    // Trigger-driven emitters with interval zero must not emit an initial burst.
    state = {};
    state[4] = 2;
    states->UpdateData(state.data(), sizeof(state));
    emitter[11] = 0;
    emitter[13] = 1;
    emitters->UpdateData(emitter.data(), sizeof(emitter));
    dispatch(0.1f);
    EXPECT_EQ(request[1], 0u);
    event = {0, 5, 2};
    queues[0] = 1;
    eventsA->UpdateData(event.data(), sizeof(event));
    queueBuffer->UpdateData(queues.data(), sizeof(queues));
    dispatch(0.1f);
    EXPECT_EQ(request[1], 5u);
}

TEST(VulkanShaderTest, LifetimeDeathPolicyPreservesSimulationNormalizedAgeAndExplicitKills)
{
    const auto context = VulkanTestContext::Get().GetGraphicsContext();
    ShaderLoader loader(context);
    const auto shader = loader.LoadShader("./Shaders/Aether", "ParticlesUpdate", EShaderStage::Compute);
    ASSERT_TRUE(shader);
    const auto pipeline = ComputePipeline::Create(context, {.Shader = shader});
    ASSERT_TRUE(pipeline);
    using namespace Aether::Core;
    using namespace Aether::Modules;
    SCompiledModuleData program;
    ModuleCompileContext compiler(program, {}, "Policy", 1.0f);
    KillOutsideBounds({-1, -1, -1}, {1, 1, 1}).Compile(compiler);
    SizeOverLife(2, 6).Compile(compiler);
    ApplyCurlNoise(1, 2.2f, {0, 0, 0}, 17).Compile(compiler);
    ASSERT_EQ(program.Operations.size(), 3);
    std::array<glm::vec4, 30> roots{};
    std::array<uint32_t, 100> emittersData{};
    for (uint32_t index = 0; index < 5; ++index)
    {
        roots[index * 6] = {0, 0, 0, 16};
        roots[index * 6 + 1] = {1, 0, 0, 2}; // velocity and age greater than lifetime
        roots[index * 6 + 2].y = 1;
        roots[index * 6 + 4] = {1, 1, 1, 1};
        roots[index * 6 + 5] = {std::bit_cast<float>(index), 0, 0.25f, 1};
        emittersData[index * 20 + 1] = 1;
        // The lifetime flag remains at word 14 in the 80-byte emitter layout.
        emittersData[index * 20 + 14] = index == 1 ? 1 : 0;
    }
    roots[2 * 6].x = 2; // explicit kill outside the box
    emittersData[2 * 20 + 4] = 0;
    emittersData[2 * 20 + 5] = 1;
    emittersData[3 * 20 + 4] = 1;
    emittersData[3 * 20 + 5] = 1;
    emittersData[4 * 20 + 4] = 2;
    emittersData[4 * 20 + 5] = 1;
    std::array<uint32_t, 15> instance{};
    instance[10] = 5;
    instance[11] = 5;
    std::array<glm::vec4, 2> time{};
    time[0] = {0.1f, 2.1f, 0, 0};
    const uint32_t zero = 0;
    const auto particles = DynamicStorageBuffer::Create(context, sizeof(roots), roots.data());
    const auto emitters = DynamicStorageBuffer::Create(context, sizeof(emittersData), emittersData.data());
    const auto instances = DynamicStorageBuffer::Create(context, sizeof(instance), instance.data());
    const auto operations = DynamicStorageBuffer::Create(context, program.Operations.size() * sizeof(SGPUOperation), program.Operations.data());
    const auto operands = DynamicStorageBuffer::Create(context, program.Operands.size() * sizeof(uint32_t), program.Operands.data());
    const auto constants = DynamicStorageBuffer::Create(context, program.Constants.size() * sizeof(uint32_t), program.Constants.data());
    const auto parameters = DynamicStorageBuffer::Create(context, sizeof(zero), &zero);
    const auto params = UniformBuffer::Create(context, sizeof(time), time.data());
    uint32_t index = 0;
    shader->SetPushConstant("pc", &index, sizeof(index));
    shader->BindStorageBuffer("particles", particles);
    shader->BindStorageBuffer("emitters", emitters);
    shader->BindStorageBuffer("instances", instances);
    shader->BindStorageBuffer("operations", operations);
    shader->BindStorageBuffer("operands", operands);
    shader->BindStorageBuffer("constants", constants);
    shader->BindStorageBuffer("parameters", parameters);
    shader->BindConstantBuffer("cbParams", params);
    const auto cmd = context->GetUploadCommandBuffer();
    cmd->Begin();
    pipeline->Bind(cmd);
    cmd->Dispatch(1);
    const VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT};
    vkCmdPipelineBarrier(static_cast<VulkanCommandBuffer*>(cmd.get())->GetVulkanCommandBuffer(),
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0,
        1, &barrier, 0, nullptr, 0, nullptr);
    cmd->End();
    cmd->Flush();
    std::memcpy(roots.data(), particles->Map(), sizeof(roots));
    particles->Unmap();
    EXPECT_EQ(roots[5].w, 1); // persistent particle remains alive
    EXPECT_NEAR(roots[1].w, 2.1f, 0.00001f); // age still advances
    EXPECT_NEAR(roots[0].x, 0.1f, 0.00001f); // velocity still integrates
    EXPECT_EQ(roots[1 * 6 + 5].w, 0); // default policy expires
    EXPECT_EQ(roots[2 * 6 + 5].w, 0); // explicit kill remains effective
    EXPECT_EQ(roots[3 * 6 + 5].w, 1);
    EXPECT_EQ(roots[3 * 6].w, 6); // NormalizedAge saturates at one instead of freezing at zero
    EXPECT_EQ(roots[4 * 6 + 5].w, 1);
    EXPECT_GT(glm::length(glm::vec3(roots[4 * 6 + 1]) - glm::vec3(1, 0, 0)), 0.000001f); // Particle-target curl updates velocity.
}
