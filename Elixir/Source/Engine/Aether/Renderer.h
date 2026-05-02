#pragma once

#include <Engine/Aether/System.h>
#include <Engine/Camera/Camera.h>
#include <Engine/Graphics/Shader/ShaderLoader.h>

namespace Elixir::Aether
{
    struct SFrameData
    {
        glm::mat4 ViewProj;
    };

    struct alignas(16) SEmitterData
    {
        glm::vec4 MetaA{};
        glm::vec4 MetaB{};
        glm::vec4 MetaC{};
    };

    struct alignas(16) SModuleData
    {
        glm::vec4 Header{};
        glm::vec4 Data0{};
        glm::vec4 Data1{};
        glm::vec4 Data2{};
    };

    struct alignas(16) SParameterData
    {
        glm::vec4 Value{};
    };

    struct alignas(16) SParamsData
    {
        glm::vec4 Time;
    };

    struct SSpawnPushConstants
    {
        uint32_t EmitterIndex = 0;
    };

    struct SRibbonPushConstants
    {
        uint32_t ParticleOffset = 0;
        uint32_t MaxParticles = 0;
        uint32_t HeadIndex = 0;
        float WidthScale = 1.0f;
    };

    struct SEmitterState
    {
        float Accumulator = 0.0f;
        uint32_t BufferCursor = 0u;
        uint32_t SpawnedParticles = 0u;

        void Reset()
        {
            Accumulator = 0.0f;
            BufferCursor = 0u;
            SpawnedParticles = 0u;
        }
    };

    class ELIXIR_API Renderer final
    {
      public:
        static constexpr uint32_t MAX_EMITTERS = 8;
        static constexpr uint32_t MAX_PARTICLES = 10000;
        static constexpr uint32_t MAX_MODULES = 128;
        static constexpr uint32_t MAX_PARAMETERS = 64;
        static constexpr uint32_t COMPUTE_GROUP_SIZE = 256;

        Renderer(const GraphicsContext* context, const ShaderLoader* shaderLoader);

        void Update(const Timestep& timestep);
        void Render(const SGPUSystem& system, const Camera& camera);

      private:
        void InitEmitterState();
        void InitRenderPass(const ShaderLoader* shaderLoader);
        void CreateBuffers();
        void InitPerFrameData();
        void BindShaderParameters() const;

        void BeginRendering(const Ref<CommandBuffer>& cmd) const;
        void EndRendering(const Ref<CommandBuffer>& cmd) const;

        void UpdateParamsBuffer(const SGPUSystem& system);

        SFrameData m_FrameData{};
        Ref<UniformBuffer> m_FrameConstantBuffer;

        struct alignas(16) SGPUParticleState
        {
            glm::vec4 PositionSize{};
            glm::vec4 VelocityAge{};
            glm::vec4 Color{};
            glm::vec4 Metadata{};
        };

        Ref<Shader> m_SpawnShader;
        Ref<ComputePipeline> m_SpawnPipeline;
        Ref<Shader> m_UpdateShader;
        Ref<ComputePipeline> m_UpdatePipeline;
        Ref<Shader> m_RendererShader;
        Ref<GraphicsPipeline> m_RendererPipeline;
        Ref<Shader> m_RibbonShader;
        Ref<GraphicsPipeline> m_RibbonPipeline;

        Ref<StorageBuffer> m_ParticleBuffer;
        std::vector<SEmitterState> m_EmitterState;

        Ref<DynamicStorageBuffer> m_EmitterBuffer;
        Ref<DynamicStorageBuffer> m_ModuleBuffer;
        Ref<DynamicStorageBuffer> m_ParameterBuffer;
        Ref<UniformBuffer> m_ParamsBuffer;

        float m_LastDeltaTimeSeconds = 0.0f;
        float m_ElapsedTimeSeconds = 0.0f;

        Extent2D m_RenderExtent{};
        const GraphicsContext* m_GraphicsContext;
    };
}
