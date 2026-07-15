#include "epch.h"
#include "MeshRenderer.h"

#include "Engine/Graphics/CommandBuffer.h"
#include "Engine/Graphics/SamplerBuilder.h"
#include "Engine/Graphics/Pipeline/PipelineBuilder.h"

#include <algorithm>
#include <chrono>

namespace Elixir
{
    MeshRenderer::MeshRenderer(const GraphicsContext* context, const ShaderLoader* shaderLoader)
        : m_GraphicsContext(context)
    {
        EE_CORE_INFO("Initializing Mesh Renderer.")

        m_Shader = shaderLoader->LoadShader("./Shaders/", "Model");
        CreatePipelines();

        m_FrameBuffer = UniformBuffer::Create(context, sizeof(SFrameData), &m_FrameData);
        m_Sampler = SamplerBuilder().SetMaxLod(16.0f).Build(context);
        m_Textures = TextureSet::Create(context);

        // A 1x1 white texture keeps the bindless array non-empty; missing maps use
        // index NO_TEXTURE and the shader falls back to factors/defaults.
        const uint32_t whitePixel = 0xffffffffu;
        const auto whiteTexture = Texture2D::Create(
            context, EImageFormat::R8G8B8A8_SRGB, 1, 1, &whitePixel);
        m_Textures->AddTexture(whiteTexture);

        BindResources();

        // Load the HDR environment and register its maps in the bindless set so
        // the shader can sample them for image-based lighting.
        m_Environment = Environment::Load(context, "./Assets/Textures/white_home_studio_2k.hdr");
        if (m_Environment)
        {
            const auto& env = m_Environment->GetEnvironment();
            m_EnvIndex = m_Textures->AddTexture(env).Index;
            m_IrradianceIndex = m_Textures->AddTexture(m_Environment->GetIrradiance()).Index;
            m_PrefIndex = m_Textures->AddTexture(m_Environment->GetPrefiltered()).Index;

            // Highest mip index of the env map, used by the shader to map roughness
            // to a prefilter LOD.
            const auto extent = env->GetExtent();
            const uint32_t maxDim = std::max(extent.Width, extent.Height);
            m_EnvMaxLod = std::floor(std::log2((float)maxDim));
        }
    }

    void MeshRenderer::CreatePipelines()
    {
        CreatePipelinesFor(m_Shader, m_Pipeline, m_TransparentPipeline);
    }

    void MeshRenderer::CreatePipelinesFor(
        const Ref<Shader>& shader, Ref<GraphicsPipeline>& opaque, Ref<GraphicsPipeline>& transparent)
    {
        const BufferLayout layout({
            {
                {
                    { EDataType::Vec3, "Position" },
                    { EDataType::Vec3, "Normal"   },
                    { EDataType::Vec4, "Tangent"  },
                    { EDataType::Vec2, "TexCoord" },
                },
                EInputRate::Vertex
            }
        });

        PipelineBuilder builder;
        builder.SetShader(shader);
        builder.SetInputTopology(EPrimitiveTopology::TriangleList);
        builder.SetPolygonMode(EPolygonMode::Fill);
        // glTF materials are commonly double-sided; cull nothing and flip the
        // normal per-face in the shader.
        builder.SetCullMode(ECullMode::None, EFrontFace::CounterClockwise);
        builder.DisableBlending();
        builder.SetColorAttachmentFormat(EImageFormat::R8G8B8A8_SRGB);
        builder.SetDepthAttachmentFormat(EDepthStencilImageFormat::D32_SFLOAT);
        builder.SetBufferLayout(layout);

        // Opaque + masked: depth test and write.
        auto opaqueInfo = builder.GetCreateInfo();
        opaqueInfo.DepthStencil.DepthTestEnable = true;
        opaqueInfo.DepthStencil.DepthWriteEnable = true;
        opaqueInfo.DepthStencil.DepthCompareOp = ECompareOp::LessOrEqual;
        opaque = GraphicsPipeline::Create(m_GraphicsContext, opaqueInfo);

        // Blend: alpha-blended, depth-tested but no depth write (drawn after opaque).
        builder.EnableAlphaBlending();
        auto blendInfo = builder.GetCreateInfo();
        blendInfo.DepthStencil.DepthTestEnable = true;
        blendInfo.DepthStencil.DepthWriteEnable = false;
        blendInfo.DepthStencil.DepthCompareOp = ECompareOp::LessOrEqual;
        transparent = GraphicsPipeline::Create(m_GraphicsContext, blendInfo);
    }

    void MeshRenderer::BindResources()
    {
        BindResourcesTo(m_Shader);
    }

