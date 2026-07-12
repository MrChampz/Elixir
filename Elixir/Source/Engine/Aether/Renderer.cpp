#include "epch.h"
#include "Renderer.h"

#include "Engine/Core/Color.h"
#include "Engine/Graphics/CommandBuffer.h"
#include "Engine/Graphics/SamplerBuilder.h"
#include "Engine/Graphics/Pipeline/PipelineBuilder.h"

namespace Elixir::Aether
{
    struct MeshVertex
    {
        glm::vec3 Position;
        glm::vec3 Normal;
    };

    struct SSpawnPushConstants
    {
        uint32_t EmitterIndex = 0;
    };

    struct SSpritePushConstants
    {
        uint32_t SpriteIndex = 0;
        uint32_t FlipbookCols = 1;
        uint32_t FlipbookRows = 1;
        uint32_t FlipbookFrames = 1;
        uint32_t FlipbookBlend = 0;
        uint32_t GradientIndex = 0;
        uint32_t GradientMode = 0; // 0 = off, 1 = remap sheet luminance through the LUT
        uint32_t NormalIndex = 0;
        uint32_t NormalMode = 0;   // 0 = unlit, 1 = fake-normal lighting
        uint32_t EmissionIndex = 0;
        float EmissionScale = 0.0f; // 0 = no emission map
    };

    struct SDistortPushConstants
    {
        uint32_t DistortIndex = 0;
        float Strength = 0.02f;
        float ScrollX = 0.0f;
        float ScrollY = 0.0f;
        float ViewW = 1.0f;
        float ViewH = 1.0f;
    };

    struct SBloomPushConstants
    {
        float Threshold = 0.6f;
        float Intensity = 1.4f;
        float Radius = 3.0f;
        float ViewW = 1.0f;
        float ViewH = 1.0f;
    };


    struct SRibbonPushConstants
    {
        uint32_t EmitterIndex = 0;
    };

    SEmitterData ToEmitterDescription(const SGPUEmitter& emitter)
    {
        SEmitterData desc{};
        desc.MetaA = {
            emitter.ParticleOffset,
            emitter.MaxParticles,
            emitter.SpawnOpOffset,
            emitter.SpawnOpCount
        };

        desc.MetaB = {
            emitter.UpdateOpOffset,
            emitter.UpdateOpCount,
            0.0,
            0.0
        };

        desc.MetaC = {
            (float)emitter.RenderMode,
            emitter.SpawnRatePerSecond,
            emitter.GravityScale,
            0.0f
        };

        desc.MetaD = { 0.0f, 0.0f, 0.f, 0.0f };

        return desc;
    }

    SParticleOpData ToOpDescription(const SGPUParticleOp& op)
    {
        SParticleOpData desc{};

        desc.Header = {
            (float)(uint32_t)op.Type,
            (float)op.Target,
            op.Parameter0Index == UINT32_MAX ? -1.0f : (float)op.Parameter0Index,
            op.Parameter1Index == UINT32_MAX ? -1.0f : (float)op.Parameter1Index
        };

        desc.Data0 = op.Data0;
        desc.Data1 = op.Data1;
        desc.Data2 = op.Data2;

        return desc;
    }

    SParameterData ToParameterDescription(const SGPUParameter& parameter)
    {
        SParameterData desc{};
        desc.Value = parameter.Value;

        return desc;
    }

    Renderer::Renderer(const GraphicsContext* context, const ShaderLoader* shaderLoader)
        : m_GraphicsContext(context)
    {
        EE_CORE_INFO("Initializing Aether Renderer.")

        Init(shaderLoader);
        CreateBuffers();
        InitPerFrameData();
        BindShaderParameters();
    }

    void Renderer::Update(const Timestep& timestep)
    {
        m_LastDeltaTimeSeconds = timestep.GetSeconds();
        m_ElapsedTimeSeconds += timestep.GetSeconds();
    }

