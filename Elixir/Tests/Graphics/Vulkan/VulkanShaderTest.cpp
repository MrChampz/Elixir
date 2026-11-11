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
#include <Engine/Aether/Modules/CurlNoiseForce.h>
#include <Engine/Aether/Modules/ApplyGravity.h>
#include <Engine/Materials/MaterialSystem.h>
#include <Engine/Materials/MaterialRegistry.h>
#include <Engine/Camera/PerspectiveCamera.h>
#include <Engine/Core/Timer.h>
#include <Engine/GUI/Renderer/DebugRenderPass.h>
#include <Engine/GUI/Renderer/QuadRenderPass.h>
#include <Engine/GUI/Renderer/TextRenderPass.h>
#include <chrono>
#include <filesystem>
#include <limits>
#include <Graphics/Vulkan/VulkanBuffer.h>



#include <Engine/Graphics/Texture.h>
#include <Engine/Graphics/Sampler.h>
#include <Engine/Graphics/TextureLoader.h>
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
            info.Compute->ConstantBuffers.emplace_back("Constants", 2, 0, 16);
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
        info.Compute->ConstantBuffers.emplace_back(
            "Constants",
            properties.limits.maxBoundDescriptorSets,
            0,
            16
        );
        const auto shader = CreateRef<VulkanShader>(context, std::move(info));
        EXPECT_FALSE(shader->IsValid());
        EXPECT_EQ(shader->GetPipelineLayout(), VK_NULL_HANDLE);
        EXPECT_TRUE(shader->GetDescriptorSets().empty());
        // Binding must stop before accessing command buffers or invalid native handles.
        shader->Bind(nullptr, nullptr);

        auto factoryInfo = MakeShaderInfo();
        factoryInfo.Compute->ConstantBuffers.emplace_back(
            "Constants",
            properties.limits.maxBoundDescriptorSets,
            0,
            16
        );
        EXPECT_EQ(Shader::Create(context, std::move(factoryInfo)), nullptr);
        EXPECT_NE(Shader::Create(context, MakeShaderInfo()), nullptr);
    }
}

namespace
{
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
        EXPECT_FALSE(compiled.Material->ParticleStrandShader->HasBinding("strandPoints"));
    }
}


TEST(VulkanShaderTest, StrandEffectSimulatesAndRecordsTubeDrawsThroughAether)
{
    const auto context = VulkanTestContext::Get().GetGraphicsContext();
    ShaderLoader loader(context);
    Materials::MaterialRegistry registry;
    Materials::MaterialSystem materials(context, {}, &loader,
        context->GetRenderTarget(), context->GetDepthStencilRenderTarget());
    Aether::Manager manager(context, &loader, registry, materials);
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
    EXPECT_EQ(result.DrawCount, 1u);
    EXPECT_EQ(result.MaterialCount, 1u);
    EXPECT_EQ(manager.GetLastSimulationMetrics().SubmittedEmitterCount, 1u);
    EXPECT_TRUE(manager.Remove(instance));
}

