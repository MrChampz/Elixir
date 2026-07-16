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
        const Ref<Shader>& shader, Ref<GraphicsPipeline>& opaque, Ref<GraphicsPipeline>& transparent,
        EMaterialBlendMode blendMode)
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

        // Blend: alpha or additive per the mode, depth-tested but no depth write.
        if (blendMode == EMaterialBlendMode::Additive)
            builder.EnableAdditiveBlending();
        else
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

    void MeshRenderer::RetireVariantCache(const uint32_t materialIndex)
    {
        if (const auto it = m_MaterialShaderCache.find(materialIndex); it != m_MaterialShaderCache.end())
        {
            for (auto& [key, variant] : it->second.Variants)
                Retire(std::move(variant));
            m_MaterialShaderCache.erase(it);
        }
        std::lock_guard<std::mutex> keyLock(m_VariantKeysMutex);
        m_VariantKeys.erase(materialIndex);
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
            RetireVariantCache(materialIndex);
            return;
        }

        SShaderVariant variant;
        variant.Shader = shader;
        variant.BlendMode = blendMode;
        CreatePipelinesFor(shader, variant.Opaque, variant.Transparent, blendMode);
        BindResourcesTo(shader);

        // Zero-initialised live-parameter buffer (cbGraphParams), bound once.
        glm::vec4 zeros[MAX_GRAPH_PARAMS] = {};
        variant.ParamBuffer = UniformBuffer::Create(m_GraphicsContext, sizeof(zeros), zeros);
        shader->BindConstantBuffer("cbGraphParams", variant.ParamBuffer);

        // Retire the slot's previous variant (may be in flight), then install the new.
        if (const auto it = m_MaterialShaders.find(materialIndex); it != m_MaterialShaders.end())
            Retire(std::move(it->second));
        // This shader is set outside the graph path, so any permutation cached for the
        // slot is now unreachable.
        RetireVariantCache(materialIndex);
        m_MaterialShaders[materialIndex] = std::move(variant);
        m_BoundModel = nullptr; // force the material buffer to rebind to all shaders
    }

    void MeshRenderer::PrepareMaterialShader(uint32_t materialIndex, const Ref<Shader>& shader,
        EMaterialBlendMode blendMode, const Ref<Model>& model,
        const Ref<MaterialInstance>& materialInstance, const size_t revision, const size_t variantKey)
    {
        if (!shader)
            return;

        // Built here on the caller's worker thread -- including the slow pipeline
        // creation -- so the render thread never blocks compiling a Metal pipeline.
        SShaderVariant variant;
        variant.Shader = shader;
        variant.BlendMode = blendMode;
        variant.Revision = revision;
        variant.VariantKey = variantKey;
        CreatePipelinesFor(shader, variant.Opaque, variant.Transparent, blendMode);
        BindResourcesTo(shader);

        glm::vec4 zeros[MAX_GRAPH_PARAMS] = {};
        variant.ParamBuffer = UniformBuffer::Create(m_GraphicsContext, sizeof(zeros), zeros);
        shader->BindConstantBuffer("cbGraphParams", variant.ParamBuffer);

        std::lock_guard<std::mutex> lock(m_PendingMutex);
        m_PendingVariants.push_back({ materialIndex, std::move(variant), model, materialInstance,
            revision, variantKey, false });
    }

    bool MeshRenderer::HasMaterialShaderVariant(
        const uint32_t materialIndex, const size_t revision, const size_t variantKey) const
    {
        std::lock_guard<std::mutex> lock(m_VariantKeysMutex);
        const auto slot = m_VariantKeys.find(materialIndex);
        return slot != m_VariantKeys.end() && slot->second.Revision == revision
            && slot->second.Keys.contains(variantKey);
    }

    void MeshRenderer::PrepareCachedMaterialShader(const uint32_t materialIndex,
        const size_t revision, const size_t variantKey, const Ref<Model>& model,
        const Ref<MaterialInstance>& materialInstance)
    {
        std::lock_guard<std::mutex> lock(m_PendingMutex);
        m_PendingVariants.push_back({ materialIndex, {}, model, materialInstance,
            revision, variantKey, true });
    }

    void MeshRenderer::InstallPendingShaders()
    {
        std::vector<SPendingVariant> pendingVariants;
        {
            std::lock_guard<std::mutex> lock(m_PendingMutex);
            pendingVariants.swap(m_PendingVariants);
        }

        for (auto& pending : pendingVariants)
        {
            // The graph asset, its edited instance values and the shader variant
            // become visible in the same render frame. This avoids a transient frame
            // that combines the old material state with the new shader (or vice versa).
            if (pending.Model && pending.MaterialInstance)
            {
                std::lock_guard<std::mutex> materialLock(pending.Model->MaterialsMutex());
                const auto& materials = pending.Model->GetMaterials();
                if (pending.Slot < materials.size())
                {
                    materials[pending.Slot]->SetParent(pending.MaterialInstance->GetParent());
                    materials[pending.Slot]->ApplyCompatibleOverridesFrom(*pending.MaterialInstance);
                    materials[pending.Slot]->SetName(pending.MaterialInstance->GetName());
                }
            }
            const auto active = m_MaterialShaders.find(pending.Slot);
            if (active != m_MaterialShaders.end() && active->second.Revision == pending.Revision
                && active->second.VariantKey == pending.VariantKey)
                continue;

            // Editing the graph mints a new revision, and no permutation of the old one
            // can ever be selected again -- release them instead of caching them for a
            // key nothing will ask for.
            auto& cache = m_MaterialShaderCache[pending.Slot];
            if (cache.Revision != pending.Revision)
            {
                for (auto& [key, variant] : cache.Variants)
                    Retire(std::move(variant));
                cache.Variants.clear();
                cache.Revision = pending.Revision;

                std::lock_guard<std::mutex> keyLock(m_VariantKeysMutex);
                SVariantKeys& keys = m_VariantKeys[pending.Slot];
                keys.Revision = pending.Revision;
                keys.Keys.clear();
            }

            if (pending.UseCached)
            {
                // The revision may have moved on since the caller saw this key.
                const auto cached = cache.Variants.find(pending.VariantKey);
                if (cached == cache.Variants.end())
                {
                    EE_CORE_WARN("Material variant cache: slot {} no longer has key {}.",
                        pending.Slot, pending.VariantKey)
                    continue;
                }
                SShaderVariant selected = std::move(cached->second);
                cache.Variants.erase(cached);
                if (active != m_MaterialShaders.end())
                {
                    cache.Variants.insert_or_assign(active->second.VariantKey, std::move(active->second));
                    m_MaterialShaders.erase(active);
                }
                m_MaterialShaders[pending.Slot] = std::move(selected);
                continue;
            }

            if (active != m_MaterialShaders.end())
            {
                // Only this revision's permutations are worth keeping; one left over
                // from the previous graph is dead weight.
                if (active->second.Revision == pending.Revision)
                    cache.Variants.insert_or_assign(active->second.VariantKey, std::move(active->second));
                else
                    Retire(std::move(active->second));
                m_MaterialShaders.erase(active);
            }
            if (const auto duplicate = cache.Variants.find(pending.VariantKey);
                duplicate != cache.Variants.end())
            {
                Retire(std::move(duplicate->second));
                cache.Variants.erase(duplicate);
            }
            m_MaterialShaders[pending.Slot] = std::move(pending.Variant);
            {
                std::lock_guard<std::mutex> keyLock(m_VariantKeysMutex);
                SVariantKeys& keys = m_VariantKeys[pending.Slot];
                keys.Revision = pending.Revision;
                keys.Keys.insert(pending.VariantKey);
            }
        }
        if (!pendingVariants.empty())
            m_BoundModel = nullptr; // force the material buffer to rebind to all shaders
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

        // Install any material shaders prepared on the main thread, then release
        // resources retired a few frames ago (now safely idle).
        InstallPendingShaders();
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

        // Graph parameter values now come from the same MaterialInstance used to pack
        // GPUMaterial. The renderer owns the upload, so the UI no longer maintains a
        // second graph-only parameter snapshot across threads.
        {
            std::lock_guard<std::mutex> lock(model->MaterialsMutex());
            const auto& instances = model->GetMaterials();
            for (auto& [index, variant] : m_MaterialShaders)
            {
                if (!variant.ParamBuffer || index >= instances.size())
                    continue;
                glm::vec4 params[MAX_GRAPH_PARAMS] = {};
                instances[index]->CollectGraphParams(params, MAX_GRAPH_PARAMS,
                    [this](const Ref<Texture>& texture) { return ResolveTexture(texture); });
                variant.ParamBuffer->UpdateData(params, sizeof(params));
            }
        }
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
                // Translucent/Additive use the blend pipeline (no depth write); Opaque
                // and Masked use the depth-writing one (Masked clips in the shader).
                const bool blended = it->second.BlendMode == EMaterialBlendMode::Translucent
                    || it->second.BlendMode == EMaterialBlendMode::Additive;
                pipeline = blended ? it->second.Transparent.get() : it->second.Opaque.get();
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
                return it->second.BlendMode == EMaterialBlendMode::Translucent
                    || it->second.BlendMode == EMaterialBlendMode::Additive;
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

        // Sort blended primitives back-to-front so their alpha compositing is ordered.
        // The primitive's world transform origin is a cheap depth proxy (exact
        // per-triangle sorting is out of scope); good enough for separated panels.
        const glm::vec3 camPos = camera.GetPosition();
        const auto depthProxy = [&](const SModelPrimitive* p)
        {
            const glm::vec3 d = glm::vec3(p->Transform[3]) - camPos;
            return glm::dot(d, d);
        };
        std::vector<const SModelPrimitive*> blended;
        for (const auto& primitive : model->GetPrimitives())
            if (isBlend(primitive))
                blended.push_back(&primitive);
        std::sort(blended.begin(), blended.end(),
            [&](const SModelPrimitive* a, const SModelPrimitive* b) { return depthProxy(a) > depthProxy(b); });
        for (const auto* primitive : blended)
            drawPrimitive(*primitive);
        cmd->EndRendering();

        m_GraphicsContext->EnqueueSecondaryCommandBuffer(cmd);
    }
}
