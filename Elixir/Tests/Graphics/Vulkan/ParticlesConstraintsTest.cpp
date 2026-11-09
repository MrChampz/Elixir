#include <gtest/gtest.h>

#include "VulkanTestContext.h"
#include "../../Engine/Aether/TestInstanceRegistry.h"
#include <Engine/Aether/Effect/Effect.h>
#include <Engine/Aether/Rendering/FrameSubmission.h>
#include <Engine/Aether/Simulation/Simulator.h>
#include <Engine/Aether/Simulation/ConstraintData.h>
#include <Engine/Graphics/Shader/ShaderLoader.h>
#include <Graphics/Vulkan/VulkanCommandBuffer.h>
#include <Graphics/Vulkan/VulkanShader.h>
#include <Graphics/Vulkan/VulkanBuffer.h>
#include <bit>
#include <cmath>
#include <cstring>
#include <glm/gtc/matrix_transform.hpp>

using namespace Elixir;
using namespace Elixir::Vulkan;

namespace
{
    /** Mirrors CoreV1 without introducing a second simulation buffer. */
    struct SConnectionParticle
    {
        glm::vec4 PositionSize{};
        glm::vec4 VelocityAge{};
        glm::vec4 Transform{0, 1, 0, 0};
        glm::vec4 TangentGroup{};
        glm::vec4 Color{1};
        glm::vec4 Metadata{0, 0, 10, 1};
    };

    struct SConstraintPushConstants
    {
        uint32_t ParticleOffset = 1;
        uint32_t ParticleCount = 8;
        uint32_t MaxPoints = 4;
        uint32_t OrderStep = 2;
        float SegmentLength = 1;
        float Stiffness = 1;
        uint32_t SolverIterations = 16;
        uint32_t PinFirstParticle = 1;
        float DeltaSeconds = 0.1f;
    };

    static_assert(sizeof(SConnectionParticle) == 96);
    static_assert(sizeof(SConstraintPushConstants) == 36);
}