    void Renderer::Render(const SGPUSystem& system, const Camera& camera)
    {
        m_RenderExtent = m_GraphicsContext->GetRenderTarget()->GetExtent();

        m_FrameData.View = camera.GetViewMatrix();
        m_FrameData.Proj = camera.GetProjectionMatrix();
        m_FrameData.ViewProj = camera.GetViewProjectionMatrix();
        m_FrameData.CameraPos = camera.GetPosition();
        m_FrameData.InvViewProj = glm::inverse(m_FrameData.ViewProj);
        m_FrameConstantBuffer->UpdateData(&m_FrameData, sizeof(SFrameData));

        // Large backlit haze volume: the setting where directional god-ray
        // shafts actually read (a big foggy space lit from behind).
        m_FroxelData.InvViewProj = m_FrameData.InvViewProj;
        m_FroxelData.CameraPos = glm::vec4(camera.GetPosition(), 45.0f);        // w = MaxDistance
        m_FroxelData.FogAlbedo = glm::vec4(0.70f, 0.72f, 0.78f, 0.28f);         // haze, w = Density
        m_FroxelData.BoxCenter = glm::vec4(0.0f, 0.0f, -2.0f, 0.01f);           // big box; w = SphereRadius (~off)
        m_FroxelData.BoxHalfExtents = glm::vec4(16.0f, 8.0f, 9.0f, 2.5f);       // fills view, w = EdgeFeather
        m_FroxelData.SphereCenter = glm::vec4(0.0f, -1000.0f, 0.0f, m_ElapsedTimeSeconds); // sphere out of the way
        m_FroxelData.LightDir = glm::vec4(glm::normalize(glm::vec3(-0.35f, 0.45f, -0.8f)), 0.18f); // backlit, w = NoiseScale
        m_FroxelData.LightColor = glm::vec4(1.0f, 0.95f, 0.82f, 0.7f);          // warm sun, w = NoiseStrength
        m_FroxelData.GridSize = glm::vec4((float)FROXEL_W, (float)FROXEL_H, (float)FROXEL_D, 0.55f); // w = Anisotropy

        // x=DirIntensity, y=Ambient, z=ScatterStrength, w=PointLightCount
        m_FroxelData.LightParams = glm::vec4(2.5f, 0.05f, 3.4f, 1.0f);
        // Warm point light glowing on the left of the haze.
        m_FroxelData.PointLight0PosRange = glm::vec4(-6.0f, 0.5f, -2.0f, 9.0f);
        m_FroxelData.PointLight0Color = glm::vec4(1.0f, 0.55f, 0.25f, 8.0f); // w = intensity
        m_FroxelData.PointLight1PosRange = glm::vec4(0.0f);
        m_FroxelData.PointLight1Color = glm::vec4(0.0f);
        // Cool spot light from the upper right, aimed down into the fog -> a
        // visible cone/beam of light.
        m_FroxelData.SpotPosRange = glm::vec4(6.0f, 6.0f, -1.0f, 16.0f);
        m_FroxelData.SpotDir = glm::vec4(glm::normalize(glm::vec3(-0.3f, -1.0f, -0.2f)), 0.90f); // w = cos(outer)
        m_FroxelData.SpotColor = glm::vec4(0.45f, 0.65f, 1.0f, 16.0f); // w = intensity
        // Emissive cube (prototype of a material emissive channel): the fog glows
        // green inside it.
        m_FroxelData.EmissiveCubeCenter = glm::vec4(0.0f, -2.0f, -2.0f, 0.0f);
        m_FroxelData.EmissiveCubeHalfExtents = glm::vec4(2.2f, 2.2f, 2.2f, 1.6f); // w = halo radius
        m_FroxelData.EmissiveCubeColor = glm::vec4(0.20f, 1.0f, 0.45f, 1.5f);     // w = intensity

        m_FroxelParamsBuffer->UpdateData(&m_FroxelData, sizeof(SFroxelData));

        // Volumetric clouds: a high slab of raymarched cumulus lit by the sun.
        const glm::vec3 sunDir = glm::normalize(glm::vec3(0.35f, 0.55f, 0.30f));
        m_CloudData.InvViewProj = m_FrameData.InvViewProj;
        m_CloudData.CameraPos = glm::vec4(m_FrameData.CameraPos, m_ElapsedTimeSeconds);
        m_CloudData.SunDir = glm::vec4(sunDir, 0.5f);                        // w = phase g (forward)
        m_CloudData.SunColor = glm::vec4(1.0f, 0.95f, 0.84f, 20.0f);         // w = intensity
        m_CloudData.SkyZenith = glm::vec4(0.24f, 0.46f, 0.78f, 38.0f);       // w = cloud bottom
        m_CloudData.SkyHorizon = glm::vec4(0.74f, 0.81f, 0.86f, 108.0f);     // w = cloud top
        m_CloudData.CloudParams = glm::vec4(1.0f, 1.0f, 0.028f, 1.5f);       // coverage, density, baseScale, wind
        m_CloudData.CloudParams2 = glm::vec4(0.17f, 0.5f, 48.0f, 6.0f);      // detailScale, detailStr, steps, lightSteps
        m_CloudData.CloudParams3 = glm::vec4(0.006f, 0.3f, 1.5f, 1.25f);     // coverageScale, powder, ambient, exposure
        m_CloudParamsBuffer->UpdateData(&m_CloudData, sizeof(SCloudData));

        const auto emitterCount = std::min((uint32_t)system.Emitters.size(), MAX_EMITTERS);
        const auto maxParticles = std::min(system.TotalMaxParticles, MAX_PARTICLES);

        UpdateBuffers(system);

        const auto cmd = m_GraphicsContext->GetSecondaryCommandBuffer();
        cmd->Begin({
            .ColorAttachment = m_GraphicsContext->GetRenderTarget(),
            .DepthStencilAttachment = m_GraphicsContext->GetDepthStencilRenderTarget(),
            .RenderArea = m_RenderExtent
        });

        const auto* emitters = static_cast<const SEmitterData*>(m_EmitterBuffer->Map());

        m_ParticleBuffer->Barrier(
            cmd,
            EPipelineStage::ComputeShader,
            EPipelineAccess::ShaderRead | EPipelineAccess::ShaderWrite
        );

        m_SpawnPipeline->Bind(cmd);
        for (uint32_t i = 0; i < emitterCount; ++i)
        {
            const auto offset = (uint32_t)emitters[i].MetaA.x;
            if (offset >= MAX_PARTICLES) continue;

            const auto particleCount = std::min((uint32_t)emitters[i].MetaA.y, MAX_PARTICLES - offset);
            if (particleCount == 0) continue;

            const SSpawnPushConstants pushConstants{ i };
            m_SpawnShader->SetPushConstant(cmd, "pc", (void*)&pushConstants, sizeof(SSpawnPushConstants));
            cmd->Dispatch((particleCount + COMPUTE_GROUP_SIZE - 1) / COMPUTE_GROUP_SIZE);
        }

        m_ParticleBuffer->Barrier(
            cmd,
            EPipelineStage::ComputeShader,
            EPipelineAccess::ShaderRead | EPipelineAccess::ShaderWrite
        );

        m_UpdatePipeline->Bind(cmd);
        cmd->Dispatch((maxParticles + COMPUTE_GROUP_SIZE - 1) / COMPUTE_GROUP_SIZE);

        // Build the froxel fog grid (density + integrated in-scattering), then
        // make it readable by the fog apply pass' fragment shader.
        m_FroxelBuildPipeline->Bind(cmd);
        cmd->Dispatch((FROXEL_W + 7) / 8, (FROXEL_H + 7) / 8, 1);
        m_FroxelBuffer->Barrier(
            cmd,
            EPipelineStage::PixelShader,
            EPipelineAccess::ShaderRead
        );

        // Particle buffer for the sprite draws.
        m_ParticleBuffer->Barrier(
            cmd,
            EPipelineStage::VertexShader | EPipelineStage::VertexInput,
            EPipelineAccess::ShaderRead | EPipelineAccess::VertexAttributeRead
        );

        BeginRendering(cmd);

        const GraphicsPipeline* boundSpritePipeline = nullptr;
        bool ribbonPipelineBound = false;

        for (uint32_t i = 0; i < emitterCount; ++i)
        {
            const auto& emitter = system.Emitters[i];

            if (emitter.RenderMode != EParticleRenderMode::Mesh || emitter.MaxParticles == 0)
                continue;

            m_MeshPipeline->Bind(cmd);
            m_MeshVertexBuffer->Bind(cmd);
            // TODO: Enhance this api
            m_ParticleBuffer->BindAs<VertexBuffer>(cmd, std::span<uint64_t>{}, 1, 1);

            cmd->Draw(
                m_MeshVertexCount,
                emitter.MaxParticles,
                0,
                emitter.ParticleOffset
            );
        }

        for (uint32_t i = 0; i < emitterCount; ++i)
        {
            const auto& emitter = system.Emitters[i];

            if (emitter.RenderMode == EParticleRenderMode::Mesh || emitter.MaxParticles == 0)
                continue;

            if (emitter.RenderMode == EParticleRenderMode::Ribbon)
            {
                if (!ribbonPipelineBound)
                {
                    m_RibbonPipeline->Bind(cmd);
                    ribbonPipelineBound = true;
                    boundSpritePipeline = nullptr;
                }

                const SRibbonPushConstants pc{ i };
                m_RibbonShader->SetPushConstant(cmd, "pc", (void*)&pc, sizeof(SRibbonPushConstants));

                cmd->Draw(emitter.MaxParticles * 6);
                continue;
            }

            // Distortion emitters read the scene behind them, so they are drawn
            // in a separate pass after the scene-color grab (see below).
            if (emitter.DistortionTexture)
                continue;

            const auto& spritePipeline = emitter.BlendMode == EParticleBlendMode::Additive
                ? m_SpriteAdditivePipeline
                : m_SpritePipeline;

            if (boundSpritePipeline != spritePipeline.get())
            {
                spritePipeline->Bind(cmd);
                m_ParticleBuffer->BindAs<VertexBuffer>(cmd);
                boundSpritePipeline = spritePipeline.get();
                ribbonPipelineBound = false;
            }

            const SSpritePushConstants pc{
                ResolveSpriteIndex(emitter.SpriteTexture),
                emitter.FlipbookCols,
                emitter.FlipbookRows,
                emitter.FlipbookFrames,
                emitter.FlipbookBlend ? 1u : 0u,
                emitter.GradientTexture ? ResolveSpriteIndex(emitter.GradientTexture) : m_WhiteTextureHandle.Index,
                emitter.GradientTexture ? 1u : 0u,
                emitter.NormalTexture ? ResolveSpriteIndex(emitter.NormalTexture) : m_WhiteTextureHandle.Index,
                emitter.NormalTexture ? 1u : 0u,
                emitter.EmissionTexture ? ResolveSpriteIndex(emitter.EmissionTexture) : m_WhiteTextureHandle.Index,
                emitter.EmissionTexture ? emitter.EmissionScale : 0.0f
            };
            m_SpriteShader->SetPushConstant(cmd, "pc", (void*)&pc, sizeof(SSpritePushConstants));

            cmd->Draw(6, emitter.MaxParticles, 0, emitter.ParticleOffset);
        }

        EndRendering(cmd);

        // Refraction pass: distortion emitters read the scene rendered so far.
        bool hasDistortion = false;
        for (uint32_t i = 0; i < emitterCount; ++i)
        {
            const auto& e = system.Emitters[i];
            if (e.RenderMode == EParticleRenderMode::Sprite && e.DistortionTexture && e.MaxParticles > 0)
            {
                hasDistortion = true;
                break;
            }
        }

        if (hasDistortion)
        {
            // Snapshot the scene (flushes the pass above onto the primary buffer).
            m_GraphicsContext->GrabSceneColor();

            const auto distortCmd = m_GraphicsContext->GetSecondaryCommandBuffer();
            distortCmd->Begin({
                .ColorAttachment = m_GraphicsContext->GetRenderTarget(),
                .DepthStencilAttachment = m_GraphicsContext->GetDepthStencilRenderTarget(),
                .RenderArea = m_RenderExtent
            });

            BeginRendering(distortCmd);
            m_DistortionPipeline->Bind(distortCmd);
            m_ParticleBuffer->BindAs<VertexBuffer>(distortCmd);

            const float scroll = m_ElapsedTimeSeconds * 0.05f;

            for (uint32_t i = 0; i < emitterCount; ++i)
            {
                const auto& emitter = system.Emitters[i];
                if (emitter.RenderMode != EParticleRenderMode::Sprite ||
                    !emitter.DistortionTexture || emitter.MaxParticles == 0)
                    continue;

                const SDistortPushConstants pc{
                    ResolveSpriteIndex(emitter.DistortionTexture),
                    emitter.DistortionStrength,
                    scroll,
                    scroll * 0.7f,
                    (float)m_RenderExtent.Width,
                    (float)m_RenderExtent.Height
                };
                m_DistortionShader->SetPushConstant(distortCmd, "pc", (void*)&pc, sizeof(SDistortPushConstants));

                distortCmd->Draw(6, emitter.MaxParticles, 0, emitter.ParticleOffset);
            }

            EndRendering(distortCmd);
        }

        // Bloom: snapshot the fully-composited scene, then add a blurred bright
        // pass back onto the render target.
        m_GraphicsContext->GrabSceneColor();

        const auto bloomCmd = m_GraphicsContext->GetSecondaryCommandBuffer();
        bloomCmd->Begin({
            .ColorAttachment = m_GraphicsContext->GetRenderTarget(),
            .DepthStencilAttachment = m_GraphicsContext->GetDepthStencilRenderTarget(),
            .RenderArea = m_RenderExtent
        });

        BeginRendering(bloomCmd);
        m_BloomPipeline->Bind(bloomCmd);

        const SBloomPushConstants bloomPc{
            0.80f, 1.15f, 3.0f,
            (float)m_RenderExtent.Width,
            (float)m_RenderExtent.Height
        };
        m_BloomShader->SetPushConstant(bloomCmd, "pc", (void*)&bloomPc, sizeof(SBloomPushConstants));

        bloomCmd->Draw(3);

        EndRendering(bloomCmd);

        // Volumetric clouds: raymarch the cloud layer at half resolution into an
        // offscreen target, blit-upscale it (linear, which also smooths the
        // per-pixel jitter), then composite it over the full-res scene + sky.
        const uint32_t halfW = m_CloudHalf->GetWidth();
        const uint32_t halfH = m_CloudHalf->GetHeight();
        const Extent2D halfExtent{ halfW, halfH };

        const auto cloudCmd = m_GraphicsContext->GetSecondaryCommandBuffer();
        const SRenderingInfo cloudInfo{
            .ColorAttachment = m_CloudHalf,
            .DepthStencilAttachment = nullptr,
            .RenderArea = halfExtent
        };
        cloudCmd->Begin(cloudInfo);
        cloudCmd->BeginRendering(cloudInfo);

        Viewport cloudViewport{};
        cloudViewport.Width = (float)halfW;
        cloudViewport.Height = (float)halfH;
        cloudViewport.MaxDepth = 1.0f;

        Rect2D cloudScissor{};
        cloudScissor.Extent = halfExtent;

        cloudCmd->SetViewports({ cloudViewport });
        cloudCmd->SetScissors({ cloudScissor });
        m_CloudPipeline->Bind(cloudCmd);
        cloudCmd->Draw(3);
        EndRendering(cloudCmd);

        // Upscale the half-res clouds to full res (linear blit); this also flushes
        // the cloud pass and provides the read-after-write barrier.
        m_GraphicsContext->BlitToTexture(m_CloudHalf, m_CloudFull);

        // Composite the upscaled clouds over the scene at full resolution.
        m_GraphicsContext->GrabSceneColor();

        const auto cloudCompositeCmd = m_GraphicsContext->GetSecondaryCommandBuffer();
        cloudCompositeCmd->Begin({
            .ColorAttachment = m_GraphicsContext->GetRenderTarget(),
            .DepthStencilAttachment = m_GraphicsContext->GetDepthStencilRenderTarget(),
            .RenderArea = m_RenderExtent
        });

        BeginRendering(cloudCompositeCmd);
        m_CloudCompositePipeline->Bind(cloudCompositeCmd);
        cloudCompositeCmd->Draw(3);
        EndRendering(cloudCompositeCmd);

        // Froxel fog apply: snapshot the composited scene, then a fullscreen pass
        // samples the integrated froxel grid and composites the fog over it.
        m_GraphicsContext->GrabSceneColor();

        const auto fogCmd = m_GraphicsContext->GetSecondaryCommandBuffer();
        fogCmd->Begin({
            .ColorAttachment = m_GraphicsContext->GetRenderTarget(),
            .DepthStencilAttachment = m_GraphicsContext->GetDepthStencilRenderTarget(),
            .RenderArea = m_RenderExtent
        });

        BeginRendering(fogCmd);
        m_FogPipeline->Bind(fogCmd);
        fogCmd->Draw(3);
        EndRendering(fogCmd);
    }