    void MeshRenderer::BindResourcesTo(const Ref<Shader>& shader)
    {
        shader->BindConstantBuffer("cbFrame", m_FrameBuffer);
        shader->BindSampler("texSampler", m_Sampler);
        shader->BindTextureSet("textures", m_Textures);
    }

    void MeshRenderer::Retire(SShaderVariant&& variant)
    {
        if (variant.Shader || variant.Opaque || variant.Transparent || variant.ParamBuffer)
            m_Retired.push_back({ std::move(variant), RETIRE_FRAMES });
    }

    void MeshRenderer::TickRetired()
    {
        for (auto& r : m_Retired)
            --r.FramesLeft;
        m_Retired.erase(
            std::remove_if(m_Retired.begin(), m_Retired.end(), [](const SRetired& r) { return r.FramesLeft <= 0; }),
            m_Retired.end());
    }

    void MeshRenderer::SetShader(const Ref<Shader>& shader)
    {
        if (!shader)
            return;
        // Retire the current shader/pipelines instead of destroying them now (in-flight
        // frames may still reference them); they're freed a few frames later.
        Retire({ m_Shader, m_Pipeline, m_TransparentPipeline, nullptr });
        m_Shader = shader;
        CreatePipelines();
        BindResources();
        m_BoundModel = nullptr; // force the material buffer to rebind to the new shader
    }

    void MeshRenderer::SetMaterialShader(uint32_t materialIndex, const Ref<Shader>& shader, EMaterialBlendMode blendMode)
    {
        if (!shader)
        {
            if (const auto it = m_MaterialShaders.find(materialIndex); it != m_MaterialShaders.end())
            {
                Retire(std::move(it->second));
                m_MaterialShaders.erase(it);
            }
            return;
        }

        SShaderVariant variant;
        variant.Shader = shader;
        variant.BlendMode = blendMode;
        CreatePipelinesFor(shader, variant.Opaque, variant.Transparent);
        BindResourcesTo(shader);

        // Zero-initialised live-parameter buffer (cbGraphParams), bound once.
        glm::vec4 zeros[MAX_GRAPH_PARAMS] = {};
        variant.ParamBuffer = UniformBuffer::Create(m_GraphicsContext, sizeof(zeros), zeros);
        shader->BindConstantBuffer("cbGraphParams", variant.ParamBuffer);

        // Retire the slot's previous variant (may be in flight), then install the new.
        if (const auto it = m_MaterialShaders.find(materialIndex); it != m_MaterialShaders.end())
            Retire(std::move(it->second));
        m_MaterialShaders[materialIndex] = std::move(variant);
        m_BoundModel = nullptr; // force the material buffer to rebind to all shaders
    }

    void MeshRenderer::SetMaterialParams(uint32_t materialIndex, const glm::vec4* params, uint32_t count)
    {
        const auto it = m_MaterialShaders.find(materialIndex);
        if (it == m_MaterialShaders.end() || !it->second.ParamBuffer)
            return;

        glm::vec4 buffer[MAX_GRAPH_PARAMS] = {};
        const uint32_t n = count < MAX_GRAPH_PARAMS ? count : MAX_GRAPH_PARAMS;
        for (uint32_t i = 0; i < n; ++i)
            buffer[i] = params[i];
        it->second.ParamBuffer->UpdateData(buffer, sizeof(buffer));
    }

    uint32_t MeshRenderer::ResolveTexture(const Ref<Texture>& texture)
    {
        if (!texture)
            return NO_TEXTURE;

        if (const auto it = m_TextureIndices.find(texture); it != m_TextureIndices.end())
            return it->second;

        const auto handle = m_Textures->AddTexture(texture);
        m_TextureIndices[texture] = handle.Index;
        return handle.Index;
    }

