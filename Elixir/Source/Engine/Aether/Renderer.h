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
        // Appended after the existing fields so shaders that only declare the
        // block above keep their offsets; the fog pass reconstructs world rays.
        glm::mat4 InvViewProj;
    };

    // Shared parameters for the froxel fog passes. Every field is a vec4 to keep
    // the std140 layout unambiguous across the compute and apply shaders.
    struct alignas(16) SFroxelData
    {
        glm::mat4 InvViewProj;
        glm::vec4 CameraPos;      // xyz, w = MaxDistance
        glm::vec4 FogAlbedo;      // xyz, w = Density
        glm::vec4 BoxCenter;      // xyz, w = SphereRadius
        glm::vec4 BoxHalfExtents; // xyz, w = EdgeFeather
        glm::vec4 SphereCenter;   // xyz, w = Time
        glm::vec4 LightDir;       // xyz, w = NoiseScale
        glm::vec4 LightColor;     // xyz, w = NoiseStrength
        glm::vec4 GridSize;       // x=W, y=H, z=D, w = Anisotropy
        // Appended (apply pass only reads up to GridSize, so its offset stays).
        glm::vec4 LightParams;    // x=DirIntensity, y=Ambient, z=ScatterStrength, w=PointCount
        glm::vec4 PointLight0PosRange; // xyz = position, w = range
        glm::vec4 PointLight0Color;    // xyz = color, w = intensity
        glm::vec4 PointLight1PosRange;
        glm::vec4 PointLight1Color;
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

    struct SEmitterState
    {
        float SpawnAccumulator = 0.0f;
        float BurstAccumulator = 0.0f;
        uint32_t BufferCursor = 0u;
        uint32_t EmissionIndex = 0u;
        bool Initialized = false;
    };

    class ELIXIR_API Renderer final
    {
      public:
        static constexpr uint32_t MAX_EMITTERS = 32;
        static constexpr uint32_t MAX_PARTICLES = 20000;
        static constexpr uint32_t MAX_OPS = 512;
        static constexpr uint32_t MAX_PARAMETERS = 512;
        static constexpr uint32_t COMPUTE_GROUP_SIZE = 256;

        // Froxel (frustum-voxel) grid for volumetric fog.
        static constexpr uint32_t FROXEL_W = 160;
        static constexpr uint32_t FROXEL_H = 90;
        static constexpr uint32_t FROXEL_D = 64;

        Renderer(const GraphicsContext* context, const ShaderLoader* shaderLoader);

        void Update(const Timestep& timestep);
        void Render(const SGPUSystem& system, const Camera& camera);

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

        Ref<Shader> m_FroxelBuildShader;
        Ref<ComputePipeline> m_FroxelBuildPipeline;
        Ref<Shader> m_SpriteShader;
        Ref<GraphicsPipeline> m_SpritePipeline;
        Ref<GraphicsPipeline> m_SpriteAdditivePipeline;
        Ref<Shader> m_DistortionShader;
        Ref<GraphicsPipeline> m_DistortionPipeline;
        Ref<Shader> m_BloomShader;
        Ref<GraphicsPipeline> m_BloomPipeline;
        Ref<Shader> m_FogShader;
        Ref<GraphicsPipeline> m_FogPipeline;
        Ref<Shader> m_RibbonShader;
        Ref<GraphicsPipeline> m_RibbonPipeline;
        Ref<Shader> m_MeshShader;
        Ref<GraphicsPipeline> m_MeshPipeline;

        Ref<StorageBuffer> m_FroxelBuffer;
        Ref<UniformBuffer> m_FroxelParamsBuffer;
        SFroxelData m_FroxelData{};

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

        bool m_CapacityErrorReported = false;

        Extent2D m_RenderExtent{};
        const GraphicsContext* m_GraphicsContext;
    };
}