    void Renderer::Init(const ShaderLoader* shaderLoader)
    {
        m_SpawnShader = shaderLoader->LoadShader(
            "./Shaders/Aether/",
            std::array<std::string_view, 1>{ "ParticlesSpawn" },
            "ParticlesSpawn",
            EShaderStage::Compute
        );

        m_UpdateShader = shaderLoader->LoadShader(
            "./Shaders/Aether/",
            std::array<std::string_view, 1>{ "ParticlesUpdate" },
            "ParticlesUpdate",
            EShaderStage::Compute
        );

        m_SpriteShader = shaderLoader->LoadShader(
            "./Shaders/Aether/",
            std::array<std::string_view, 1>{ "Sprite" },
            "SpriteRenderer"
        );

        m_RibbonShader = shaderLoader->LoadShader(
            "./Shaders/Aether/",
            std::array<std::string_view, 1>{ "Ribbon" },
            "RibbonRenderer"
        );

        m_MeshShader = shaderLoader->LoadShader(
            "./Shaders/Aether/",
            std::array<std::string_view, 1>{ "Mesh" },
            "MeshRenderer"
        );

        m_FroxelBuildShader = shaderLoader->LoadShader(
            "./Shaders/Aether/",
            std::array<std::string_view, 1>{ "FroxelBuild" },
            "FroxelBuild",
            EShaderStage::Compute
        );

        SPipelineCreateInfo pipelineInfo{};
        pipelineInfo.Shader = m_SpawnShader;
        m_SpawnPipeline = ComputePipeline::Create(m_GraphicsContext, pipelineInfo);

        pipelineInfo.Shader = m_UpdateShader;
        m_UpdatePipeline = ComputePipeline::Create(m_GraphicsContext, pipelineInfo);

        pipelineInfo.Shader = m_FroxelBuildShader;
        m_FroxelBuildPipeline = ComputePipeline::Create(m_GraphicsContext, pipelineInfo);

        const BufferLayout spriteBufferLayout({
            {
                {
                    { EDataType::Vec4,  "PositionSize"    },
                    { EDataType::Vec4,  "VelocityAge"     },
                    { EDataType::Vec4,  "Transform"       },
                    { EDataType::Vec4,  "TangentRibbonId" },
                    { EDataType::Vec4,  "Color"           },
                    { EDataType::Vec4,  "Metadata"        }
                },
                EInputRate::Instance
            }
        });

        PipelineBuilder spriteBuilder;
        spriteBuilder.SetShader(m_SpriteShader);
        spriteBuilder.SetInputTopology(EPrimitiveTopology::TriangleList);
        spriteBuilder.SetPolygonMode(EPolygonMode::Fill);
        spriteBuilder.SetCullMode(ECullMode::None, EFrontFace::CounterClockwise);
        spriteBuilder.EnableAlphaBlending();
        spriteBuilder.DisableDepthTest();
        spriteBuilder.SetColorAttachmentFormat(EImageFormat::R8G8B8A8_SRGB);
        spriteBuilder.SetDepthAttachmentFormat(EDepthStencilImageFormat::D32_SFLOAT);
        spriteBuilder.SetBufferLayout(spriteBufferLayout);
        m_SpritePipeline = spriteBuilder.Build(m_GraphicsContext);

        // Additive variant selected per emitter (fire/sparks); same shader and
        // vertex layout, only the color-blend state differs.
        spriteBuilder.EnableAdditiveBlending();
        m_SpriteAdditivePipeline = spriteBuilder.Build(m_GraphicsContext);

        // Distortion pipeline: same instanced billboard layout, but its own
        // shader samples the grabbed scene color. Alpha-blended so the refracted
        // region feathers over the scene.
        m_DistortionShader = shaderLoader->LoadShader(
            "./Shaders/Aether/",
            std::array<std::string_view, 1>{ "SpriteDistortion" },
            "SpriteDistortion"
        );

        PipelineBuilder distortionBuilder;
        distortionBuilder.SetShader(m_DistortionShader);
        distortionBuilder.SetInputTopology(EPrimitiveTopology::TriangleList);
        distortionBuilder.SetPolygonMode(EPolygonMode::Fill);
        distortionBuilder.SetCullMode(ECullMode::None, EFrontFace::CounterClockwise);
        distortionBuilder.EnableAlphaBlending();
        distortionBuilder.DisableDepthTest();
        distortionBuilder.SetColorAttachmentFormat(EImageFormat::R8G8B8A8_SRGB);
        distortionBuilder.SetDepthAttachmentFormat(EDepthStencilImageFormat::D32_SFLOAT);
        distortionBuilder.SetBufferLayout(spriteBufferLayout);
        m_DistortionPipeline = distortionBuilder.Build(m_GraphicsContext);

        // Bloom: a fullscreen triangle (no vertex buffer) that samples the scene
        // color and adds a blurred bright pass back onto the render target.
        m_BloomShader = shaderLoader->LoadShader(
            "./Shaders/Aether/",
            std::array<std::string_view, 1>{ "Bloom" },
            "Bloom"
        );

        PipelineBuilder bloomBuilder;
        bloomBuilder.SetShader(m_BloomShader);
        bloomBuilder.SetInputTopology(EPrimitiveTopology::TriangleList);
        bloomBuilder.SetPolygonMode(EPolygonMode::Fill);
        bloomBuilder.SetCullMode(ECullMode::None, EFrontFace::CounterClockwise);
        bloomBuilder.EnableAdditiveBlending();
        bloomBuilder.DisableDepthTest();
        bloomBuilder.SetColorAttachmentFormat(EImageFormat::R8G8B8A8_SRGB);
        bloomBuilder.SetDepthAttachmentFormat(EDepthStencilImageFormat::D32_SFLOAT);
        bloomBuilder.SetBufferLayout({});
        m_BloomPipeline = bloomBuilder.Build(m_GraphicsContext);

        // Volumetric fog: a fullscreen triangle that raymarches a height-fog
        // medium and alpha-blends the result over the scene.
        m_FogShader = shaderLoader->LoadShader(
            "./Shaders/Aether/",
            std::array<std::string_view, 1>{ "Fog" },
            "Fog"
        );

        PipelineBuilder fogBuilder;
        fogBuilder.SetShader(m_FogShader);
        fogBuilder.SetInputTopology(EPrimitiveTopology::TriangleList);
        fogBuilder.SetPolygonMode(EPolygonMode::Fill);
        fogBuilder.SetCullMode(ECullMode::None, EFrontFace::CounterClockwise);
        fogBuilder.DisableBlending(); // apply overwrites RT with scene*T + scatter
        fogBuilder.DisableDepthTest();
        fogBuilder.SetColorAttachmentFormat(EImageFormat::R8G8B8A8_SRGB);
        fogBuilder.SetDepthAttachmentFormat(EDepthStencilImageFormat::D32_SFLOAT);
        fogBuilder.SetBufferLayout({});
        m_FogPipeline = fogBuilder.Build(m_GraphicsContext);

        // Volumetric clouds: a fullscreen triangle that raymarches a high cloud
        // slab and composites it over the scene as distant sky.
        m_CloudShader = shaderLoader->LoadShader(
            "./Shaders/Aether/",
            std::array<std::string_view, 1>{ "Clouds" },
            "Clouds"
        );

        // Cloud raymarch renders to the half-res target only (no depth, so the
        // single attachment stays MoltenVK/Metal-friendly).
        PipelineBuilder cloudBuilder;
        cloudBuilder.SetShader(m_CloudShader);
        cloudBuilder.SetInputTopology(EPrimitiveTopology::TriangleList);
        cloudBuilder.SetPolygonMode(EPolygonMode::Fill);
        cloudBuilder.SetCullMode(ECullMode::None, EFrontFace::CounterClockwise);
        cloudBuilder.DisableBlending();
        cloudBuilder.DisableDepthTest();
        cloudBuilder.SetColorAttachmentFormat(EImageFormat::R8G8B8A8_UNORM);
        cloudBuilder.SetDepthAttachmentFormat(EDepthStencilImageFormat::Undefined);
        cloudBuilder.SetBufferLayout({});
        m_CloudPipeline = cloudBuilder.Build(m_GraphicsContext);

        // Cloud composite: full-res pass that upsamples the cloud result over the
        // scene and paints the procedural sky behind it.
        m_CloudCompositeShader = shaderLoader->LoadShader(
            "./Shaders/Aether/",
            std::array<std::string_view, 1>{ "CloudComposite" },
            "CloudComposite"
        );

        PipelineBuilder compositeBuilder;
        compositeBuilder.SetShader(m_CloudCompositeShader);
        compositeBuilder.SetInputTopology(EPrimitiveTopology::TriangleList);
        compositeBuilder.SetPolygonMode(EPolygonMode::Fill);
        compositeBuilder.SetCullMode(ECullMode::None, EFrontFace::CounterClockwise);
        compositeBuilder.DisableBlending(); // overwrites RT with background*T + scatter
        compositeBuilder.DisableDepthTest();
        compositeBuilder.SetColorAttachmentFormat(EImageFormat::R8G8B8A8_SRGB);
        compositeBuilder.SetDepthAttachmentFormat(EDepthStencilImageFormat::D32_SFLOAT);
        compositeBuilder.SetBufferLayout({});
        m_CloudCompositePipeline = compositeBuilder.Build(m_GraphicsContext);

        m_Sprites = TextureSet::Create(m_GraphicsContext);
        m_SpriteSampler = SamplerBuilder().Build(m_GraphicsContext);

        PipelineBuilder ribbonBuilder;
        ribbonBuilder.SetShader(m_RibbonShader);
        ribbonBuilder.SetInputTopology(EPrimitiveTopology::TriangleList);
        ribbonBuilder.SetPolygonMode(EPolygonMode::Fill);
        ribbonBuilder.SetCullMode(ECullMode::None, EFrontFace::CounterClockwise);
        ribbonBuilder.EnableAlphaBlendingMax();
        ribbonBuilder.DisableDepthTest();
        ribbonBuilder.SetColorAttachmentFormat(EImageFormat::R8G8B8A8_SRGB);
        ribbonBuilder.SetDepthAttachmentFormat(EDepthStencilImageFormat::D32_SFLOAT);
        ribbonBuilder.SetBufferLayout({});
        m_RibbonPipeline = ribbonBuilder.Build(m_GraphicsContext);

        const BufferLayout meshBufferLayout({
            {
                {
                    { EDataType::Vec3,  "Position" },
                    { EDataType::Vec3,  "Normal"   },
                },
                EInputRate::Vertex
            },
            {
                {
                    { EDataType::Vec4,  "PositionSize"    },
                    { EDataType::Vec4,  "VelocityAge"     },
                    { EDataType::Vec4,  "Transform"       },
                    { EDataType::Vec4,  "TangentRibbonId" },
                    { EDataType::Vec4,  "Color"           },
                    { EDataType::Vec4,  "Metadata"        }
                },
                EInputRate::Instance
            }
        });

        PipelineBuilder meshBuilder;
        meshBuilder.SetShader(m_MeshShader);
        meshBuilder.SetInputTopology(EPrimitiveTopology::TriangleList);
        meshBuilder.SetPolygonMode(EPolygonMode::Fill);
        meshBuilder.SetCullMode(ECullMode::Back, EFrontFace::CounterClockwise);
        meshBuilder.EnableAlphaBlendingMax();
        meshBuilder.SetColorAttachmentFormat(EImageFormat::R8G8B8A8_SRGB);
        meshBuilder.SetDepthAttachmentFormat(EDepthStencilImageFormat::D32_SFLOAT);
        meshBuilder.SetBufferLayout(meshBufferLayout);

        auto info = meshBuilder.GetCreateInfo();
        info.DepthStencil.DepthTestEnable = true;
        info.DepthStencil.DepthWriteEnable = true;
        info.DepthStencil.DepthCompareOp = ECompareOp::LessOrEqual;

        m_MeshPipeline = GraphicsPipeline::Create(m_GraphicsContext, info);
    }