TEST(VulkanShaderTest, StrandSamplesCompileAndRenderWithMovingRoots)
{
    for (const auto* filename : {"StrandCables.json", "StrandSeaweed.json", "StrandEnergy.json"})
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
        EXPECT_EQ(result.DrawCount, 1u);
        EXPECT_EQ(result.MaterialCount, 1u);
        EXPECT_EQ(manager.GetLastSimulationMetrics().SubmittedEmitterCount, 1u);
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
    constexpr size_t burstDelayWord = 11;
    constexpr size_t burstIntervalWord = 12;
    constexpr size_t burstCountWord = 13;
    constexpr size_t triggerDrivenWord = 14;
    std::array<uint32_t, 16> emitter{};
    emitter[1] = 64; // particle capacity
    emitter[burstCountWord] = 64; // burst count
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

    // Reset reads the delay at byte 44 of the 64-byte GPU emitter layout.
    const auto init = loader.LoadShader(
        "./Shaders/Aether",
        "ParticlesSchedulerInitEmitters",
        EShaderStage::Compute
    );
    ASSERT_TRUE(init);
    const auto initPipeline = ComputePipeline::Create(context, {.Shader = init});
    init->SetPushConstant("pc", &index, sizeof(index));
    init->BindStorageBuffer("emitters", emitters);
    init->BindStorageBuffer("instances", instances);
    init->BindStorageBuffer("emitterStates", states);
    init->BindStorageBuffer("schedulerStates", schedulers);
    init->BindStorageBuffer("triggerQueueStates", queueBuffer);
    emitter[burstDelayWord] = std::bit_cast<uint32_t>(0.5f);
    emitters->UpdateData(emitter.data(), sizeof(emitter));
    scheduler[2] = 1;
    schedulers->UpdateData(scheduler.data(), sizeof(scheduler));
    const auto reset = context->GetUploadCommandBuffer();
    reset->Begin();
    states->Barrier(
        reset,
        EPipelineStage::ComputeShader,
        EPipelineAccess::ShaderRead | EPipelineAccess::ShaderWrite
    );
    initPipeline->Bind(reset);
    reset->Dispatch(1);
    reset->End();
    reset->Flush();
    scheduler[2] = 0;
    schedulers->UpdateData(scheduler.data(), sizeof(scheduler));
    // Delay counts down without consuming or repeating a one-shot burst.
    dispatch(0.25f);
    EXPECT_EQ(request[1], 0u);
    dispatch(0.25f);
    EXPECT_EQ(request[1], 64u);
    dispatch(1.0f);
    EXPECT_EQ(request[1], 0u);

    // A positive interval still waits for the interval and repeats.
    state = {};
    state[4] = 2;
    states->UpdateData(state.data(), sizeof(state));
    emitter[burstIntervalWord] = std::bit_cast<uint32_t>(0.5f);
    emitter[burstCountWord] = 2;
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
    emitter[burstIntervalWord] = 0;
    emitter[triggerDrivenWord] = 1;
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
    CurlNoiseForce(1, 2.2f, {0, 0, 0}, 17).Compile(compiler);
    ASSERT_EQ(program.Operations.size(), 13);
    std::array<glm::vec4, 30> roots{};
    std::array<uint32_t, 80> emittersData{};
    for (uint32_t index = 0; index < 5; ++index)
    {
        roots[index * 6] = {0, 0, 0, 16};
        roots[index * 6 + 1] = {1, 0, 0, 2}; // velocity and age greater than lifetime
        roots[index * 6 + 2].y = 1;
        roots[index * 6 + 4] = {1, 1, 1, 1};
        roots[index * 6 + 5] = {std::bit_cast<float>(index), 0, 0.25f, 1};
        emittersData[index * 16 + 1] = 1;
        // The lifetime flag follows trigger state at word 15 of the 64-byte layout.
        emittersData[index * 16 + 15] = index == 1 ? 1 : 0;
    }
    roots[2 * 6].x = 2; // explicit kill outside the box
    emittersData[2 * 16 + 4] = 0;
    emittersData[2 * 16 + 5] = 1;
    emittersData[3 * 16 + 4] = 1;
    emittersData[3 * 16 + 5] = 1;
    emittersData[4 * 16 + 4] = 2;
    emittersData[4 * 16 + 5] = 11;
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

namespace
{
    /** Uses an isolated working directory to test unavailable shader files. */
    class ScopedShaderDirectory final
    {
    public:
        /** @brief Creates and selects an empty temporary shader directory. */
        ScopedShaderDirectory() : m_Previous(std::filesystem::current_path())
        {
            const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
            m_Temporary = std::filesystem::temp_directory_path() /
                ("elixir-shader-failure-" + std::to_string(stamp));
            std::filesystem::create_directories(m_Temporary / "Shaders" / "Aether");
            std::filesystem::current_path(m_Temporary);
        }

        /** @brief Restores the working directory and removes the temporary files. */
        ~ScopedShaderDirectory()
        {
            std::error_code error;
            std::filesystem::current_path(m_Previous, error);
            std::filesystem::remove_all(m_Temporary, error);
        }

        /**
         * @brief Makes a shader available for the next initialization attempt.
         * @param name Name of the compute shader to copy from the build directory.
         */
        void CopyComputeShader(const std::string_view name) const
        {
            const auto filename = std::string(name) + ".cs.spirv";
            std::filesystem::copy_file(
                m_Previous / "Shaders" / "Aether" / filename,
                m_Temporary / "Shaders" / "Aether" / filename
            );
        }

    private:
        std::filesystem::path m_Previous;
        std::filesystem::path m_Temporary;
    };
}

TEST(VulkanShaderTest, MissingGUIShadersDisablePasses)
{
    const auto context = VulkanTestContext::Get().GetGraphicsContext();
    ShaderLoader loader(context);
    ScopedShaderDirectory directory;
    const Ref<UniformBuffer> perFrame;
    GUI::QuadRenderPass quads(context, &loader, 1.0f, perFrame);
    GUI::TextRenderPass text(context, &loader, 1.0f, perFrame);
    GUI::DebugRenderPass debug(context, &loader, 1.0f, perFrame);
    const std::array<GUI::RenderPass*, 3> passes{&quads, &text, &debug};
    const std::array<GUI::SDrawCommand, 1> commands{};
    for (auto* pass : passes)
    {
        pass->BeginFrame();
        EXPECT_EQ(pass->AppendRange(commands), 0u);
        pass->EndFrame();
        EXPECT_FALSE(pass->HasData());
        EXPECT_EQ(pass->GetInstanceCount(), 0u);
        pass->Bind(nullptr);
        pass->Render(nullptr, 0, 1);
    }
}

TEST(VulkanShaderTest, MissingSimulationShadersStopInitialization)
{
    const auto context = VulkanTestContext::Get().GetGraphicsContext();
    ShaderLoader loader(context);
    ScopedShaderDirectory directory;
    const std::array<std::string_view, 7> shaders{
        "ParticlesSchedulerBegin",
        "ParticlesSchedulerInitEmitters",
        "ParticlesSchedulerScheduleEmitters",
        "ParticlesSchedulerFinalize",
        "ParticlesSpawn",
        "ParticlesUpdate",
        "ParticlesConstraints"
    };
    const Aether::Rendering::FrameSubmission submission;
    const auto command = context->GetSecondaryCommandBuffer();
    ASSERT_TRUE(command);
    for (const auto name : shaders)
    {
        SCOPED_TRACE(name);
        {
            Aether::Simulation::Simulator simulator(context, &loader);
            simulator.BeginFrame(Timestep(0.016f));
            EXPECT_FALSE(simulator.Simulate(submission, command));
        }
        directory.CopyComputeShader(name);
    }
}

TEST(VulkanShaderTest, ConvertsRuntimeScalarParametersToUIntWithAndWithoutCache)
{
    const auto context = VulkanTestContext::Get().GetGraphicsContext();
    ShaderLoader loader(context);
    const auto shader = loader.LoadShader(
        ELIXIR_TEST_SHADER_DIR,
        "UIntParameterConversion",
        EShaderStage::Compute
    );
    ASSERT_TRUE(shader);
    const auto pipeline = ComputePipeline::Create(context, {.Shader = shader});
    ASSERT_TRUE(pipeline);
    using namespace Aether::Core;
    const auto scalar = EncodeOperandReference(
        EOperandSource::Parameter,
        EOperandFormat::Float,
        0
    );
    const auto integer = EncodeOperandReference(
        EOperandSource::Parameter,
        EOperandFormat::UInt,
        1
    );
    const std::array<uint32_t, 4> references{scalar, integer, integer, scalar};
    const auto operands = StorageBuffer::Create(
        context,
        sizeof(references),
        references.data()
    );
    const auto constants = StorageBuffer::Create(context, sizeof(uint32_t));
    const auto parameters = DynamicStorageBuffer::Create(context, 2 * sizeof(uint32_t));
    const auto output = StorageBuffer::Create(context, sizeof(references));
    auto info = StagingBuffer::CreateBufferInfo(sizeof(references), nullptr);
    info.Usage = EBufferUsage::TransferDst;
    const Ref<StagingBuffer> readback = CreateRef<VulkanStagingBuffer>(context, info);
    ASSERT_TRUE(operands && constants && parameters && output && readback);
    shader->BindStorageBuffer("operands", operands);
    shader->BindStorageBuffer("constants", constants);
    shader->BindStorageBuffer("parameters", parameters);
    shader->BindStorageBuffer("results", output);
    const std::array values{
        0.0f, 17.0f, 65537.0f, 16777216.0f, 4294967040.0f, -1.0f, 1.5f,
        4294967296.0f, std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::quiet_NaN()
    };
    for (const float value : values)
    {
        SCOPED_TRACE(value);
        const std::array<uint32_t, 2> words{std::bit_cast<uint32_t>(value), UINT32_MAX};
        parameters->UpdateData(words.data(), sizeof(words));
        const auto command = context->GetUploadCommandBuffer();
        command->Begin();
        parameters->Barrier(
            command,
            EPipelineStage::ComputeShader,
            EPipelineAccess::ShaderRead
        );
        output->Barrier(command, EPipelineStage::ComputeShader, EPipelineAccess::ShaderWrite);
        pipeline->Bind(command);
        command->Dispatch(1);
        output->Barrier(command, EPipelineStage::Transfer, EPipelineAccess::TransferRead);
        output->Copy(command, readback);
        command->Flush();
        std::array<uint32_t, 4> result{};
        std::memcpy(result.data(), readback->Map(), sizeof(result));
        readback->Unmap();
        const auto expected = IsValidUIntScalar(value) ? static_cast<uint32_t>(value) : 0u;
        EXPECT_EQ(result[0], expected);
        EXPECT_EQ(result[1], UINT32_MAX);
        EXPECT_EQ(result[2], UINT32_MAX);
        EXPECT_EQ(result[3], expected);
    }
}

TEST(VulkanShaderTest, ComposedCurlNoisePreservesForceAndRuntimeBindings)
{
    const auto context = VulkanTestContext::Get().GetGraphicsContext();
    ShaderLoader loader(context);
    const auto shader = loader.LoadShader(
        ELIXIR_TEST_SHADER_DIR,
        "ComposedCurlNoise",
        EShaderStage::Compute
    );
    ASSERT_TRUE(shader);
    const auto pipeline = ComputePipeline::Create(context, {.Shader = shader});
    ASSERT_TRUE(pipeline);
    using namespace Aether::Core;
    using namespace Aether::Modules;
    const std::vector<SParameter> authored{
        {"Strength", EParameterType::Float, glm::vec4(1.7f, 0, 0, 0)},
        {"Frequency", EParameterType::Float, glm::vec4(2.2f, 0, 0, 0)},
        {"Scroll", EParameterType::Float3, glm::vec4(0.12f, 0.09f, 0.07f, 0)},
        {"Seed", EParameterType::UInt, glm::vec4(std::bit_cast<float>(UINT32_MAX), 0, 0, 0)}
    };
    const auto definitions = CompileParameters(authored);
    auto words = PackParameterValues(definitions);
    SCompiledModuleData program;
    ModuleCompileContext compiler(program, definitions, "Curl", 1.0f);
    CurlNoiseForce curl;
    curl.BindParameters("Strength", "Frequency", "Scroll", "Seed");
    curl.Compile(compiler);
    ASSERT_TRUE(compiler.IsValid());
    const auto operations = StorageBuffer::Create(
        context,
        program.Operations.size() * sizeof(SGPUOperation),
        program.Operations.data()
    );
    const auto operands = StorageBuffer::Create(
        context,
        program.Operands.size() * sizeof(uint32_t),
        program.Operands.data()
    );
    const auto constants = StorageBuffer::Create(
        context,
        program.Constants.size() * sizeof(uint32_t),
        program.Constants.data()
    );
    const auto parameters = DynamicStorageBuffer::Create(
        context,
        words.size() * sizeof(uint32_t)
    );
    const auto output = StorageBuffer::Create(context, 3 * sizeof(glm::vec4));
    auto info = StagingBuffer::CreateBufferInfo(3 * sizeof(glm::vec4), nullptr);
    info.Usage = EBufferUsage::TransferDst;
    const Ref<StagingBuffer> readback = CreateRef<VulkanStagingBuffer>(context, info);
    shader->BindStorageBuffer("operations", operations);
    shader->BindStorageBuffer("operands", operands);
    shader->BindStorageBuffer("constants", constants);
    shader->BindStorageBuffer("parameters", parameters);
    shader->BindStorageBuffer("results", output);
    const std::array values{
        glm::vec2(1.7f, 2.2f),
        glm::vec2(0.0f, 0.0f),
        glm::vec2(-2.0f, -3.0f),
        glm::vec2(1.7f, -3.0f),
        glm::vec2(0.65f, 0.0000001f)
    };
    for (uint32_t sample = 0; sample < values.size() * 2; ++sample)
    {
        const auto value = values[sample % values.size()];
        words[5] = sample < values.size() ? 17u : UINT32_MAX;
        SCOPED_TRACE(sample);
        words[0] = std::bit_cast<uint32_t>(value.x);
        words[1] = std::bit_cast<uint32_t>(value.y);
        parameters->UpdateData(words.data(), words.size() * sizeof(uint32_t));
        const auto command = context->GetUploadCommandBuffer();
        command->Begin();
        parameters->Barrier(
            command,
            EPipelineStage::ComputeShader,
            EPipelineAccess::ShaderRead
        );
        output->Barrier(command, EPipelineStage::ComputeShader, EPipelineAccess::ShaderWrite);
        pipeline->Bind(command);
        command->Dispatch(1);
        output->Barrier(command, EPipelineStage::Transfer, EPipelineAccess::TransferRead);
        output->Copy(command, readback);
        command->Flush();
        std::array<glm::vec4, 3> result{};
        std::memcpy(result.data(), readback->Map(), sizeof(result));
        readback->Unmap();
        for (uint32_t component = 0; component < 4; ++component)
            EXPECT_NEAR(result[0][component], result[1][component], 0.0001f);
        EXPECT_EQ(result[2], glm::vec4(0.2f, 0.3f, 0.4f, 0.0f));
    }
}

TEST(VulkanShaderTest, StrandRendererUsesDefaultsWithoutAuthoredRenderConfig)
{
    const auto context = VulkanTestContext::Get().GetGraphicsContext();
    ShaderLoader loader(context);
    Materials::MaterialRegistry registry;
    Materials::MaterialSystem materials(
        context,
        {},
        &loader,
        context->GetRenderTarget(),
        context->GetDepthStencilRenderTarget()
    );
    Aether::Manager manager(context, &loader, registry, materials);
    const auto system = CreateRef<Aether::System>("Default strand configuration");
    auto& emitter = system->AddEmitter("Particles", 2, 0);
    emitter.SetRenderMode(Aether::Core::EParticleRenderMode::Strand);
    emitter.SetBurst(2, 0);
    ASSERT_FALSE(emitter.GetRenderConfig());
    const auto instance = system->CreateInstance();
    ASSERT_TRUE(manager.Add(instance));
    PerspectiveCamera camera(45, 1, 0.01f, 100);
    Materials::Rendering::SRenderResult result;
    context->RenderFrame([&]()
    {
        materials.BeginFrame();
        manager.BeginFrame(Timestep(1.0f / 60));
        manager.Render(camera, context->GetRenderTarget());
        result = materials.RenderFrame();
    });
    ASSERT_TRUE(context->RunRenderTaskAndWait([] {}));
    context->WaitDeviceIdle();
    EXPECT_EQ(result.DrawCount, 1u);
    EXPECT_TRUE(manager.Remove(instance));
}


TEST(VulkanShaderTest, AtlasSamplingClampsCellsAndInterpolatesWithoutBleeding)
{
    const auto context = VulkanTestContext::Get().GetGraphicsContext();
    ShaderLoader loader(context);
    const auto shader = loader.LoadShader(
        ELIXIR_TEST_SHADER_DIR,
        "TextureAtlasValidation",
        EShaderStage::Compute
    );
    ASSERT_TRUE(shader);
    const std::array<uint8_t, 16> pixels{
        255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255
    };
    STexture2DCreateInfo info;
    info.Format = EImageFormat::R8G8B8A8_UNORM;
    info.Width = info.Height = 2;
    info.InitialData = pixels.data();
    const auto texture = Texture2D::Create(context, info);
    const auto sampler = Sampler::Create(context, {
        .MagFilter = ESamplerFilter::Linear,
        .MinFilter = ESamplerFilter::Linear,
        .AddressModeU = ESamplerAddressMode::ClampToEdge,
        .AddressModeV = ESamplerAddressMode::ClampToEdge
    });
    std::array<glm::vec4, 8> values{};
    const auto output = DynamicStorageBuffer::Create(context, sizeof(values), values.data());
    shader->BindImage("atlas", texture->GetImage());
    shader->BindSampler("linearClamp", sampler);
    shader->BindStorageBuffer("results", output);
    const auto pipeline = ComputePipeline::Create(context, {.Shader = shader});
    const auto cmd = context->GetUploadCommandBuffer();
    cmd->Begin();
    pipeline->Bind(cmd);
    cmd->Dispatch(1);
    const VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT};
    vkCmdPipelineBarrier(
        static_cast<VulkanCommandBuffer*>(cmd.get())->GetVulkanCommandBuffer(),
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_HOST_BIT,
        0,
        1,
        &barrier,
        0,
        nullptr,
        0,
        nullptr
    );
    cmd->End();
    cmd->Flush();
    std::memcpy(values.data(), output->Map(), sizeof(values));
    output->Unmap();
    const std::array<glm::vec4, 8> expected{
        glm::vec4{1, 0, 0, 1}, glm::vec4{0, 1, 0, 1}, glm::vec4{0, 0, 1, 1},
        glm::vec4{1, 1, 1, 1}, glm::vec4{0.5f, 0.5f, 0, 1}, glm::vec4{1, 0, 0, 1},
        glm::vec4{1, 1, 1, 1}, glm::vec4{1, 0, 0, 1}
    };
    for (size_t sample = 0; sample < values.size(); ++sample)
        for (uint32_t component = 0; component < 4; ++component)
            EXPECT_NEAR(values[sample][component], expected[sample][component], 0.001f)
                << "sample " << sample;
}