    Ref<StorageBuffer> MeshRenderer::BuildMaterialBuffer(const Ref<Model>& model)
    {
        std::vector<SGPUMaterial> materials;
        materials.reserve(model->GetMaterials().size());

        // Serialise with UI-thread material edits (see Model::MaterialsMutex).
        std::lock_guard<std::mutex> lock(model->MaterialsMutex());
        for (const auto& material : model->GetMaterials())
        {
            const uint32_t alphaMode = (uint32_t)material->GetScalar("AlphaMode");
            const glm::vec4 baseColor = material->GetVector("BaseColorFactor");

            SGPUMaterial gpu;
            gpu.BaseColorFactor = baseColor;
            gpu.EmissiveMetallic = glm::vec4(glm::vec3(material->GetVector("EmissiveFactor")), material->GetScalar("Metallic"));
            gpu.RoughOccNormalCutoff = glm::vec4(
                material->GetScalar("Roughness"), material->GetScalar("OcclusionStrength"),
                material->GetScalar("NormalScale"), material->GetScalar("AlphaCutoff"));
            gpu.TexIndex0 = glm::uvec4(
                ResolveTexture(material->GetTexture("BaseColorTexture")),
                ResolveTexture(material->GetTexture("MetallicRoughnessTexture")),
                ResolveTexture(material->GetTexture("NormalTexture")),
                ResolveTexture(material->GetTexture("EmissiveTexture")));
            gpu.TexIndex1 = glm::uvec4(
                ResolveTexture(material->GetTexture("OcclusionTexture")), alphaMode,
                ResolveTexture(material->GetTexture("SpecularTexture")),
                ResolveTexture(material->GetTexture("SpecularColorTexture")));
            gpu.BaseColorTransform = material->GetVector("BaseColorTransform");
            gpu.MetallicRoughnessTransform = material->GetVector("MetallicRoughnessTransform");
            gpu.NormalTransform = material->GetVector("NormalTransform");
            gpu.EmissiveTransform = material->GetVector("EmissiveTransform");
            gpu.OcclusionTransform = material->GetVector("OcclusionTransform");
            // The model has no KHR_materials_transmission, so treat its translucent
            // (BLEND, sub-opaque) glass as refractive: it samples the grabbed scene
            // instead of alpha-blending. Packed into Clearcoat.z.
            const float transmission = (alphaMode == 2 && baseColor.a < 0.95f) ? 0.9f : 0.0f;
            gpu.Clearcoat = glm::vec4(
                material->GetScalar("ClearcoatFactor"), material->GetScalar("ClearcoatRoughness"), transmission, 0.0f);
            gpu.Specular = glm::vec4(glm::vec3(material->GetVector("SpecularColorFactor")), material->GetScalar("SpecularFactor"));
            materials.push_back(gpu);
        }

        return StorageBuffer::Create(
            m_GraphicsContext, materials.size() * sizeof(SGPUMaterial), materials.data());
    }