    void Renderer::CreateBuffers()
    {
        m_ParticleBuffer = StorageBuffer::Create(m_GraphicsContext, sizeof(SGPUParticleState) * MAX_PARTICLES);
        m_EmitterBuffer = DynamicStorageBuffer::Create(m_GraphicsContext, sizeof(SEmitterData) * MAX_EMITTERS);
        m_OpBuffer = DynamicStorageBuffer::Create(m_GraphicsContext, sizeof(SParticleOpData) * MAX_OPS);
        m_ParameterBuffer = DynamicStorageBuffer::Create(m_GraphicsContext, sizeof(SParameterData) * MAX_PARAMETERS);
        m_ParamsBuffer = UniformBuffer::Create(m_GraphicsContext, sizeof(SParamsData));

        m_FroxelBuffer = StorageBuffer::Create(
            m_GraphicsContext, sizeof(glm::vec4) * FROXEL_W * FROXEL_H * FROXEL_D);
        m_FroxelParamsBuffer = UniformBuffer::Create(m_GraphicsContext, sizeof(SFroxelData), &m_FroxelData);
        m_CloudParamsBuffer = UniformBuffer::Create(m_GraphicsContext, sizeof(SCloudData), &m_CloudData);

        // Half-res cloud render target + its full-res upscaled copy.
        const auto sc = m_GraphicsContext->GetSwapchainExtent();
        const uint32_t fullW = sc.Width, fullH = sc.Height;
        const uint32_t halfW = (fullW + 1) / 2, halfH = (fullH + 1) / 2;

        auto halfInfo = Texture2D::CreateImageInfo(EImageFormat::R8G8B8A8_UNORM, halfW, halfH);
        halfInfo.Usage = EImageUsage::ColorAttachment | EImageUsage::TransferSrc | EImageUsage::Sampled;
        halfInfo.InitialLayout = EImageLayout::General;
        m_CloudHalf = Texture2D::Create(m_GraphicsContext, halfInfo);

        auto fullInfo = Texture2D::CreateImageInfo(EImageFormat::R8G8B8A8_UNORM, fullW, fullH);
        fullInfo.Usage = EImageUsage::Sampled | EImageUsage::TransferDst;
        fullInfo.InitialLayout = EImageLayout::General;
        m_CloudFull = Texture2D::Create(m_GraphicsContext, fullInfo);

        CreateMeshVertexBuffer();
    }