TEST(ParticlesConstraintsTest, SolvesInterleavedParticleChainsAndPreservesLifecycle)
{
    const auto context = VulkanTestContext::Get().GetGraphicsContext();
    ShaderLoader loader(context);
    const auto shader = loader.LoadShader(
        "./Shaders/Aether",
        "ParticlesConstraints",
        EShaderStage::Compute
    );
    ASSERT_TRUE(shader);
    const Aether::Simulation::SConstraintFrameData constraintData{};
    const auto constraintBuffer = UniformBuffer::Create(
        context,
        sizeof(constraintData),
        &constraintData
    );
    if (shader->HasBinding("cbConstraint"))
        shader->BindConstantBuffer("cbConstraint", constraintBuffer);
    const auto pipeline = ComputePipeline::Create(context, {.Shader = shader});
    ASSERT_TRUE(pipeline);
    const auto ranges = static_cast<VulkanShader*>(shader.get())->GetPushConstantRanges();
    ASSERT_EQ(ranges.size(), 1u);
    EXPECT_EQ(ranges[0].size, sizeof(SConstraintPushConstants));

    std::array<SConnectionParticle, 98> points{};
    for (uint32_t slot = 0; slot < 8; ++slot)
    {
        auto& point = points[slot + 1];
        point.PositionSize = {float(slot / 2) * 1.2f + 0.01f, float(slot % 2) * 10, 0, 2};
        point.VelocityAge = {0.1f, 0, 0, 3};
        point.TangentGroup.w = std::bit_cast<float>(slot % 2);
        point.Metadata.y = std::bit_cast<float>(slot);
    }
    const auto initial = points;
    const auto buffer = StorageBuffer::Create(context, sizeof(points));
    auto readbackInfo = StagingBuffer::CreateBufferInfo(sizeof(points), nullptr);
    readbackInfo.Usage = EBufferUsage::TransferDst;
    const Ref<StagingBuffer> readback = CreateRef<VulkanStagingBuffer>(context, readbackInfo);
    ASSERT_TRUE(buffer);
    shader->BindStorageBuffer("particles", buffer);
    SConstraintPushConstants pc;
    shader->SetPushConstant("pc", &pc, sizeof(pc));
    const auto dispatch = [&]()
    {
        const auto upload = StagingBuffer::Create(context, sizeof(points), points.data());
        const auto command = context->GetUploadCommandBuffer();
        command->Begin();
        buffer->Barrier(command, EPipelineStage::Transfer, EPipelineAccess::TransferWrite);
        command->CopyBuffer(upload, buffer);
        buffer->Barrier(
            command,
            EPipelineStage::ComputeShader,
            EPipelineAccess::ShaderRead | EPipelineAccess::ShaderWrite
        );
        pipeline->Bind(command);
        shader->SetPushConstant(command, "pc", &pc, sizeof(pc));
        command->Dispatch(1);
        buffer->Barrier(command, EPipelineStage::Transfer, EPipelineAccess::TransferRead);
        buffer->Copy(command, readback);
        command->End();
        command->Flush();
        std::memcpy(points.data(), readback->Map(), sizeof(points));
        readback->Unmap();
    };
    dispatch();
    EXPECT_NEAR(points[1].PositionSize.x, 0, 0.00001f);
    EXPECT_NEAR(points[2].PositionSize.x, 0, 0.00001f);
    for (uint32_t slot = 0; slot < 8; ++slot)
    {
        const auto& point = points[slot + 1];
        EXPECT_EQ(point.Metadata, initial[slot + 1].Metadata);
        EXPECT_EQ(point.Color, initial[slot + 1].Color);
        EXPECT_EQ(point.VelocityAge.w, 3);
        EXPECT_EQ(point.PositionSize.w, 2);
        EXPECT_NEAR(point.PositionSize.y, float(slot % 2) * 10, 0.00001f);
        if (slot >= 2)
            EXPECT_NEAR(
                glm::length(glm::vec3(point.PositionSize - points[slot - 1].PositionSize)),
                1,
                0.025f
            );
        for (uint32_t axis = 0; axis < 3; ++axis)
        {
            EXPECT_TRUE(std::isfinite(point.PositionSize[axis]));
            EXPECT_TRUE(std::isfinite(point.VelocityAge[axis]));
        }
    }
    EXPECT_EQ(points.front().PositionSize, initial.front().PositionSize);
    EXPECT_EQ(points.back().PositionSize, initial.back().PositionSize);

    points = initial;
    pc.DeltaSeconds = 0;
    dispatch();
    EXPECT_EQ(std::memcmp(points.data(), initial.data(), sizeof(points)), 0);

    // Without a pinned root, internal constraints preserve the chain's center of mass.
    points = initial;
    pc.DeltaSeconds = 0.1f;
    pc.PinFirstParticle = 0;
    dispatch();
    float centerBefore = 0;
    float centerAfter = 0;
    for (uint32_t index = 0; index < 4; ++index)
    {
        centerBefore += initial[index * 2 + 1].PositionSize.x;
        centerAfter += points[index * 2 + 1].PositionSize.x;
    }
    EXPECT_NEAR(centerAfter, centerBefore, 0.00001f);
    EXPECT_GT(points[1].PositionSize.x, initial[1].PositionSize.x);
    pc.PinFirstParticle = 1;

    // A dead middle particle must split the chain and must remain dead.
    points = initial;
    points[5].Metadata.w = 0;
    pc.DeltaSeconds = 0.1f;
    dispatch();
    EXPECT_EQ(points[5].Metadata.w, 0);
    EXPECT_NEAR(points[7].PositionSize.x, initial[7].PositionSize.x - 0.01f, 0.00001f);

    // A reused slot with a different order is not the old particle's successor.
    points = initial;
    points[5].Metadata.y = std::bit_cast<float>(100u);
    dispatch();
    EXPECT_NEAR(points[7].PositionSize.x, initial[7].PositionSize.x - 0.01f, 0.00001f);
    // The same chain can cross the physical end of an emitter's ring allocation.
    points = initial;
    for (uint32_t slot = 0; slot < 8; ++slot)
        points[slot + 1].Metadata.w = 0;
    for (uint32_t index = 0; index < 4; ++index)
    {
        const auto slot = (6u + index) % 8u;
        auto& point = points[slot + 1];
        point.PositionSize = {float(index) * 1.2f + 0.01f, 0, 0, 2};
        point.Metadata = {0, std::bit_cast<float>(index + 4u), 10, 1};
        point.TangentGroup.w = 0;
    }
    pc.OrderStep = 1;
    dispatch();
    EXPECT_NEAR(points[7].PositionSize.x, 0, 0.00001f);
    for (uint32_t index = 1; index < 4; ++index)
    {
        const auto slot = (6u + index) % 8u;
        const auto previous = (5u + index) % 8u;
        EXPECT_NEAR(
            glm::length(glm::vec3(points[slot + 1].PositionSize -
                points[previous + 1].PositionSize)),
            1,
            0.025f
        );
    }
    // Exercise the largest supported chain and verify every particle is visited.
    pc.ParticleCount = 96;
    pc.MaxPoints = 96;
    for (uint32_t index = 0; index < 96; ++index)
    {
        auto& point = points[index + 1];
        point = SConnectionParticle{};
        point.PositionSize = {float(index) * 1.02f, 0, 0, 2};
        point.Metadata.y = std::bit_cast<float>(index);
    }
    dispatch();
    EXPECT_NEAR(points[1].PositionSize.x, 0, 0.00001f);
    for (uint32_t index = 1; index <= 96; ++index)
    {
        EXPECT_TRUE(std::isfinite(points[index].VelocityAge.x));
        EXPECT_NEAR(glm::length(glm::vec3(points[index].TangentGroup)), 1, 0.00001f);
    }

}