    void MeshRenderer::Render(const Ref<Model>& model, const Camera& camera)
    {
        if (!model || model->GetPrimitives().empty())
            return;

        // Release shader/pipeline resources retired a few frames ago (now safely idle).
        TickRetired();

        // Build (and cache) the model's material buffer, resolving its textures
        // into the bindless set the first time we see it.
        // Repack the material buffer from the instances every frame so runtime edits
        // (e.g. the material editor) always take effect. Cheap for typical material
        // counts, and avoids a cross-thread dirty flag between UI and render.
        auto& materialBuffer = m_MaterialBuffers[model.get()];
        materialBuffer = BuildMaterialBuffer(model);
        m_Shader->BindStorageBuffer("materials", materialBuffer);
        for (auto& [index, variant] : m_MaterialShaders)
            variant.Shader->BindStorageBuffer("materials", materialBuffer);
        m_BoundModel = model.get();

        const auto extent = m_GraphicsContext->GetRenderTarget()->GetExtent();

        // (Re)create the scene-colour grab target to match the render target; glass
        // samples it to refract the scene behind it.
        if (!m_SceneColor || m_SceneColorExtent.Width != extent.Width || m_SceneColorExtent.Height != extent.Height)
        {
            m_SceneColor = Texture2D::Create(
                m_GraphicsContext, EImageFormat::R8G8B8A8_SRGB, extent.Width, extent.Height, nullptr);
            m_SceneColorIndex = m_Textures->AddTexture(m_SceneColor).Index;
            m_SceneColorExtent = { extent.Width, extent.Height, 1 };
        }

        m_FrameData.View = camera.GetViewMatrix();
        m_FrameData.Proj = camera.GetProjectionMatrix();
        m_FrameData.ViewProj = camera.GetViewProjectionMatrix();
        m_FrameData.CameraPos = camera.GetPosition();
        m_FrameData.EnvIndex = m_EnvIndex;
        m_FrameData.IrradianceIndex = m_IrradianceIndex;
        m_FrameData.EnvIntensity = 1.5f;
        m_FrameData.EnvMaxLod = m_EnvMaxLod;
        m_FrameData.PrefIndex = m_PrefIndex;
        m_FrameData.SceneColorIndex = m_SceneColorIndex;
        m_FrameData.ScreenWidth = (float)extent.Width;
        m_FrameData.ScreenHeight = (float)extent.Height;

        // Wall-clock seconds since the first frame, for animated graph materials.
        static const auto s_Start = std::chrono::steady_clock::now();
        m_FrameData.Time = std::chrono::duration<float>(std::chrono::steady_clock::now() - s_Start).count();

        // Directional "sun": a warm key light roughly matching the sunset HDR.
        m_FrameData.LightDirection = glm::vec4(glm::normalize(glm::vec3(-0.5f, 0.65f, -0.55f)), 0.0f);
        m_FrameData.LightColor = glm::vec4(1.0f, 0.96f, 0.9f, 2.2f);
        m_FrameBuffer->UpdateData(&m_FrameData, sizeof(SFrameData));

        const auto cmd = m_GraphicsContext->GetSecondaryCommandBuffer();

        const SRenderingInfo renderingInfo{
            .ColorAttachment = m_GraphicsContext->GetRenderTarget(),
            .DepthStencilAttachment = m_GraphicsContext->GetDepthStencilRenderTarget(),
            .RenderArea = { extent.Width, extent.Height }
        };
        cmd->BeginRendering(renderingInfo);

        Viewport viewport{};
        viewport.Width = (float)extent.Width;
        viewport.Height = (float)extent.Height;
        viewport.MaxDepth = 1.0f;

        Rect2D scissor{};
        scissor.Extent = { extent.Width, extent.Height };

        cmd->SetViewports({ viewport });
        cmd->SetScissors({ scissor });

        const auto& materials = model->GetMaterials();

        // The pipeline currently bound on the command buffer, so consecutive
        // primitives sharing a shader don't rebind. Reset at each pass.
        GraphicsPipeline* boundPipeline = nullptr;
        const auto drawPrimitive = [&](const SModelPrimitive& primitive)
        {
            // Pick the per-material override shader/pipeline if one is set, else the
            // shared default.
            const Ref<Shader>* shader = &m_Shader;
            GraphicsPipeline* pipeline = m_Pipeline.get();
            if (const auto it = m_MaterialShaders.find(primitive.MaterialIndex); it != m_MaterialShaders.end())
            {
                shader = &it->second.Shader;
                // Translucent uses the alpha-blend pipeline (no depth write); Opaque and
                // Masked use the depth-writing one (Masked clips in the shader).
                pipeline = it->second.BlendMode == EMaterialBlendMode::Translucent
                    ? it->second.Transparent.get() : it->second.Opaque.get();
            }

            if (pipeline != boundPipeline)
            {
                pipeline->Bind(cmd);
                boundPipeline = pipeline;
            }

            SModelPushConstants pc;
            pc.Model = primitive.Transform;
            pc.MaterialIndex = primitive.MaterialIndex;
            (*shader)->SetPushConstant(cmd, "pc", (void*)&pc, PUSH_CONSTANT_SIZE);

            primitive.Vertices->Bind(cmd);
            primitive.Indices->Bind(cmd);
            cmd->DrawIndexed(primitive.IndexCount, 1, 0, 0, 0);
        };

        // Treat only genuinely translucent materials as blend. Materials flagged
        // BLEND but with near-opaque base alpha (e.g. the emissive DRL housing at
        // 0.99) are effectively opaque; routing them through the unsorted blend pass
        // just lets later transparent layers overdraw them and swallow their glow.
        const auto isBlend = [&](const SModelPrimitive& p)
        {
            // A graph material's blend mode wins over the glTF alpha mode.
            if (const auto it = m_MaterialShaders.find(p.MaterialIndex); it != m_MaterialShaders.end())
                return it->second.BlendMode == EMaterialBlendMode::Translucent;
            if (p.MaterialIndex >= materials.size())
                return false;
            const auto& m = materials[p.MaterialIndex];
            return (int)m->GetScalar("AlphaMode") == 2 && m->GetVector("BaseColorFactor").a < 0.95f;
        };

        // Pass 1: everything that isn't refractive glass (opaque + masked + the
        // near-opaque emissive parts), with depth write.
        boundPipeline = nullptr;
        for (const auto& primitive : model->GetPrimitives())
            if (!isBlend(primitive))
                drawPrimitive(primitive);
        cmd->EndRendering();

        // Grab the opaque result so the glass can sample the scene behind it.
        m_GraphicsContext->BlitToTexture(cmd, m_SceneColor);

        // Pass 2: refractive glass. Drawn opaque (no alpha blend) -- the shader
        // composites the grabbed scene through it -- so it reads as real glass.
        // Load (don't clear) the opaque depth so the glass is occluded correctly.
        SRenderingInfo glassInfo = renderingInfo;
        glassInfo.LoadDepthStencil = true;
        cmd->BeginRendering(glassInfo);
        cmd->SetViewports({ viewport });
        cmd->SetScissors({ scissor });
        boundPipeline = nullptr;
        for (const auto& primitive : model->GetPrimitives())
            if (isBlend(primitive))
                drawPrimitive(primitive);
        cmd->EndRendering();

        m_GraphicsContext->EnqueueSecondaryCommandBuffer(cmd);
    }
}