    void Renderer::CreateMeshVertexBuffer()
    {
        static constexpr std::array<MeshVertex, 36> vertices = {{
            {{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
            {{0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
            {{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
            {{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
            {{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
            {{-0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},

            {{0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
            {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
            {{-0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
            {{0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
            {{-0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
            {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},

            {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}},
            {{-0.5f, -0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}},
            {{-0.5f, 0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}},
            {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}},
            {{-0.5f, 0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}},
            {{-0.5f, 0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}},

            {{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},
            {{0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
            {{0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
            {{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},
            {{0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
            {{0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},

            {{-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
            {{0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
            {{0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
            {{-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
            {{0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
            {{-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},

            {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}},
            {{0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}},
            {{0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}},
            {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}},
            {{0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}},
            {{-0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}},
        }};

        m_MeshVertexCount = (uint32_t)vertices.size();
        m_MeshVertexBuffer = VertexBuffer::Create(
            m_GraphicsContext,
            sizeof(MeshVertex) * vertices.size(),
            vertices.data()
        );

        m_MeshVertexBuffer->SetLayout(m_MeshPipeline->GetBufferLayout());
    }

    void Renderer::InitPerFrameData()
    {
        m_FrameConstantBuffer = UniformBuffer::Create(
            m_GraphicsContext,
            sizeof(SFrameData),
            &m_FrameData
        );
    }

    void Renderer::BindShaderParameters()
    {
        constexpr SSpawnPushConstants pushConstants{ 0 };
        m_SpawnShader->SetPushConstant("pc", (void*)&pushConstants, sizeof(SSpawnPushConstants));
        m_SpawnShader->BindStorageBuffer("particles", m_ParticleBuffer);
        m_SpawnShader->BindStorageBuffer("emitters", m_EmitterBuffer);
        m_SpawnShader->BindStorageBuffer("ops", m_OpBuffer);
        m_SpawnShader->BindStorageBuffer("parameters", m_ParameterBuffer);
        m_SpawnShader->BindConstantBuffer("cbParams", m_ParamsBuffer);

        m_UpdateShader->BindStorageBuffer("particles", m_ParticleBuffer);
        m_UpdateShader->BindStorageBuffer("emitters", m_EmitterBuffer);
        m_UpdateShader->BindStorageBuffer("ops", m_OpBuffer);
        m_UpdateShader->BindStorageBuffer("parameters", m_ParameterBuffer);
        m_UpdateShader->BindConstantBuffer("cbParams", m_ParamsBuffer);

        const auto whiteTex = Texture2D::Create(
            m_GraphicsContext,
            EImageFormat::R8G8B8A8_SRGB,
            1, 1,
            &Color::WhiteAlpha
        );

        m_WhiteTextureHandle = m_Sprites->AddTexture(whiteTex);

        const SSpritePushConstants spritePc{ m_WhiteTextureHandle.Index };
        m_SpriteShader->SetPushConstant("pc", (void*)&spritePc, sizeof(SSpritePushConstants));
        m_SpriteShader->BindConstantBuffer("cbFrame", m_FrameConstantBuffer);
        m_SpriteShader->BindTextureSet("sprites", m_Sprites);
        m_SpriteShader->BindSampler("spriteSampler", m_SpriteSampler);

        constexpr SRibbonPushConstants ribbonPc{ 0 };
        m_RibbonShader->SetPushConstant("pc", (void*)&ribbonPc, sizeof(SRibbonPushConstants));
        m_RibbonShader->BindStorageBuffer("particles", m_ParticleBuffer);
        m_RibbonShader->BindStorageBuffer("emitters", m_EmitterBuffer);
        m_RibbonShader->BindConstantBuffer("cbFrame", m_FrameConstantBuffer);

        m_MeshShader->BindConstantBuffer("cbFrame", m_FrameConstantBuffer);

        constexpr SDistortPushConstants distortPc{};
        m_DistortionShader->SetPushConstant("pc", (void*)&distortPc, sizeof(SDistortPushConstants));
        m_DistortionShader->BindConstantBuffer("cbFrame", m_FrameConstantBuffer);
        m_DistortionShader->BindTextureSet("sprites", m_Sprites);
        m_DistortionShader->BindSampler("spriteSampler", m_SpriteSampler);
        m_DistortionShader->BindTexture("sceneColor", m_GraphicsContext->GetSceneColorTexture());

        constexpr SBloomPushConstants bloomPc{};
        m_BloomShader->SetPushConstant("pc", (void*)&bloomPc, sizeof(SBloomPushConstants));
        m_BloomShader->BindSampler("bloomSampler", m_SpriteSampler);
        m_BloomShader->BindTexture("sceneColor", m_GraphicsContext->GetSceneColorTexture());

        // Froxel build (compute) and fog apply (fullscreen) share the params
        // uniform and the froxel grid storage buffer.
        m_FroxelBuildShader->BindStorageBuffer("froxels", m_FroxelBuffer);
        m_FroxelBuildShader->BindConstantBuffer("cbFroxel", m_FroxelParamsBuffer);

        m_FogShader->BindConstantBuffer("cbFroxel", m_FroxelParamsBuffer);
        m_FogShader->BindStorageBuffer("froxels", m_FroxelBuffer);
        m_FogShader->BindSampler("fogSampler", m_SpriteSampler);
        m_FogShader->BindTexture("sceneColor", m_GraphicsContext->GetSceneColorTexture());

        m_CloudShader->BindConstantBuffer("cbCloud", m_CloudParamsBuffer);

        m_CloudCompositeShader->BindConstantBuffer("cbCloud", m_CloudParamsBuffer);
        m_CloudCompositeShader->BindSampler("linearSampler", m_SpriteSampler);
        m_CloudCompositeShader->BindTexture("sceneColor", m_GraphicsContext->GetSceneColorTexture());
        m_CloudCompositeShader->BindTexture("cloudTex", m_CloudFull);
    }

    uint32_t Renderer::ResolveSpriteIndex(const Ref<Texture2D>& texture)
    {
        if (!texture)
            return m_WhiteTextureHandle.Index;

        if (const auto it = m_SpriteTextures.find(texture); it != m_SpriteTextures.end())
            return it->second.Index;

        const auto handle = m_Sprites->AddTexture(texture);
        m_SpriteTextures[texture] = handle;

        return handle.Index;
    }

    void Renderer::BeginRendering(const Ref<CommandBuffer>& cmd) const
    {
        const auto renderingInfo = SRenderingInfo
        {
            .ColorAttachment = m_GraphicsContext->GetRenderTarget(),
            .DepthStencilAttachment = m_GraphicsContext->GetDepthStencilRenderTarget(),
            .RenderArea = m_RenderExtent
        };

        Viewport viewport = {};
        viewport.X = 0;
        viewport.Y = 0;
        viewport.Width = (float)m_RenderExtent.Width;
        viewport.Height = (float)m_RenderExtent.Height;
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;

        Rect2D scissor = {};
        scissor.Offset = { 0, 0 };
        scissor.Extent = m_RenderExtent;

        cmd->BeginRendering(renderingInfo);
        cmd->SetViewports({ viewport });
        cmd->SetScissors({ scissor });
    }

    void Renderer::EndRendering(const Ref<CommandBuffer>& cmd) const
    {
        cmd->EndRendering();
        m_GraphicsContext->EnqueueSecondaryCommandBuffer(cmd);
    }

    void Renderer::UpdateBuffers(const SGPUSystem& system)
    {
        // The GPU buffers are fixed-capacity. If the built system exceeds any of
        // them, the copy below clamps silently while emitter op offsets/counts
        // (computed without these limits in Emitter::Build) keep pointing past
        // the uploaded range — i.e. out-of-bounds reads on the GPU. Surface it
        // loudly once instead of corrupting the dispatch.
        if (!m_CapacityErrorReported &&
            (system.Emitters.size()   > MAX_EMITTERS ||
             system.Ops.size()        > MAX_OPS      ||
             system.Parameters.size() > MAX_PARAMETERS))
        {

            EE_CORE_ERROR(
                "Aether system '{}' exceeds renderer capacity and will be truncated "
                "(emitters {}/{}, ops {}/{}, parameters {}/{}).",
                system.Name,
                system.Emitters.size(),   MAX_EMITTERS,
                system.Ops.size(),        MAX_OPS,
                system.Parameters.size(), MAX_PARAMETERS
            )

            m_CapacityErrorReported = true;
        }

        SParamsData params{};

        params.Time = {
            m_LastDeltaTimeSeconds,
            m_ElapsedTimeSeconds,
            (float)system.TotalMaxParticles,
            (float)system.Emitters.size(),
        };

        params.Viewport = {
            (float)m_RenderExtent.Width,
            (float)m_RenderExtent.Height,
            0.0f,
            0.0f
        };

        m_ParamsBuffer->UpdateData(&params, sizeof(SParamsData));

        auto* emitters = (SEmitterData*)m_EmitterBuffer->Map();
        const auto emitterCount = std::min(system.Emitters.size(), (size_t)MAX_EMITTERS);

        for (size_t i = 0; i < emitterCount; ++i)
        {
            const auto& emitter = system.Emitters[i];
            auto& emitterState = m_EmittersState[emitter];

            if (!emitterState.Initialized)
            {
                // A burst delay is a phase offset on the first burst: start the
                // accumulator behind so it reaches the interval that much later.
                emitterState.BurstAccumulator = -emitter.BurstDelaySeconds;
                emitterState.Initialized = true;
            }

            emitterState.SpawnAccumulator += emitter.SpawnRatePerSecond * m_LastDeltaTimeSeconds;

            uint32_t spawnCount = std::min((uint32_t)emitterState.SpawnAccumulator, emitter.MaxParticles);
            if (spawnCount > 0u)
                emitterState.SpawnAccumulator -= (float)spawnCount;

            if (emitter.BurstCount > 0u && emitter.BurstIntervalSeconds > 0.0f)
            {
                auto& accumulator = emitterState.BurstAccumulator;
                accumulator += m_LastDeltaTimeSeconds;
                uint32_t burstLoops = 0u;
                while (accumulator >= emitter.BurstIntervalSeconds && burstLoops < 8u)
                {
                    accumulator -= emitter.BurstIntervalSeconds;
                    spawnCount = std::min(emitter.MaxParticles, spawnCount + emitter.BurstCount);
                    ++burstLoops;
                }
            }

            auto desc = ToEmitterDescription(emitter);

            const uint32_t nextBufferCursor = emitter.MaxParticles > 0
                ? (emitterState.BufferCursor + spawnCount) % emitter.MaxParticles
                : 0u;

            desc.MetaB.x = (float)emitter.UpdateOpOffset;
            desc.MetaB.y = (float)emitter.UpdateOpCount;
            desc.MetaB.z = (float)emitterState.BufferCursor;
            desc.MetaB.w = (float)spawnCount;
            desc.MetaC.w = (float)nextBufferCursor;
            desc.MetaD.x = (float)emitterState.EmissionIndex;

            emitters[i] = desc;

            emitterState.EmissionIndex += spawnCount;

            if (emitter.MaxParticles > 0)
                emitterState.BufferCursor = nextBufferCursor;
        }

        // Zero-out inactive slots so removed emitters don't leave stale data.
        // This is done AFTER writing the active slots to avoid a window where
        // the GPU could briefly read zeroed MetaA fields for an active emitter.
        for (size_t i = emitterCount; i < MAX_EMITTERS; ++i)
            emitters[i] = {};

        auto* ops = (SParticleOpData*)m_OpBuffer->Map();
        const size_t opCount = std::min(system.Ops.size(), (size_t)MAX_OPS);
        for (size_t i = 0; i < opCount; ++i)
            ops[i] = ToOpDescription(system.Ops[i]);
        for (size_t i = opCount; i < MAX_OPS; ++i)
            ops[i] = {};

        auto* parameters = (SParameterData*)m_ParameterBuffer->Map();
        const size_t parameterCount = std::min(system.Parameters.size(), (size_t)MAX_PARAMETERS);
        for (size_t i = 0; i < parameterCount; ++i)
            parameters[i] = ToParameterDescription(system.Parameters[i]);
        for (size_t i = parameterCount; i < MAX_PARAMETERS; ++i)
            parameters[i] = {};
    }
}
