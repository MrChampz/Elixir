#include "epch.h"
#include "MeshRenderer.h"

#include "Engine/Graphics/CommandBuffer.h"
#include "Engine/Graphics/SamplerBuilder.h"

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
        m_SamplerClamp = SamplerBuilder()
            .SetAddressModeU(ESamplerAddressMode::ClampToEdge)
            .SetAddressModeV(ESamplerAddressMode::ClampToEdge)
            .SetAddressModeW(ESamplerAddressMode::ClampToEdge)
            .SetMaxLod(16.0f)
            .Build(context);
        m_SamplerPoint = SamplerBuilder()
            .SetMagFilter(ESamplerFilter::Nearest)
            .SetMinFilter(ESamplerFilter::Nearest)
            .SetMipmapMode(ESamplerMipmapMode::Nearest)
            .SetMaxLod(16.0f)
            .Build(context);
        m_SamplerPointClamp = SamplerBuilder()
            .SetMagFilter(ESamplerFilter::Nearest)
            .SetMinFilter(ESamplerFilter::Nearest)
            .SetMipmapMode(ESamplerMipmapMode::Nearest)
            .SetAddressModeU(ESamplerAddressMode::ClampToEdge)
            .SetAddressModeV(ESamplerAddressMode::ClampToEdge)
            .SetAddressModeW(ESamplerAddressMode::ClampToEdge)
            .SetMaxLod(16.0f)
            .Build(context);
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
        opaque = MeshPassProcessor::CreatePipeline(
            m_GraphicsContext, shader, EMeshPass::BaseOpaque, blendMode);
        transparent = MeshPassProcessor::CreatePipeline(
            m_GraphicsContext, shader, EMeshPass::Translucent, blendMode);
    }

    void MeshRenderer::BindResources()
    {
        BindResourcesTo(m_Shader);
    }

    void MeshRenderer::BindResourcesTo(const Ref<Shader>& shader)
    {
        shader->BindConstantBuffer("cbFrame", m_FrameBuffer);
        shader->BindSampler("texSampler", m_Sampler);
        if (shader->HasBinding("texSamplerClamp"))
            shader->BindSampler("texSamplerClamp", m_SamplerClamp);
        if (shader->HasBinding("texSamplerPoint"))
            shader->BindSampler("texSamplerPoint", m_SamplerPoint);
        if (shader->HasBinding("texSamplerPointClamp"))
            shader->BindSampler("texSamplerPointClamp", m_SamplerPointClamp);
        shader->BindTextureSet("textures", m_Textures);
    }

    void MeshRenderer::Retire(SShaderVariant&& variant)
    {
        if (variant.Shader || variant.Opaque || variant.Transparent || variant.ParamBuffer)
            m_Retired.push_back({ std::move(variant), RETIRE_FRAMES });
    }

    void MeshRenderer::RetireBuffer(Ref<Buffer> buffer)
    {
        if (buffer)
            m_RetiredBuffers.push_back({ std::move(buffer), RETIRE_FRAMES });
    }

    void MeshRenderer::RegisterModel(const Ref<Model>& model)
    {
        UpdateModel(model);
    }

    void MeshRenderer::UpdateModel(const Ref<Model>& model)
    {
        if (model)
            m_RenderScene.QueueUpdate(model->CreateSceneProxy());
    }

    void MeshRenderer::UnregisterModel(const SModelRenderHandle handle)
    {
        if (!handle.IsValid())
            return;
        m_RenderScene.QueueRemove(handle);
    }

    void MeshRenderer::RetireModelVariants(const SModelRenderHandle model)
    {
        for (auto variant = m_MaterialShaders.begin();
            variant != m_MaterialShaders.end();)
        {
            if (variant->first.Model != model)
            {
                ++variant;
                continue;
            }
            Retire(std::move(variant->second));
            variant = m_MaterialShaders.erase(variant);
        }

        for (auto cache = m_MaterialShaderCache.begin();
            cache != m_MaterialShaderCache.end();)
        {
            if (cache->first.Model != model)
            {
                ++cache;
                continue;
            }
            for (auto& [key, variant] : cache->second.Variants)
                Retire(std::move(variant));
            cache = m_MaterialShaderCache.erase(cache);
        }

        for (auto revision = m_MaterialDrawBindingRevisions.begin();
            revision != m_MaterialDrawBindingRevisions.end();)
        {
            if (revision->first.Model == model)
                revision = m_MaterialDrawBindingRevisions.erase(revision);
            else
                ++revision;
        }

        std::lock_guard<std::mutex> keyLock(m_VariantKeysMutex);
        for (auto keys = m_VariantKeys.begin(); keys != m_VariantKeys.end();)
        {
            if (keys->first.Model == model)
                keys = m_VariantKeys.erase(keys);
            else
                ++keys;
        }
    }

    void MeshRenderer::RetireModelState(const SModelRenderHandle handle)
    {
        const auto state = m_ModelRenderStates.find(handle);
        if (state != m_ModelRenderStates.end())
        {
            m_RetiredModelStates.push_back(
                { std::move(state->second), RETIRE_FRAMES });
            m_ModelRenderStates.erase(state);
        }
        RetireModelVariants(handle);
    }

    void MeshRenderer::ProcessModelLifecycle()
    {
        for (const SModelRenderHandle handle : m_RenderScene.ApplyPending())
            RetireModelState(handle);

        for (const Ref<const MeshSceneProxy>& proxy : m_RenderScene.GetProxies())
            m_ModelRenderStates.try_emplace(proxy->GetHandle());
    }

    void MeshRenderer::TickRetired()
    {
        for (auto& r : m_Retired)
            --r.FramesLeft;
        m_Retired.erase(
            std::remove_if(m_Retired.begin(), m_Retired.end(), [](const SRetired& r) { return r.FramesLeft <= 0; }),
            m_Retired.end());
        for (auto& retired : m_RetiredBuffers)
            --retired.FramesLeft;
        m_RetiredBuffers.erase(
            std::remove_if(m_RetiredBuffers.begin(), m_RetiredBuffers.end(),
                [](const SRetiredBuffer& retired) { return retired.FramesLeft <= 0; }),
            m_RetiredBuffers.end());
        for (auto& retired : m_RetiredModelStates)
            --retired.FramesLeft;
        m_RetiredModelStates.erase(
            std::remove_if(m_RetiredModelStates.begin(), m_RetiredModelStates.end(),
                [](const SRetiredModelState& retired) { return retired.FramesLeft <= 0; }),
            m_RetiredModelStates.end());
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
        if (m_SceneMaterialBuffer)
            m_Shader->BindStorageBuffer("materials", m_SceneMaterialBuffer);
        InvalidateDefaultDrawCommands();
    }

    void MeshRenderer::RetireVariantCache(const SModelMaterialHandle material)
    {
        if (const auto it = m_MaterialShaderCache.find(material);
            it != m_MaterialShaderCache.end())
        {
            for (auto& [key, variant] : it->second.Variants)
                Retire(std::move(variant));
            m_MaterialShaderCache.erase(it);
        }
        std::lock_guard<std::mutex> keyLock(m_VariantKeysMutex);
        m_VariantKeys.erase(material);
    }

    void MeshRenderer::SetMaterialShader(const SModelRenderHandle model,
        const uint32_t materialIndex, const Ref<Shader>& shader,
        const EMaterialBlendMode blendMode)
    {
        const SModelMaterialHandle material{ model, materialIndex };
        if (!shader)
        {
            if (const auto it = m_MaterialShaders.find(material);
                it != m_MaterialShaders.end())
            {
                Retire(std::move(it->second));
                m_MaterialShaders.erase(it);
                InvalidateMaterialDrawCommands(material);
            }
            RetireVariantCache(material);
            return;
        }

        SShaderVariant variant;
        variant.Shader = shader;
        variant.BlendMode = blendMode;
        variant.Permutation = MaterialShaderPermutation::SurfaceStaticMesh();
        CreatePipelinesFor(shader, variant.Opaque, variant.Transparent, blendMode);
        BindResourcesTo(shader);

        // Zero-initialised live-parameter buffer (cbGraphParams), bound once.
        glm::vec4 zeros[MAX_GRAPH_PARAMS] = {};
        variant.ParamBuffer = UniformBuffer::Create(m_GraphicsContext, sizeof(zeros), zeros);
        shader->BindConstantBuffer("cbGraphParams", variant.ParamBuffer);
        if (m_SceneMaterialBuffer)
            shader->BindStorageBuffer("materials", m_SceneMaterialBuffer);

        // Retire the slot's previous variant (may be in flight), then install the new.
        if (const auto it = m_MaterialShaders.find(material);
            it != m_MaterialShaders.end())
            Retire(std::move(it->second));
        // This shader is set outside the graph path, so any permutation cached for the
        // slot is now unreachable.
        RetireVariantCache(material);
        m_MaterialShaders[material] = std::move(variant);
        InvalidateMaterialDrawCommands(material);
    }

    void MeshRenderer::PrepareMaterialShader(const SModelRenderHandle model,
        const uint32_t materialIndex, const Ref<Shader>& shader,
        const EMaterialBlendMode blendMode, const SMaterialShaderPermutation& permutation,
        const Ref<const MaterialRenderProxy>& proxy,
        const size_t revision, const size_t variantKey)
    {
        if (!shader)
            return;
        if (permutation != MaterialShaderPermutation::SurfaceStaticMesh()
            || !proxy || !proxy->GetResource()
            || !proxy->GetResource()->Matches(permutation, revision, variantKey))
        {
            EE_CORE_ERROR("MeshRenderer rejected an incompatible material shader permutation.")
            return;
        }

        // Built here on the caller's worker thread -- including the slow pipeline
        // creation -- so the render thread never blocks compiling a Metal pipeline.
        SShaderVariant variant;
        variant.Shader = shader;
        variant.BlendMode = blendMode;
        variant.Permutation = permutation;
        variant.Revision = revision;
        variant.VariantKey = variantKey;
        variant.Proxy = proxy;
        CreatePipelinesFor(shader, variant.Opaque, variant.Transparent, blendMode);
        BindResourcesTo(shader);

        glm::vec4 zeros[MAX_GRAPH_PARAMS] = {};
        variant.ParamBuffer = UniformBuffer::Create(m_GraphicsContext, sizeof(zeros), zeros);
        shader->BindConstantBuffer("cbGraphParams", variant.ParamBuffer);

        std::lock_guard<std::mutex> lock(m_PendingMutex);
        m_PendingVariants.push_back({ model, materialIndex, std::move(variant), proxy,
            permutation, revision, variantKey, false });
    }

    bool MeshRenderer::HasMaterialShaderVariant(
        const SModelRenderHandle model, const uint32_t materialIndex,
        const SMaterialShaderPermutation& permutation,
        const size_t revision, const size_t variantKey) const
    {
        if (permutation != MaterialShaderPermutation::SurfaceStaticMesh())
            return false;
        std::lock_guard<std::mutex> lock(m_VariantKeysMutex);
        const auto slot = m_VariantKeys.find({ model, materialIndex });
        return slot != m_VariantKeys.end() && slot->second.Revision == revision
            && slot->second.Keys.contains(variantKey);
    }

    void MeshRenderer::PrepareCachedMaterialShader(const SModelRenderHandle model,
        const uint32_t materialIndex,
        const SMaterialShaderPermutation& permutation, const size_t revision,
        const size_t variantKey, const Ref<const MaterialRenderProxy>& proxy)
    {
        if (permutation != MaterialShaderPermutation::SurfaceStaticMesh()
            || !proxy || !proxy->GetResource()
            || !proxy->GetResource()->Matches(permutation, revision, variantKey))
        {
            EE_CORE_ERROR("MeshRenderer rejected an incompatible cached material permutation.")
            return;
        }
        std::lock_guard<std::mutex> lock(m_PendingMutex);
        m_PendingVariants.push_back({ model, materialIndex, {}, proxy,
            permutation, revision, variantKey, true });
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
            if (!m_ModelRenderStates.contains(pending.Model))
            {
                Retire(std::move(pending.Variant));
                continue;
            }

            const SModelMaterialHandle material{ pending.Model, pending.Slot };
            const auto active = m_MaterialShaders.find(material);
            if (active != m_MaterialShaders.end() && active->second.Revision == pending.Revision
                && active->second.VariantKey == pending.VariantKey
                && active->second.Permutation == pending.Permutation)
            {
                // A runtime-only parameter edit does not require another shader swap.
                // Replace just its immutable snapshot at this frame boundary.
                active->second.Proxy = pending.Proxy;
                continue;
            }

            // Editing the graph mints a new revision, and no permutation of the old one
            // can ever be selected again -- release them instead of caching them for a
            // key nothing will ask for.
            auto& cache = m_MaterialShaderCache[material];
            if (cache.Revision != pending.Revision)
            {
                for (auto& [key, variant] : cache.Variants)
                    Retire(std::move(variant));
                cache.Variants.clear();
                cache.Revision = pending.Revision;

                std::lock_guard<std::mutex> keyLock(m_VariantKeysMutex);
                SVariantKeys& keys = m_VariantKeys[material];
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
                selected.Proxy = pending.Proxy;
                if (active != m_MaterialShaders.end())
                {
                    cache.Variants.insert_or_assign(active->second.VariantKey, std::move(active->second));
                    m_MaterialShaders.erase(active);
                }
                if (m_SceneMaterialBuffer)
                    selected.Shader->BindStorageBuffer(
                        "materials", m_SceneMaterialBuffer);
                m_MaterialShaders[material] = std::move(selected);
                InvalidateMaterialDrawCommands(material);
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
            if (m_SceneMaterialBuffer)
                pending.Variant.Shader->BindStorageBuffer(
                    "materials", m_SceneMaterialBuffer);
            m_MaterialShaders[material] = std::move(pending.Variant);
            InvalidateMaterialDrawCommands(material);
            {
                std::lock_guard<std::mutex> keyLock(m_VariantKeysMutex);
                SVariantKeys& keys = m_VariantKeys[material];
                keys.Revision = pending.Revision;
                keys.Keys.insert(pending.VariantKey);
            }
        }
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

    MeshRenderer::SGPUMaterial MeshRenderer::PackMaterial(
        const Ref<const MaterialRenderProxy>& material)
    {
        if (!material)
            return {};

        const uint32_t alphaMode = (uint32_t)material->GetScalar("AlphaMode");
        const glm::vec4 baseColor = material->GetVector("BaseColorFactor");

        SGPUMaterial gpu{};
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
        return gpu;
    }

    Ref<StorageBuffer> MeshRenderer::CreateMaterialBuffer(
        const std::vector<SGPUMaterial>& materials)
    {
        return StorageBuffer::Create(
            m_GraphicsContext, materials.size() * sizeof(SGPUMaterial), materials.data());
    }

    void MeshRenderer::InvalidateDefaultDrawCommands()
    {
        m_DefaultDrawBindingRevision = ++m_NextDrawBindingRevision;
    }

    void MeshRenderer::InvalidateMaterialDrawCommands(
        const SModelMaterialHandle material)
    {
        m_MaterialDrawBindingRevisions[material] = ++m_NextDrawBindingRevision;
    }

    SMeshDrawCommand MeshRenderer::BuildDrawCommand(
        const SModelRenderHandle model, const uint32_t materialOffset,
        const SModelPrimitive& primitive,
        const MaterialGPUParameterCache::ProxyList& materialProxies) const
    {
        const Ref<const MaterialRenderProxy> material =
            primitive.MaterialIndex < materialProxies.size()
            ? materialProxies[primitive.MaterialIndex] : nullptr;

        if (const auto variant = m_MaterialShaders.find(
                { model, primitive.MaterialIndex });
            variant != m_MaterialShaders.end())
        {
            const SMeshMaterialBinding binding{
                .Shader = variant->second.Shader,
                .BasePipeline = variant->second.Opaque,
                .TranslucentPipeline = variant->second.Transparent,
                .BlendMode = variant->second.BlendMode
            };
            SMeshDrawCommand command = MeshPassProcessor::BuildCommand(
                primitive, material, m_Shader, m_Pipeline, &binding);
            command.PushConstants.MaterialIndex += materialOffset;
            return command;
        }

        SMeshDrawCommand command = MeshPassProcessor::BuildCommand(
            primitive, material, m_Shader, m_Pipeline);
        command.PushConstants.MaterialIndex += materialOffset;
        return command;
    }

    void MeshRenderer::UpdateDrawCommands(const SModelRenderHandle handle,
        const MeshSceneProxy& proxy,
        const MaterialGPUParameterCache::ProxyList& materialProxies,
        SModelRenderState& renderState)
    {
        if (renderState.PrimitiveSnapshot != proxy.GetPrimitiveSnapshot())
        {
            renderState.PrimitiveSnapshot = proxy.GetPrimitiveSnapshot();
            renderState.DrawCommandCache.Clear();
            renderState.DrawCommands.clear();
        }

        MaterialDrawCommandCache::SlotList slots;
        slots.reserve(materialProxies.size());
        for (uint32_t slot = 0; slot < materialProxies.size(); ++slot)
        {
            const SModelMaterialHandle material{ handle, slot };
            uint64_t bindingRevision = m_DefaultDrawBindingRevision;
            std::optional<EMaterialBlendMode> overrideBlendMode;
            if (const auto variant = m_MaterialShaders.find(material);
                variant != m_MaterialShaders.end())
            {
                if (const auto revision = m_MaterialDrawBindingRevisions.find(material);
                    revision != m_MaterialDrawBindingRevisions.end())
                {
                    bindingRevision = revision->second;
                }
                overrideBlendMode = variant->second.BlendMode;
            }
            slots.push_back({ materialProxies[slot]
                    ? materialProxies[slot]->GetResource() : nullptr,
                bindingRevision,
                MeshPassProcessor::Classify(
                    materialProxies[slot], overrideBlendMode) });
        }

        const MaterialDrawCommandCache::SUpdate update =
            renderState.DrawCommandCache.Update(std::move(slots));
        const auto& primitives = proxy.GetPrimitives();
        const bool rebuildAll = update.LayoutChanged
            || renderState.DrawCommands.size() != primitives.size();
        if (!rebuildAll && update.DirtySlots.empty())
            return;

        std::unordered_set<uint32_t> dirtySlots(
            update.DirtySlots.begin(), update.DirtySlots.end());
        if (renderState.DrawCommands.size() != primitives.size())
            renderState.DrawCommands.resize(primitives.size());

        bool rebuildPassLists = rebuildAll;
        for (uint32_t commandIndex = 0; commandIndex < primitives.size(); ++commandIndex)
        {
            const SModelPrimitive& primitive = primitives[commandIndex];
            if (!rebuildAll && !dirtySlots.contains(primitive.MaterialIndex))
                continue;

            const EMeshPass previousPass = renderState.DrawCommands[commandIndex].Pass;
            renderState.DrawCommands[commandIndex] =
                BuildDrawCommand(handle, renderState.MaterialOffset,
                    primitive, materialProxies);
            rebuildPassLists = rebuildPassLists
                || previousPass != renderState.DrawCommands[commandIndex].Pass;
        }

        if (!rebuildPassLists)
            return;

        renderState.BasePassCommands.clear();
        renderState.SortedPassCommands.clear();
        renderState.BasePassCommands.reserve(renderState.DrawCommands.size());
        renderState.SortedPassCommands.reserve(renderState.DrawCommands.size());
        for (uint32_t commandIndex = 0;
            commandIndex < renderState.DrawCommands.size(); ++commandIndex)
        {
            if (MeshPassProcessor::IsBasePass(
                    renderState.DrawCommands[commandIndex].Pass))
                renderState.BasePassCommands.push_back(commandIndex);
            else
                renderState.SortedPassCommands.push_back(commandIndex);
        }
    }

    void MeshRenderer::Render(const Camera& camera)
    {
        // Registration/removal and prepared shader swaps become visible together at a
        // frame boundary. Workers remain free to compile the next material while this
        // render thread consumes immutable model and material snapshots.
        TickRetired();
        ProcessModelLifecycle();
        InstallPendingShaders();

        std::vector<Ref<const MeshSceneProxy>> liveProxies;
        liveProxies.reserve(m_RenderScene.GetProxies().size());
        bool sceneMaterialsDirty = false;

        for (const Ref<const MeshSceneProxy>& sceneProxy :
            m_RenderScene.GetProxies())
        {
            const SModelRenderHandle handle = sceneProxy->GetHandle();
            const auto stateIt = m_ModelRenderStates.find(handle);
            if (stateIt == m_ModelRenderStates.end())
                continue;

            const Ref<const MeshSceneProxy::MaterialList>& publishedProxies =
                sceneProxy->GetMaterials();
            if (!publishedProxies)
                continue;
            MaterialGPUParameterCache::ProxyList effectiveProxies =
                *publishedProxies;

            // A slot number only has meaning inside its model. Adopt runtime values
            // only from the proxy compatible with this model-scoped shader variant.
            for (auto& [material, variant] : m_MaterialShaders)
            {
                if (material.Model != handle)
                    continue;
                Ref<const MaterialRenderProxy> proxy = variant.Proxy;
                if (material.Slot < publishedProxies->size())
                {
                    const Ref<const MaterialRenderProxy>& published =
                        (*publishedProxies)[material.Slot];
                    if (published && published->GetResource()
                        && published->GetResource()->GetBlendMode() == variant.BlendMode
                        && published->GetResource()->Matches(
                            variant.Permutation, variant.Revision,
                            variant.VariantKey))
                    {
                        proxy = published;
                        variant.Proxy = published;
                    }
                }
                if (!proxy)
                    continue;
                if (material.Slot >= effectiveProxies.size())
                    effectiveProxies.resize(material.Slot + 1);
                effectiveProxies[material.Slot] = std::move(proxy);
            }

            SModelRenderState& state = stateIt->second;
            const MaterialGPUParameterCache::SUpdate parameterUpdate =
                state.Parameters.Update(std::move(effectiveProxies));
            const auto& proxies = state.Parameters.GetProxies();
            if (parameterUpdate.LayoutChanged)
                state.PackedMaterials.resize(proxies.size());
            for (const uint32_t slot : parameterUpdate.DirtySlots)
                state.PackedMaterials[slot] = PackMaterial(proxies[slot]);
            sceneMaterialsDirty = sceneMaterialsDirty
                || parameterUpdate.HasChanges();
            liveProxies.push_back(sceneProxy);
        }

        // Assign stable contiguous ranges in registration order. A changed range
        // invalidates only that model's cached commands because their push constant
        // stores the scene-global material index.
        uint32_t materialCount = 0;
        for (const Ref<const MeshSceneProxy>& proxy : liveProxies)
        {
            const SModelRenderHandle handle = proxy->GetHandle();
            SModelRenderState& state = m_ModelRenderStates.at(handle);
            if (state.MaterialOffset != materialCount)
            {
                state.MaterialOffset = materialCount;
                state.DrawCommandCache.Clear();
                sceneMaterialsDirty = true;
            }
            materialCount += (uint32_t)state.PackedMaterials.size();
        }
        sceneMaterialsDirty = sceneMaterialsDirty
            || materialCount != m_PackedSceneMaterials.size();

        if (sceneMaterialsDirty || (!m_SceneMaterialBuffer && materialCount > 0))
        {
            m_PackedSceneMaterials.clear();
            m_PackedSceneMaterials.reserve(materialCount);
            for (const Ref<const MeshSceneProxy>& proxy : liveProxies)
            {
                const SModelRenderHandle handle = proxy->GetHandle();
                const auto& packed = m_ModelRenderStates.at(handle).PackedMaterials;
                m_PackedSceneMaterials.insert(
                    m_PackedSceneMaterials.end(), packed.begin(), packed.end());
            }

            Ref<StorageBuffer> previous = std::move(m_SceneMaterialBuffer);
            if (!m_PackedSceneMaterials.empty())
            {
                m_SceneMaterialBuffer =
                    CreateMaterialBuffer(m_PackedSceneMaterials);
                m_Shader->BindStorageBuffer(
                    "materials", m_SceneMaterialBuffer);
                for (auto& [material, variant] : m_MaterialShaders)
                    variant.Shader->BindStorageBuffer(
                        "materials", m_SceneMaterialBuffer);
            }
            RetireBuffer(std::move(previous));
        }
        if (!m_SceneMaterialBuffer)
            return;

        // Graph parameters remain model/slot-local even though the standard material
        // table is scene-wide. Every graph variant owns its immutable parameter buffer.
        for (auto& [material, variant] : m_MaterialShaders)
        {
            const auto state = m_ModelRenderStates.find(material.Model);
            if (state == m_ModelRenderStates.end())
                continue;
            const auto& proxies = state->second.Parameters.GetProxies();
            if (material.Slot >= proxies.size() || !proxies[material.Slot]
                || variant.UploadedProxy == proxies[material.Slot])
                continue;
            glm::vec4 params[MAX_GRAPH_PARAMS] = {};
            proxies[material.Slot]->CollectGraphParams(
                params, MAX_GRAPH_PARAMS,
                [this](const Ref<Texture>& texture)
                {
                    return ResolveTexture(texture);
                });

            Ref<UniformBuffer> previous = std::move(variant.ParamBuffer);
            variant.ParamBuffer = UniformBuffer::Create(
                m_GraphicsContext, sizeof(params), params);
            variant.Shader->BindConstantBuffer(
                "cbGraphParams", variant.ParamBuffer);
            variant.UploadedProxy = proxies[material.Slot];
            RetireBuffer(std::move(previous));
        }

        std::vector<const SMeshDrawCommand*> baseCommands;
        std::vector<const SMeshDrawCommand*> sortedCommands;
        for (const Ref<const MeshSceneProxy>& proxy : liveProxies)
        {
            const SModelRenderHandle handle = proxy->GetHandle();
            SModelRenderState& state = m_ModelRenderStates.at(handle);
            UpdateDrawCommands(
                handle, *proxy, state.Parameters.GetProxies(), state);
            baseCommands.reserve(
                baseCommands.size() + state.BasePassCommands.size());
            sortedCommands.reserve(
                sortedCommands.size() + state.SortedPassCommands.size());
            for (const uint32_t command : state.BasePassCommands)
                baseCommands.push_back(&state.DrawCommands[command]);
            for (const uint32_t command : state.SortedPassCommands)
                sortedCommands.push_back(&state.DrawCommands[command]);
        }
        if (baseCommands.empty() && sortedCommands.empty())
            return;

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

        // The pipeline currently bound on the command buffer, so consecutive cached
        // commands sharing a shader don't rebind. Reset at each pass.
        GraphicsPipeline* boundPipeline = nullptr;
        const auto drawCommand = [&](const SMeshDrawCommand& draw)
        {
            if (draw.Pipeline.get() != boundPipeline)
            {
                draw.Pipeline->Bind(cmd);
                boundPipeline = draw.Pipeline.get();
            }

            draw.Shader->SetPushConstant(
                cmd, "pc", (void*)&draw.PushConstants, PUSH_CONSTANT_SIZE);

            draw.Vertices->Bind(cmd);
            draw.Indices->Bind(cmd);
            cmd->DrawIndexed(draw.IndexCount, 1, 0, 0, 0);
        };

        // Base pass: opaque, masked, and near-opaque built-in materials. The pass
        // processor guarantees that every command in this list writes depth.
        boundPipeline = nullptr;
        for (const SMeshDrawCommand* draw : baseCommands)
            drawCommand(*draw);
        cmd->EndRendering();

        if (sortedCommands.empty())
        {
            m_GraphicsContext->EnqueueSecondaryCommandBuffer(cmd);
            return;
        }

        // Grab the opaque result so the glass can sample the scene behind it.
        m_GraphicsContext->BlitToTexture(cmd, m_SceneColor);

        // Sorted pass: graph translucency uses alpha/additive blending without depth
        // writes, while built-in refraction composites the grabbed scene through a
        // depth-writing pipeline. Load the base depth so both are occluded correctly.
        SRenderingInfo glassInfo = renderingInfo;
        glassInfo.LoadDepthStencil = true;
        cmd->BeginRendering(glassInfo);
        cmd->SetViewports({ viewport });
        cmd->SetScissors({ scissor });
        boundPipeline = nullptr;

        // Sort translucent and refractive primitives back-to-front.
        // The primitive's world transform origin is a cheap depth proxy (exact
        // per-triangle sorting is out of scope); good enough for separated panels.
        const glm::vec3 camPos = camera.GetPosition();
        const auto depthProxy = [&](const SMeshDrawCommand* draw)
        {
            const glm::vec3 d = draw->SortOrigin - camPos;
            return glm::dot(d, d);
        };
        std::sort(sortedCommands.begin(), sortedCommands.end(),
            [&](const SMeshDrawCommand* a, const SMeshDrawCommand* b)
            {
                return depthProxy(a) > depthProxy(b);
            });
        for (const SMeshDrawCommand* draw : sortedCommands)
            drawCommand(*draw);
        cmd->EndRendering();

        m_GraphicsContext->EnqueueSecondaryCommandBuffer(cmd);
    }
}