TEST(ParticlesConstraintsTest, TubeReadsParticlePositionsAndRejectsBrokenConnections)
{
    const auto context = VulkanTestContext::Get().GetGraphicsContext();
    ShaderLoader loader(context);
    const auto shader = loader.LoadShader(
        ELIXIR_TEST_SHADER_DIR,
        "ParticleStrandGeometryValidation",
        EShaderStage::Compute
    );
    ASSERT_TRUE(shader);
    const Aether::Simulation::SConstraintFrameData constraintData{};
    const auto constraintBuffer = UniformBuffer::Create(
        context,
        sizeof(constraintData),
        &constraintData
    );
    if (shader->HasBinding("cbConstraint"))
        shader->BindConstantBuffer("cbConstraint", constraintBuffer);
    const auto pipeline = ComputePipeline::Create(context, {.Shader = shader});
    ASSERT_TRUE(pipeline);
    struct SConstants
    {
        glm::mat4 WorldTransform{1};
        uint32_t ParticleOffset = 1;
        uint32_t ParticleCount = 4;
        uint32_t RadialSegments = 4;
        uint32_t Subdivisions = 1;
        float WidthScale = 1;
        uint32_t OrderStep = 1;
        uint32_t MaterialIndex = 0;
        uint32_t MaxPoints = 4;
        float RootWidthScale = 1;
        float TipWidthScale = 1;
        float WidthExponent = 1;
        float BranchWidthScale = 1;
        uint32_t HasGrowth = 0;
        uint32_t BranchCount = 0;
    } pc;
    static_assert(sizeof(SConstants) == 120);
    pc.WorldTransform[0].x = 2;
    pc.WorldTransform[1].y = 3;
    pc.WorldTransform[2].z = 4;
    std::array<SConnectionParticle, 6> points{};
    for (uint32_t slot = 0; slot < 4; ++slot)
    {
        points[slot + 1].PositionSize = {float(slot), 0, 0, 1};
        points[slot + 1].Metadata.y = std::bit_cast<float>(slot);
    }
    const auto input = StorageBuffer::Create(context, sizeof(points));
    const auto output = StorageBuffer::Create(context, 8 * sizeof(glm::vec4));
    auto info = StagingBuffer::CreateBufferInfo(8 * sizeof(glm::vec4), nullptr);
    info.Usage = EBufferUsage::TransferDst;
    const Ref<StagingBuffer> readback = CreateRef<VulkanStagingBuffer>(context, info);
    shader->BindStorageBuffer("particles", input);
    shader->BindStorageBuffer("results", output);
    shader->SetPushConstant("pc", &pc, sizeof(pc));
    std::array<glm::vec4, 8> result{};
    const auto dispatch = [&]()
    {
        const auto upload = StagingBuffer::Create(context, sizeof(points), points.data());
        const auto command = context->GetUploadCommandBuffer();
        command->Begin();
        input->Barrier(command, EPipelineStage::Transfer, EPipelineAccess::TransferWrite);
        command->CopyBuffer(upload, input);
        input->Barrier(command, EPipelineStage::ComputeShader, EPipelineAccess::ShaderRead);
        output->Barrier(command, EPipelineStage::ComputeShader, EPipelineAccess::ShaderWrite);
        pipeline->Bind(command);
        command->Dispatch(1);
        output->Barrier(command, EPipelineStage::Transfer, EPipelineAccess::TransferRead);
        output->Copy(command, readback);
        command->Flush();
        std::memcpy(result.data(), readback->Map(), sizeof(result));
        readback->Unmap();
    };
    dispatch();
    for (uint32_t slot = 0; slot < 3; ++slot)
    {
        EXPECT_EQ(result[slot * 2].w, 1);
        EXPECT_NEAR(result[slot * 2].x, float(slot) * 2, 0.00001f);
        EXPECT_NEAR(result[slot * 2].y, -0.0016f * 3, 0.00001f);
        EXPECT_NEAR(result[slot * 2 + 1].y, -1, 0.00001f);
    }
    EXPECT_EQ(result[6].w, 0);
    points[2].Metadata.w = 0;
    dispatch();
    EXPECT_EQ(result[0].w, 0);
    EXPECT_EQ(result[2].w, 0);
    EXPECT_EQ(result[4].w, 1);
    points[2].Metadata.w = 1;
    points[2].Metadata.y = std::bit_cast<float>(100u);
    dispatch();
    EXPECT_EQ(result[0].w, 0);
    EXPECT_EQ(result[2].w, 0);
    points[2].Metadata.y = std::bit_cast<float>(1u);
    points[2].TangentGroup.w = std::bit_cast<float>(9u);
    dispatch();
    EXPECT_EQ(result[0].w, 0);
    points[2].TangentGroup.w = 0;
    points[2].Metadata.x = std::bit_cast<float>(1u);
    dispatch();
    EXPECT_EQ(result[0].w, 0);
    for (uint32_t index = 0; index < 4; ++index)
    {
        const auto slot = (2u + index) % 4u;
        auto& point = points[slot + 1];
        point.PositionSize = {float(index), 0, 0, 1};
        point.Metadata = {0, std::bit_cast<float>(index + 4u), 10, 1};
        point.TangentGroup.w = 0;
    }
    dispatch();
    EXPECT_EQ(result[4].w, 1);
    EXPECT_EQ(result[6].w, 1);
    EXPECT_EQ(result[0].w, 1);
    EXPECT_EQ(result[2].w, 0);

}

