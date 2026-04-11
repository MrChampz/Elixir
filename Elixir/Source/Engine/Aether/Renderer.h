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
        glm::vec4 SpawnCenterRadiusRate{};
        glm::vec4 AngleSpeed{};
        glm::vec4 LifetimeSize{};
        glm::vec4 GravityDrag{};
        glm::vec4 BoundsMin{};
        glm::vec4 BoundsMax{};
        glm::vec4 ColorStart{};
        glm::vec4 ColorEnd{};
        glm::vec4 MetaA{};
        glm::vec4 MetaB{};
    };

    struct alignas(16) SParamsData
    {
        glm::vec4 Time;
        std::array<SEmitterData, 8> Emitters{};
    };

    struct SSpawnPushConstants
    {
        uint32_t EmitterIndex = 0;
    };

    class ELIXIR_API Renderer final
    {
      public:
        static constexpr uint32_t MAX_EMITTERS = 8;
        static constexpr uint32_t MAX_PARTICLES = 10000;
        static constexpr uint32_t COMPUTE_GROUP_SIZE = 256;

        Renderer(const GraphicsContext* context, const ShaderLoader* shaderLoader);

        void Update(const Timestep& timestep);
        void Render(const SGPUSystem& system, const Camera& camera);

      private:
        void InitRingBuffer();
        void InitRenderPass(const ShaderLoader* shaderLoader);
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
        Ref<StorageBuffer> m_ParticleBuffer;
        Ref<UniformBuffer> m_ParamsBuffer;

        std::vector<float> m_SpawnAccumulators;
        std::vector<uint32_t> m_SpawnCursors;

        float m_LastDeltaTimeSeconds = 0.0f;
        float m_ElapsedTimeSeconds = 0.0f;

        Extent2D m_RenderExtent{};
        const GraphicsContext* m_GraphicsContext;
    };
}