TEST(VulkanShaderTest, MigratedEffectsRenderAdditiveAndRefractiveMaterialsThroughAether)
{
    const auto context = VulkanTestContext::Get().GetGraphicsContext();
    ShaderLoader loader(context);
    TextureLoader::Initialize(context);
    const auto root = std::filesystem::path(__FILE__).parent_path().parent_path()
        .parent_path().parent_path().parent_path();
    for (const auto* name : {"DistortionTest", "NS_Explosion_01", "CinematicFX_Showcase"})
    {
        Materials::MaterialRegistry registry;
        Materials::MaterialSystem materials(
            context,
            {},
            &loader,
            context->GetRenderTarget(),
            context->GetDepthStencilRenderTarget()
        );
        Aether::Manager manager(context, &loader, registry, materials);
        const auto system = Aether::Manager::LoadEffect(
            root / "Assets/VFX" / (std::string(name) + ".json")
        );
        ASSERT_TRUE(system) << name;
        for (const auto& emitter : system->GetEmitters())
        {
            auto description = *emitter->GetMaterialDescription();
            for (auto* path : {&description.BaseColorTexturePath,
                &description.EmissiveTexturePath, &description.ColorRampTexturePath,
                &description.NormalTexturePath, &description.RefractionTexturePath})
                if (!path->empty()) *path = (root / *path).string();
            emitter->SetMaterialDescription(description);
        }
        const auto instance = system->CreateInstance();
        ASSERT_TRUE(manager.Add(instance));
        PerspectiveCamera camera(45, 16.0f / 9, 0.01f, 100);
        camera.SetPosition({0, 0, 4});
        Materials::Rendering::SRenderResult result;
        // Run beyond the authored three-second burst intervals.
        for (int frame = 0; frame < 100; ++frame)
        {
            context->RenderFrame([&]
            {
                materials.BeginFrame();
                manager.BeginFrame(Timestep(1.0f / 30));
                manager.Render(camera, context->GetRenderTarget());
                result = materials.RenderFrame();
            });
            ASSERT_TRUE(context->RunRenderTaskAndWait([] {}));
        }
        context->WaitDeviceIdle();
        EXPECT_EQ(result.DrawCount, system->GetEmitters().size()) << name;
        EXPECT_EQ(result.MaterialCount, system->GetEmitters().size()) << name;
        EXPECT_TRUE(manager.Remove(instance));
    }
}