TEST(ParticlesConstraintsTest, SolverAlternatesModulesForRibbonAndStrandEmitters)
{
    const auto context = VulkanTestContext::Get().GetGraphicsContext();
    ShaderLoader loader(context);
    std::string error;
    const auto system = Aether::Effect::ParseEffect(R"({
        "name":"Coupled constraints",
        "emitters":[{
            "name":"Coupled", "renderMode":"Ribbon", "maxParticles":2, "spawnRate":0,
            "burst":{"count":2,"interval":0},
            "spawnModules":[
                {"type":"SetLifetime","min":10,"max":10},
                {"type":"SetGroupIdFromSpawnOrder","groupCount":1,"firstGroupId":16777217}
            ],
            "solver":{"iterations":12},
            "updateModules":[
                {"type":"DistanceConstraint","maxPoints":2,
                 "segmentLength":1,"stiffness":0.5},
                {"type":"ApplyLinearDrag","drag":1},
                {"type":"DistanceConstraint","maxPoints":2,
                 "segmentLength":3,"stiffness":0.5}
            ]
        },{
            "name":"Single", "renderMode":"Strand", "maxParticles":2, "spawnRate":0,
            "burst":{"count":2,"interval":0},
            "spawnModules":[
                {"type":"SetLifetime","min":10,"max":10},
                {"type":"SetGroupIdFromSpawnOrder","groupCount":1,"firstGroupId":16777219}
            ],
            "solver":{"iterations":12},
            "updateModules":[
                {"type":"DistanceConstraint","maxPoints":2,
                 "segmentLength":1,"stiffness":0.5}
            ]
        }]
    })", &error);
    ASSERT_TRUE(system) << error;
    TestInstanceRegistry registry;
    const auto instance = registry.CreateRegisteredInstance(system);
    ASSERT_TRUE(instance);
    Aether::Rendering::FrameSubmission submission;
    ASSERT_TRUE(submission.Submit(*instance));
    Aether::Core::SResourcePoolLimits limits;
    limits.ParticleCapacity = 16;
    Aether::Simulation::Simulator simulator(context, &loader, limits);
    simulator.BeginFrame(Timestep(1.0f / 60));
    const auto command = context->GetUploadCommandBuffer();
    command->Begin();
    const auto frame = simulator.Simulate(submission, command);
    ASSERT_TRUE(frame);
    ASSERT_EQ(frame->GetItems().size(), 2u);
    ASSERT_EQ(frame->GetResources().size(), 1u);
    const auto& particles = frame->GetResources()[0].ParticleStateBuffer;
    std::array<SConnectionParticle, 4> result{};
    auto info = StagingBuffer::CreateBufferInfo(sizeof(result), nullptr);
    info.Usage = EBufferUsage::TransferDst;
    const Ref<StagingBuffer> readback = CreateRef<VulkanStagingBuffer>(context, info);
    std::array<SBufferCopy, 1> regions{{{
        .SrcOffset = frame->GetItems()[0].Allocation.Particles.Offset *
            sizeof(SConnectionParticle),
        .Size = sizeof(result),
    }}};
    particles->Barrier(command, EPipelineStage::Transfer, EPipelineAccess::TransferRead);
    particles->Copy(command, readback, regions);
    command->End();
    command->Flush();
    std::memcpy(result.data(), readback->Map(), sizeof(result));
    readback->Unmap();
    EXPECT_NEAR(result[0].PositionSize.y, 0, 0.00001f);
    EXPECT_NEAR(result[1].PositionSize.y, 7.0f / 3, 0.00001f);
    EXPECT_NEAR(result[2].PositionSize.y, 0, 0.00001f);
    EXPECT_NEAR(result[3].PositionSize.y, 1.0f - std::pow(0.5f, 12), 0.00001f);
    EXPECT_EQ(std::bit_cast<uint32_t>(result[0].TangentGroup.w), 16'777'217u);
    EXPECT_EQ(std::bit_cast<uint32_t>(result[1].TangentGroup.w), 16'777'217u);
    EXPECT_EQ(std::bit_cast<uint32_t>(result[2].TangentGroup.w), 16'777'219u);
    EXPECT_EQ(std::bit_cast<uint32_t>(result[3].TangentGroup.w), 16'777'219u);
    for (const auto& particle : result)
    {
        EXPECT_EQ(particle.Metadata.w, 1);
        EXPECT_NEAR(particle.VelocityAge.w, 1.0f / 60, 0.00001f);
    }
}

