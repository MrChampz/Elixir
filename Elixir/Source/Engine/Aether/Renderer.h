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

    struct alignas(16) SParamsData
    {
        glm::vec4 SpawnCenterRadiusRate{};
        glm::vec4 AngleSpeed{};
        glm::vec4 LifetimeSize{};
        glm::vec4 GravityDragTime{};
        glm::vec4 BoundsMin{};
        glm::vec4 BoundsMax{};
        glm::vec4 ColorStart{};
        glm::vec4 ColorEnd{};
        glm::vec4 Meta{};
    };

    class ELIXIR_API Renderer final
    {
      public:
        static constexpr uint32_t MAX_PARTICLES = 10000;
        static constexpr uint32_t COMPUTE_GROUP_SIZE = 256;

        Renderer(const GraphicsContext* context, const ShaderLoader* shaderLoader);

        void Render(const SGPUSystem& system, const Camera& camera);

      private:
        void InitRenderPass(const ShaderLoader* shaderLoader);
        void InitPerFrameData();
        void BindShaderParameters() const;

        void BeginRendering(const Ref<CommandBuffer>& cmd) const;
        void EndRendering(const Ref<CommandBuffer>& cmd) const;

        void UpdateParamsBuffer(const SGPUSystem& system);

        SFrameData m_FrameData{};
        Ref<UniformBuffer> m_FrameConstantBuffer;

        struct alignas(16) SGPUParticleVertex
        {
            glm::vec4 PositionSize{};
            glm::vec4 Color{};
        };

        Ref<Shader> m_SimulationShader;
        Ref<ComputePipeline> m_SimulationPipeline;
        Ref<Shader> m_RendererShader;
        Ref<GraphicsPipeline> m_RendererPipeline;
        Ref<VertexBuffer> m_ParticleBuffer;
        Ref<UniformBuffer> m_ParamsBuffer;

        Extent2D m_RenderExtent{};
        const GraphicsContext* m_GraphicsContext;
    };
}