#pragma once

#include <Engine/Core/Timer.h>
#include <Engine/Aether/System.h>
#include <Engine/Camera/Camera.h>
#include <Engine/Graphics/Shader/ShaderLoader.h>

namespace Elixir::Aether
{
    struct alignas(16) SFrameData
    {
        glm::mat4 View;
        glm::mat4 Proj;
        glm::mat4 ViewProj;
        glm::vec3 CameraPos;
        float _Padding = 0.0f;
    };

    struct alignas(16) SEmitterData
    {
        glm::vec4 MetaA{};
        glm::vec4 MetaB{};
        glm::vec4 MetaC{};
        glm::vec4 MetaD{};
    };

    struct alignas(16) SParticleOpData
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
        glm::vec4 Time{};
        glm::vec4 Viewport{};
    };

    // CPU-side observability only. These values describe what the renderer
    // submitted for one Render() call; they do not read back GPU state.
    struct SParticleSubmissionMetrics
    {
        uint64_t SubmissionSerial = 0u;
        float DeltaTimeSeconds = 0.0f;
        float ElapsedTimeSeconds = 0.0f;

        size_t RequestedEmitterCount = 0u;
        size_t SubmittedEmitterCount = 0u;
        uint32_t RequestedParticleCapacity = 0u;
        uint32_t SubmittedParticleCapacity = 0u;

        uint32_t SpawnRequestCount = 0u;
        uint32_t TriggerEventsQueued = 0u;
        uint32_t TriggerEventsReleased = 0u;
        uint32_t TriggeredParticlesReleased = 0u;

        // This intentionally exposes the current leak baseline. It must stop
        // growing with unrelated effect loads once the GPU-only instance path
        // replaces m_EmittersState.
        size_t PersistentEmitterStateCount = 0;
    };

    struct SPendingEmitterBurst
    {
        float DelaySeconds = 0.0f;
        uint32_t Count = 0u;
    };

    struct SEmitterState
    {
        float SpawnAccumulator = 0.0f;
        float BurstAccumulator = 0.0f;
        std::vector<SPendingEmitterBurst> PendingEmitterBursts;
        uint32_t BufferCursor = 0u;
        uint32_t EmissionIndex = 0u;
    };

    class ELIXIR_API Renderer final
    {
      public:
        static constexpr uint32_t MAX_EMITTERS = 16;
        static constexpr uint32_t MAX_PARTICLES = 20000;
        static constexpr uint32_t MAX_OPS = 512;
        static constexpr uint32_t MAX_PARAMETERS = 128;
        static constexpr uint32_t COMPUTE_GROUP_SIZE = 256;

        Renderer(const GraphicsContext* context, const ShaderLoader* shaderLoader);

        void Update(const Timestep& timestep);
        void Render(const SGPUSystem& system, const Camera& camera);

        // Read only at the frame boundary after Render() returns.
        const SParticleSubmissionMetrics& GetLastSubmissionMetrics() const;

      private:
        void Init(const ShaderLoader* shaderLoader);
        void CreateBuffers();
        void CreateMeshVertexBuffer();
        void InitPerFrameData();
        void BindShaderParameters();

        uint32_t ResolveSpriteIndex(const Ref<Texture2D>& texture);

        void BeginRendering(const Ref<CommandBuffer>& cmd) const;
        void EndRendering(const Ref<CommandBuffer>& cmd) const;

        void UpdateBuffers(const SGPUSystem& system);

        SFrameData m_FrameData{};
        Ref<UniformBuffer> m_FrameConstantBuffer;

        struct alignas(16) SGPUParticleState
        {
            glm::vec4 PositionSize{};
            glm::vec4 VelocityAge{};
            glm::vec4 Transform{};
            glm::vec4 TangentRibbonId{};
            glm::vec4 Color{};
            glm::vec4 Metadata{};
        };

        Ref<Shader> m_SpawnShader;
        Ref<ComputePipeline> m_SpawnPipeline;
        Ref<Shader> m_UpdateShader;
        Ref<ComputePipeline> m_UpdatePipeline;
        Ref<Shader> m_SpriteShader;
        Ref<GraphicsPipeline> m_SpritePipeline;
        Ref<Shader> m_RibbonShader;
        Ref<GraphicsPipeline> m_RibbonPipeline;
        Ref<Shader> m_MeshShader;
        Ref<GraphicsPipeline> m_MeshPipeline;

        Ref<StorageBuffer> m_ParticleBuffer;
        std::unordered_map<SGPUEmitter, SEmitterState> m_EmittersState;

        Ref<DynamicStorageBuffer> m_EmitterBuffer;
        Ref<DynamicStorageBuffer> m_OpBuffer;
        Ref<DynamicStorageBuffer> m_ParameterBuffer;
        Ref<UniformBuffer> m_ParamsBuffer;

        Ref<TextureSet> m_Sprites;
        Ref<Sampler> m_SpriteSampler;
        std::unordered_map<Ref<Texture2D>, SResourceHandle> m_SpriteTextures;

        SResourceHandle m_WhiteTextureHandle{};

        uint32_t m_MeshVertexCount = 0;
        Ref<VertexBuffer> m_MeshVertexBuffer;

        float m_LastDeltaTimeSeconds = 0.0f;
        float m_ElapsedTimeSeconds = 0.0f;

        uint64_t m_SubmissionSerial = 0;
        SParticleSubmissionMetrics m_LastSubmissionMetrics{};

        bool m_CapacityErrorReported = false;

        Extent2D m_RenderExtent{};
        const GraphicsContext* m_GraphicsContext;
    };
}