TEST(ParticlesConstraintsTest, DevelopsBranchesPausesAndPinsWorldAttachments)
{
    using namespace Aether::Simulation;
    const auto context = VulkanTestContext::Get().GetGraphicsContext();
    ShaderLoader loader(context);
    const auto shader = loader.LoadShader(
        "./Shaders/Aether",
        "ParticlesConstraints",
        EShaderStage::Compute
    );
    ASSERT_TRUE(shader);
    auto pipeline = ComputePipeline::Create(context, {.Shader = shader});
    ASSERT_TRUE(pipeline);
    static_assert(sizeof(SConstraintFrameData) == 608);
    static_assert(offsetof(SConstraintFrameData, Surfaces) == 224);
    std::array<SConnectionParticle, 14> points{};
    const auto buffer = StorageBuffer::Create(context, sizeof(points));
    auto info = StagingBuffer::CreateBufferInfo(sizeof(points), nullptr);
    info.Usage = EBufferUsage::TransferDst;
    const Ref<StagingBuffer> readback = CreateRef<VulkanStagingBuffer>(context, info);
    ASSERT_TRUE(buffer);
    ASSERT_TRUE(readback);
    shader->BindStorageBuffer("particles", buffer);
    SConstraintPushConstants pc;
    pc.ParticleCount = 12;
    pc.MaxPoints = 4;
    pc.OrderStep = 3;
    SDistanceConstraint config;
    config.MaxPoints = pc.MaxPoints;
    config.OrderStep = pc.OrderStep;
    config.SegmentLength = pc.SegmentLength;
    config.HasGrowth = true;
    config.Growth.Speed = 1;
    config.Growth.BranchCount = 2;
    config.Growth.BranchInterval = 1;
    config.Growth.BranchLengthScale = 0.5f;
    for (uint32_t slot = 0; slot < pc.ParticleCount; ++slot)
    {
        auto& particle = points[slot + 1];
        particle.PositionSize = {0, 0, 0, 2};
        particle.TangentGroup.w = std::bit_cast<float>(slot % pc.OrderStep);
        particle.Metadata.y = std::bit_cast<float>(slot);
    }
    auto data = BuildConstraintFrameData(config, glm::mat4(1));
    ASSERT_TRUE(data);
    const auto dispatch = [&]
    {
        const auto uniform = UniformBuffer::Create(context, sizeof(*data), &*data);
        ASSERT_TRUE(uniform);
        shader->BindConstantBuffer("cbConstraint", uniform);
        const auto upload = StagingBuffer::Create(context, sizeof(points), points.data());
        const auto command = context->GetUploadCommandBuffer();
        command->Begin();
        buffer->Barrier(command, EPipelineStage::Transfer, EPipelineAccess::TransferWrite);
        command->CopyBuffer(upload, buffer);
        buffer->Barrier(
            command,
            EPipelineStage::ComputeShader,
            EPipelineAccess::ShaderRead | EPipelineAccess::ShaderWrite
        );
        pipeline->Bind(command);
        shader->SetPushConstant(command, "pc", &pc, sizeof(pc));
        command->Dispatch(1);
        buffer->Barrier(command, EPipelineStage::Transfer, EPipelineAccess::TransferRead);
        buffer->Copy(command, readback);
        command->End();
        command->Flush();
        std::memcpy(points.data(), readback->Map(), sizeof(points));
        readback->Unmap();
    };
    for (int step = 0; step < 5; ++step) dispatch();
    EXPECT_NEAR(points[1].Transform.w, 0.5f, 0.0001f);
    EXPECT_EQ(points[2].Transform.w, 0);
    EXPECT_NEAR(points[4].Transform.z, 0.5f, 0.0001f);
    EXPECT_EQ(points[7].Transform.z, 0);
    data->Topology.w = uint32_t(Aether::Core::EStrandGrowthState::Paused);
    for (int step = 0; step < 3; ++step) dispatch();
    EXPECT_NEAR(points[1].Transform.w, 0.5f, 0.0001f);
    data->Topology.w = uint32_t(Aether::Core::EStrandGrowthState::Growing);
    for (int step = 0; step < 20; ++step) dispatch();
    EXPECT_NEAR(points[1].Transform.w, 2.5f, 0.0001f);
    EXPECT_GT(points[2].Transform.w, 0);
    EXPECT_NEAR(
        glm::length(glm::vec3(points[2].PositionSize - points[4].PositionSize)),
        0,
        0.0001f
    );
    EXPECT_NEAR(
        glm::length(glm::vec3(points[3].PositionSize - points[7].PositionSize)),
        0,
        0.0001f
    );
    for (uint32_t index = 1; index <= 12; ++index)
    {
        EXPECT_TRUE(std::isfinite(points[index].PositionSize.x));
        EXPECT_EQ(points[index].Metadata.y, std::bit_cast<float>(index - 1));
        EXPECT_EQ(points[index].Metadata.w, 1);
    }
    EXPECT_EQ(points[0].PositionSize, glm::vec4(0));
    EXPECT_EQ(points[13].PositionSize, glm::vec4(0));

    // Two fixed ends use simulation and world coordinates independently.
    pc.ParticleCount = 4;
    pc.OrderStep = 1;
    config.HasGrowth = false;
    config.OrderStep = 1;
    config.EndAttachment.Enabled = true;
    config.EndAttachment.Space = Aether::Core::EStrandEndSpace::World;
    config.EndAttachment.Position = {5, 2, 0};
    auto world = glm::translate(glm::mat4(1), glm::vec3(5, 0, 0));
    data = BuildConstraintFrameData(config, world);
    ASSERT_TRUE(data);
    for (uint32_t slot = 0; slot < 4; ++slot)
    {
        points[slot + 1] = SConnectionParticle{};
        points[slot + 1].Metadata.y = std::bit_cast<float>(slot);
    }
    dispatch();
    EXPECT_EQ(glm::vec3(points[1].PositionSize), glm::vec3(0));
    EXPECT_EQ(glm::vec3(points[4].PositionSize), glm::vec3(0, 2, 0));

    // Surface queries stay in world coordinates when the instance moves.
    config.EndAttachment.Enabled = false;
    config.SurfaceAttachment.Count = 1;
    auto& surface = config.SurfaceAttachment.Surfaces.front();
    surface.Type = Aether::Core::EStrandSurfaceType::Plane;
    surface.Center = {0, 0, 1};
    surface.Normal = {0, 0, 1};
    config.SurfaceAttachment.ClimbStrength = 0;
    world = glm::translate(glm::mat4(1), glm::vec3(0, 0, 1));
    data = BuildConstraintFrameData(config, world);
    ASSERT_TRUE(data);
    for (uint32_t slot = 0; slot < 4; ++slot)
    {
        points[slot + 1] = SConnectionParticle{};
        points[slot + 1].PositionSize = {float(slot), 0, 0.014f, 1};
        points[slot + 1].Metadata.y = std::bit_cast<float>(slot);
    }
    dispatch();
    for (uint32_t slot = 1; slot < 4; ++slot)
        EXPECT_NEAR(points[slot + 1].PositionSize.z, 0.014f, 0.0001f);

    pc.Stiffness = 0;
    config.SurfaceAttachment.Surfaces.front().Type = Aether::Core::EStrandSurfaceType::Sphere;
    config.SurfaceAttachment.Surfaces.front().Center = {};
    config.SurfaceAttachment.Surfaces.front().Radius = 1;
    data = BuildConstraintFrameData(config, glm::mat4(1));
    ASSERT_TRUE(data);
    for (uint32_t slot = 0; slot < 4; ++slot)
    {
        points[slot + 1].PositionSize = {1.03f, 0, 0, 1};
        points[slot + 1].VelocityAge = {};
    }
    dispatch();
    for (uint32_t slot = 1; slot < 4; ++slot)
        EXPECT_NEAR(points[slot + 1].PositionSize.x, 1.014f, 0.0001f);

    auto& box = config.SurfaceAttachment.Surfaces.front();
    box.Type = Aether::Core::EStrandSurfaceType::Box;
    box.HalfExtents = glm::vec3(1);
    box.Rounding = 0.2f;
    data = BuildConstraintFrameData(config, glm::mat4(1));
    ASSERT_TRUE(data);
    for (uint32_t slot = 0; slot < 4; ++slot)
    {
        points[slot + 1].PositionSize = {0.96f, 0.96f, 0, 1};
        points[slot + 1].VelocityAge = {};
    }
    dispatch();
    const float roundedCorner = 0.8f + 0.214f / std::sqrt(2.0f);
    for (uint32_t slot = 1; slot < 4; ++slot)
    {
        EXPECT_NEAR(points[slot + 1].PositionSize.x, roundedCorner, 0.0001f);
        EXPECT_NEAR(points[slot + 1].PositionSize.y, roundedCorner, 0.0001f);
    }
}