TEST(VulkanShaderTest, SurfaceAndParticleDirectLightingUsesTheSharedBRDF)
{
    const auto context = VulkanTestContext::Get().GetGraphicsContext();
    ShaderLoader loader(context);
    const auto shader = loader.LoadShader(
        ELIXIR_TEST_SHADER_DIR,
        "DirectLightingValidation",
        EShaderStage::Compute
    );
    ASSERT_TRUE(shader);
    std::array<glm::vec4, 4> values{};
    const auto output = DynamicStorageBuffer::Create(context, sizeof(values), values.data());
    shader->BindStorageBuffer("results", output);
    const auto pipeline = ComputePipeline::Create(context, {.Shader = shader});
    const auto cmd = context->GetUploadCommandBuffer();
    cmd->Begin();
    pipeline->Bind(cmd);
    cmd->Dispatch(1);
    const VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT};
    vkCmdPipelineBarrier(
        static_cast<VulkanCommandBuffer*>(cmd.get())->GetVulkanCommandBuffer(),
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_HOST_BIT,
        0,
        1,
        &barrier,
        0,
        nullptr,
        0,
        nullptr
    );
    cmd->End();
    cmd->Flush();
    std::memcpy(values.data(), output->Map(), sizeof(values));
    output->Unmap();
    for (uint32_t component = 0; component < 3; ++component)
    {
        EXPECT_NEAR(values[0][component], 0.96f / 3.14159265359f, 0.0001f);
        EXPECT_NEAR(values[1][component], 0.05093f, 0.0001f);
        EXPECT_FLOAT_EQ(values[2][component], 0);
        EXPECT_FLOAT_EQ(values[3][component], 0);
    }
}
