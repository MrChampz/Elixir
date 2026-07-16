#include "Dissolve.h"

#include <Engine/Core/Entrypoint.h>
#include <Engine/Graphics/SamplerBuilder.h>
#include <Engine/Aether/Renderer.h>

#include "Engine/Aether/Effect.h"
#include "Engine/Graphics/Material/MaterialGraph.h"
#include "Engine/Graphics/Material/MaterialCompiler.h"

#include <imgui.h>

Ref<GraphicsPipeline> pipeline;
Scope<Aether::Renderer> m_ParticlesRenderer;
Ref<Aether::System> m_ParticleSystem;
Aether::SGPUSystem m_GPUSystem;

Dissolve::Dissolve()
{
    EE_PROFILE_ZONE_SCOPED()

    m_Window->SetTitle("Dissolve");
    m_DrawExtent = m_Window->GetFramebufferExtent();

    const auto aspectRatio = (float)m_DrawExtent.Width / (float)m_DrawExtent.Height;
    // m_CameraController = CreateScope<SplineCameraController>(aspectRatio);
    // m_CameraController->AddKeyframe({ { 0.0f, 0.5f, 5.0f }, { 0.0f, 0.0f, 0.0f }, 60.0f });
    // m_CameraController->AddKeyframe({ { 1.0f, 0.3f, 2.5f }, { 0.0f, 0.0f, 0.0f }, 50.0f });
    // m_CameraController->AddKeyframe({ { 0.0f, 0.0f, 1.5f }, { 0.0f, 0.0f, 0.0f }, 40.0f });
    // m_CameraController->SetDuration(4.0f);
    // m_CameraController->SetLooping(true);
    // m_CameraController->Play();
    m_CameraController = CreateScope<ArcBallCameraController>(60.0f, aspectRatio);
    m_FrameData.ViewProj = m_CameraController->GetCamera().GetViewProjectionMatrix();

    const auto sampler = SamplerBuilder()
        .Build(m_GraphicsContext.get());

    const auto tex = TextureLoader::Load("./Assets/Bricks.png");

    const auto shader = m_ShaderLoader->LoadShader("./Shaders/", "FixedTriangle");
    shader->BindTexture("texture", tex);
    shader->BindSampler("sampl", sampler);

    PipelineBuilder builder;
    builder.SetShader(shader);
    builder.SetInputTopology(EPrimitiveTopology::TriangleList);
    builder.SetPolygonMode(EPolygonMode::Fill);
    builder.DisableBlending();
    builder.DisableDepthTest();
    builder.SetColorAttachmentFormat(EImageFormat::R8G8B8A8_SRGB);
    builder.SetBufferLayout({});
    pipeline = builder.Build(m_GraphicsContext.get());

    m_FrameConstantBuffer = UniformBuffer::Create(
        m_GraphicsContext.get(),
        sizeof(SFrameData),
        &m_FrameData
    );

    shader->BindConstantBuffer("cbFrame", m_FrameConstantBuffer);

    m_ParticlesRenderer = CreateScope<Aether::Renderer>(m_GraphicsContext.get(), m_ShaderLoader.get());

    m_MeshRenderer = CreateScope<MeshRenderer>(m_GraphicsContext.get(), m_ShaderLoader.get());
    m_PostProcessor = CreateScope<PostProcessor>(m_GraphicsContext.get(), m_ShaderLoader.get());
    m_GraphicsContext->InitImGui();
    m_Model = Model::Load(m_GraphicsContext.get(), "./Assets/Meshes/McLaren/scene.mesh.json");
    LoadMaterialAssets();

    m_ParticleSystem = Aether::LoadEffectFile("./Assets/VFX/RibbonVortex.json");
    // m_ParticleSystem = CreateScope<Aether::System>("Ribbon Garden");
    // m_ParticleSystem->GetParameters().SetFloat("GravityScale", 1.0f);
    //
    // auto& canopy = m_ParticleSystem->AddEmitter("CanopyMist", 4096, 160.0f);
    // canopy.AddSpawnModule<Aether::SetPositionDisk>(glm::vec3{ 0.0f, -0.86f, 0.0f }, 0.18f);
    // canopy.AddSpawnModule<Aether::SetVelocityCone>(glm::vec3{ 0.0f, 1.0f, 0.0f }, 0.62f, 0.24f, 0.56f);
    // canopy.AddSpawnModule<Aether::SetLifetime>(3.8f, 6.2f);
    // canopy.AddSpawnModule<Aether::SetSize>(14.0f, 28.0f);
    // canopy.AddSpawnModule<Aether::SetScale>(0.72f, 1.08f);
    // canopy.AddSpawnModule<Aether::SetColor>(glm::vec4{ 0.70f, 0.96f, 1.0f, 0.72f });
    //
    // canopy.AddUpdateModule<Aether::ApplyGravity>(glm::vec3{ 0.0f, -0.10f, 0.0f });
    // canopy.AddUpdateModule<Aether::ApplyLinearDrag>(0.04f);
    // canopy.AddUpdateModule<Aether::ColorOverLife>(glm::vec4{ 0.70f, 0.96f, 1.0f, 0.78f }, glm::vec4{ 0.4f, 0.18f, 0.72f, 0.0f });
    // canopy.AddUpdateModule<Aether::SizeOverLife>(28.0f, 6.0f);
    // canopy.AddUpdateModule<Aether::KillOutsideBounds>(glm::vec3{ -1.45f, -1.2f, -2.0f }, glm::vec3{ 1.45f, 1.35f, 2.0f });
    //
    // auto& sparks = m_ParticleSystem->AddEmitter("RoseSparks", 2048, 120.0f);
    // sparks.AddSpawnModule<Aether::SetPositionDisk>(glm::vec3{ 0.0f, -0.8f, 0.0f }, 0.08f);
    // sparks.AddSpawnModule<Aether::SetVelocityCone>(glm::vec3{ 0.0f, 1.0f, 0.0f }, 0.96f, 0.36f, 0.84f);
    // sparks.AddSpawnModule<Aether::SetLifetime>(1.8f, 3.0f);
    // sparks.AddSpawnModule<Aether::SetSize>(8.0f, 16.0f);
    // sparks.AddSpawnModule<Aether::SetColor>(glm::vec4{ 1.0f, 0.68f, 0.88f, 0.95f });
    //
    // sparks.AddUpdateModule<Aether::ApplyGravity>(glm::vec3{ 0.0f, -0.28f, 0.0f });
    // sparks.AddUpdateModule<Aether::ApplyLinearDrag>(0.08f);
    // sparks.AddUpdateModule<Aether::ColorOverLife>(glm::vec4{ 1.0f, 0.72f, 0.9f, 0.95f }, glm::vec4{ 1.0f, 0.36f, 0.48f, 0.0f });
    // sparks.AddUpdateModule<Aether::SizeOverLife>(16.0f, 2.5f);
    // sparks.AddUpdateModule<Aether::KillOutsideBounds>(glm::vec3{ -1.45f, -1.2f, -2.0f }, glm::vec3{ 1.45f, 1.35f, 2.0f });
    //
    // auto& sparks2 = m_ParticleSystem->AddEmitter("GreenSparks", 2048, 120.0f);
    // sparks2.AddSpawnModule<Aether::SetPositionDisk>(glm::vec3{ 0.0f, -0.8f, 0.0f }, 0.08f);
    // sparks2.AddSpawnModule<Aether::SetVelocityCone>(glm::vec3{ 0.0f, 1.0f, 0.0f }, 0.96f, 0.36f, 0.84f);
    // sparks2.AddSpawnModule<Aether::SetLifetime>(1.8f, 3.0f);
    // sparks2.AddSpawnModule<Aether::SetSize>(8.0f, 16.0f);
    // sparks2.AddSpawnModule<Aether::SetScale>(0.9f, 1.35f);
    // sparks2.AddSpawnModule<Aether::SetColor>(glm::vec4{ 1.0f, 0.68f, 0.88f, 0.95f });
    //
    // sparks2.AddUpdateModule<Aether::ApplyGravity>(glm::vec3{ 0.0f, -0.28f, 0.0f });
    // sparks2.AddUpdateModule<Aether::ApplyLinearDrag>(0.08f);
    // sparks2.AddUpdateModule<Aether::ColorOverLife>(glm::vec4{ 0.0f, 0.73f, 0.12f, 0.8f }, glm::vec4{ 0.0f, 0.73f, 0.12f, 0.0f });
    // sparks2.AddUpdateModule<Aether::SizeOverLife>(16.0f, 2.5f);
    // sparks2.AddUpdateModule<Aether::ScaleOverLife>(1.0f, 0.35f);
    // sparks2.AddUpdateModule<Aether::KillOutsideBounds>(glm::vec3{ -1.45f, -1.2f, -2.0f }, glm::vec3{ 1.45f, 1.35f, 2.0f });
    //
    // constexpr uint32_t ribbonParticles = 256;
    // constexpr float ribbonLifetime = 4.0f;
    // constexpr float ribbonSpawnRate = (float)ribbonParticles / ribbonLifetime;
    //
    // auto& ribbon = m_ParticleSystem->AddEmitter("AuroraRibbon", ribbonParticles, ribbonSpawnRate);
    // ribbon.SetRenderMode(Aether::EParticleRenderMode::Ribbon);
    // // ribbon.AddSpawnModule<Aether::SetPositionCircularPath>(
    // //     glm::vec3{ 0.0f, -0.25f, 0.0f },
    // //     glm::vec3{ 1.05f, 0.38f, 0.28f },
    // //     glm::vec3{ 0.32f, 0.16f, 0.18f },
    // //     1.15f
    // // );
    // ribbon.AddSpawnModule<Aether::SetPositionOnCircle>(glm::vec3{ 0.0f, 0.0f, 0.0f }, 2.0f, 1.0f);
    // ribbon.AddSpawnModule<Aether::SetLifetime>(ribbonLifetime, ribbonLifetime);
    // ribbon.AddSpawnModule<Aether::SetSize>(8.0f, 8.0f);
    // ribbon.AddSpawnModule<Aether::SetColor>(glm::vec4{ 0.55f, 0.92f, 1.0f, 0.95f });
    //
    // ribbon.AddUpdateModule<Aether::ColorOverLife>(glm::vec4{ 0.55f, 0.92f, 1.0f, 0.95f }, glm::vec4{ 0.78f, 0.36f, 1.0f, 0.0f });
    //
    // auto& shards = m_ParticleSystem->AddEmitter("CrystalShards", 220, 26.0f);
    // shards.SetRenderMode(Aether::EParticleRenderMode::Mesh);
    // shards.AddSpawnModule<Aether::SetPositionDisk>(glm::vec3{ 0.0f, -0.64f, 0.0f }, 0.22f);
    // shards.AddSpawnModule<Aether::SetVelocityCone>(glm::vec3{ 0.94f }, 2.20f, 0.14f, 0.36f);
    // shards.AddSpawnModule<Aether::SetLifetime>(2.6f, 3.8f);
    // shards.AddSpawnModule<Aether::SetSize>(10.0f, 18.0f);
    // shards.AddSpawnModule<Aether::SetScale>(0.82f, 1.45f);
    // shards.AddSpawnModule<Aether::SetRotation>(0.0f, 6.28318530718f);
    // shards.AddSpawnModule<Aether::SetColor>(glm::vec4{ 0.86f, 0.94f, 1.0f, 0.62f });
    //
    // shards.AddUpdateModule<Aether::ApplyGravity>(glm::vec3{ 0.0f, -0.28f, 0.0f });
    // shards.AddUpdateModule<Aether::ApplyLinearDrag>(0.03f);
    // shards.AddUpdateModule<Aether::ApplyAngularVelocity>(1.2f);
    // shards.AddUpdateModule<Aether::SizeOverLife>(18.0f, 3.0f);
    // shards.AddUpdateModule<Aether::ScaleOverLife>(1.15f, 0.28f);
    // shards.AddUpdateModule<Aether::KillOutsideBounds>(glm::vec3{ -1.45f, -1.2f, -1.45f }, glm::vec3{ 1.45f, 1.35f, 1.45f });

    m_GPUSystem = m_ParticleSystem->Build();

    m_GraphicsContext->SetClearColor({ 0.015f, 0.025f, 0.06f, 1.0f });
}

Dissolve::~Dissolve()
{
    pipeline.reset();
}

void Dissolve::StartGraphCompile()
{
    m_Compiling = true;
    const uint32_t compileSlot = (uint32_t)m_GraphEditor.TargetMaterial();

    Ref<Material> baseMaterial;
    {
        std::lock_guard<std::mutex> lock(m_Model->MaterialsMutex());
        const auto& materials = m_Model->GetMaterials();
        if (compileSlot < materials.size())
            baseMaterial = materials[compileSlot]->GetParent();
    }
    const Ref<Material> compileMaterial = m_GraphEditor.BuildMaterial(baseMaterial);
    const auto compileInstance = CreateRef<MaterialInstance>(compileMaterial);
    m_GraphEditor.ApplyParametersTo(*compileInstance);
    QueueGraphCompile(compileSlot, compileInstance, true);
}

void Dissolve::QueueGraphCompile(
    const uint32_t slot, const Ref<MaterialInstance>& instance, const bool notifyEditor)
{
    if (!instance || !instance->GetParent() || !instance->GetParent()->HasGraph())
        return;
    const MaterialGraph compileGraph = *instance->GetParent()->GetGraph();
    const EMaterialBlendMode compileBlend = compileGraph.GetBlendMode();
    uint64_t generation;
    {
        std::lock_guard<std::mutex> lock(m_GraphGenerationMutex);
        generation = ++m_GraphGenerations[slot];
    }

    // Do the whole build on a worker thread -- DXC (no Vulkan) plus the Vulkan
    // shader/pipeline creation -- so neither the render thread nor the UI thread
    // stalls on the Metal pipeline compile. MeshRenderer::Render installs the result.
    m_Executor.Enqueue([this, slot, compileGraph, compileBlend, instance, notifyEditor, generation]()
    {
        {
            // The graphics backend already supports preparing a pipeline away from
            // the render thread, but its shared caches are not used concurrently.
            // Multiple restored slots therefore compile serially on workers while
            // UI and rendering continue independently.
            std::lock_guard<std::mutex> buildLock(m_GraphBuildMutex);
            if (const auto compiled = MaterialCompiler::CompileToSpirv(compileGraph))
                if (const auto shader = MaterialCompiler::LoadCompiled(m_ShaderLoader.get(), *compiled))
                {
                    bool current;
                    {
                        std::lock_guard<std::mutex> generationLock(m_GraphGenerationMutex);
                        current = m_GraphGenerations[slot] == generation;
                    }
                    if (current)
                        m_MeshRenderer->PrepareMaterialShader(
                            slot, shader, compileBlend, m_Model, instance);
                }
        }
        if (notifyEditor)
        {
            std::lock_guard<std::mutex> lock(m_CompileMutex);
            m_CompileReady = true;
        }
    });
}

std::filesystem::path Dissolve::MaterialAssetPath(const std::string& name)
{
    return std::filesystem::path("./Assets/Materials") / (name + ".material.json");
}

std::filesystem::path Dissolve::InstanceAssetPath(const std::string& name)
{
    return std::filesystem::path("./Assets/Materials") / (name + ".matinstance.json");
}

// The node editor authors the parent material and saves only that. Which slots use it,
// and with what overrides, is the instance's business (see SaveInstanceAsset).
void Dissolve::SaveMaterialAsset()
{
    const int target = m_GraphEditor.TargetMaterial();
    Ref<Material> base;
    {
        std::lock_guard<std::mutex> lock(m_Model->MaterialsMutex());
        const auto& materials = m_Model->GetMaterials();
        if (target >= 0 && target < (int)materials.size())
            base = materials[target]->GetParent();
    }

    // Built against the slot's current parent so the graph's Parameter nodes keep the
    // StandardPBR/glTF defaults they read from.
    const Ref<Material> material = m_GraphEditor.BuildMaterial(base);
    MaterialAsset::SaveMaterial(m_GraphEditor.MaterialPath(), *material);
}

// Reopens a saved material for editing. The graph travels as the material's document,
// so the editor is filled from the asset rather than parsing the file itself.
void Dissolve::OpenMaterialAsset()
{
    const Ref<Material> material = MaterialAsset::LoadMaterial(m_GraphEditor.MaterialPath());
    if (!material || !material->GetDocument())
        return;
    m_GraphEditor.SetDocument(*material->GetDocument());

    // Show the reopened graph on its target slot without waiting for an edit.
    if (m_Compiling) m_RecompileQueued = true;
    else StartGraphCompile();
}

// Saves the selected slot's instance: its overrides, its name, and a reference to the
// parent material asset. Several instances may share one parent, so the instance name
// is the user's to choose rather than the material's.
void Dissolve::SaveInstanceAsset(const uint32_t slot)
{
    Ref<MaterialInstance> snapshot;
    {
        std::lock_guard<std::mutex> lock(m_Model->MaterialsMutex());
        const auto& materials = m_Model->GetMaterials();
        if (slot >= materials.size())
            return;
        snapshot = CreateRef<MaterialInstance>(*materials[slot]);
    }

    const Ref<Material>& parent = snapshot->GetParent();
    if (!parent || !parent->GetDocument())
    {
        EE_CORE_WARN("Material instance: slot {} has no graph material to reference.", slot)
        return;
    }

    // An instance is only a parent reference plus overrides, so a parent that was never
    // saved would leave it dangling on the next load.
    const auto materialPath = MaterialAssetPath(parent->GetName());
    if (!std::filesystem::exists(materialPath))
    {
        EE_CORE_WARN("Material instance: save the '{}' material before its instances.",
            parent->GetName())
        return;
    }

    const std::string name = m_InstanceName[0] != '\0' ? m_InstanceName : parent->GetName();
    snapshot->SetName(name);
    const auto instancePath = InstanceAssetPath(name);
    if (!MaterialAsset::SaveInstance(instancePath, *snapshot, materialPath))
        return;

    // Keep the live instance in step with what was written, so the panel does not go on
    // showing the name the slot had before the save.
    {
        std::lock_guard<std::mutex> lock(m_Model->MaterialsMutex());
        const auto& materials = m_Model->GetMaterials();
        if (slot < materials.size())
            materials[slot]->SetName(name);
    }

    if (m_Model->SetMaterialAsset(slot, instancePath))
        m_Model->SaveAsset();
}

bool Dissolve::LoadMaterialAsset(const uint32_t slot, const std::filesystem::path& instancePath)
{
    if (slot >= m_Model->GetMaterials().size())
    {
        EE_CORE_WARN("Material slots: ignoring out-of-range slot {}.", slot)
        return false;
    }
    const Ref<MaterialInstance> instance = MaterialAsset::LoadInstance(instancePath);
    if (!instance)
        return false;
    if (instance->GetParent()->HasGraph())
        QueueGraphCompile(slot, instance, false);
    else
    {
        std::lock_guard<std::mutex> lock(m_Model->MaterialsMutex());
        const auto& current = m_Model->GetMaterials()[slot];
        current->SetParent(instance->GetParent());
        current->ApplyCompatibleOverridesFrom(*instance);
        current->SetName(instance->GetName());
    }
    return true;
}

void Dissolve::LoadMaterialAssets()
{
    for (uint32_t slot = 0; slot < m_Model->GetMaterials().size(); ++slot)
    {
        const auto& material = m_Model->GetMaterialAsset(slot);
        if (!material.empty())
            LoadMaterialAsset(slot, material);
    }
}

void Dissolve::OnGUI(const Timestep frameTime)
{
    EE_PROFILE_ZONE_SCOPED()
    Application::OnGUI(frameTime);

    // Build the ImGui UI on the main thread (GLFW input lives here); the draw is
    // submitted later from OnRender on the render thread.
    DrawMaterialEditor();

    // Visual node editor. On Apply (or live-preview debounce), kick off the graph's
    // DXC compilation on a worker thread so the UI doesn't stall.
    const int materialCount = (int)m_Model->GetMaterials().size();
    {
        std::lock_guard<std::mutex> lock(m_Model->MaterialsMutex());
        const int target = m_GraphEditor.TargetMaterial();
        if (target >= 0 && target < materialCount)
            m_GraphEditor.SyncParametersFrom(*m_Model->GetMaterials()[target]);
    }
    const bool compileRequested = m_GraphEditor.Draw(materialCount);
    {
        std::lock_guard<std::mutex> lock(m_Model->MaterialsMutex());
        const int target = m_GraphEditor.TargetMaterial();
        if (target >= 0 && target < materialCount)
        {
            const auto& instance = m_Model->GetMaterials()[target];
            if (instance->GetParent() && instance->GetParent()->HasGraph())
                m_GraphEditor.ApplyParametersTo(*instance);
        }
    }
    if (m_GraphEditor.SavedThisFrame())
        SaveMaterialAsset();
    if (m_GraphEditor.LoadedThisFrame())
        OpenMaterialAsset();
    if (compileRequested)
    {
        // Coalesce changes that arrive while a compile is already in flight.
        if (m_Compiling) m_RecompileQueued = true;
        else StartGraphCompile();
    }

    // Observe worker completion on the main thread. The prepared result, when valid,
    // is already waiting for the render thread to install at a frame boundary.
    bool ready = false;
    {
        std::lock_guard<std::mutex> lock(m_CompileMutex);
        if (m_CompileReady) { ready = true; m_CompileReady = false; }
    }
    if (ready)
    {
        // A successful compile is already queued for atomic material + shader
        // installation by Render; this only advances the compile state machine.
        m_Compiling = false;
        if (m_RecompileQueued) { m_RecompileQueued = false; StartGraphCompile(); }
    }

    // Finalize and snapshot ImGui on this thread. The render thread later consumes
    // immutable draw data, so it never races the next ImGui::NewFrame().
    m_GraphicsContext->EndImGuiFrame();
}

void Dissolve::DrawMaterialEditor()
{
    m_GraphicsContext->BeginImGuiFrame();

    ImGui::Begin("Material Editor");
    const auto& materials = m_Model->GetMaterials();
    int saveInstanceSlot = -1;
    if (!materials.empty())
    {
        static int selected = 0;
        selected = std::clamp(selected, 0, (int)materials.size() - 1);
        ImGui::SliderInt("Material", &selected, 0, (int)materials.size() - 1);

        // Serialise with the render thread's material pack (see Model::MaterialsMutex).
        std::lock_guard<std::mutex> lock(m_Model->MaterialsMutex());

        const auto& mat = materials[selected];
        ImGui::TextWrapped("%s", mat->GetName().c_str());
        if (const Ref<Material>& parent = mat->GetParent())
            ImGui::TextDisabled("Parent: %s", parent->GetName().c_str());

        // Saving the instance is separate from saving the graph: one parent material can
        // back any number of named instances, each with its own overrides.
        if (selected != m_InstanceNameSlot)
        {
            m_InstanceNameSlot = selected;
            const std::string& name = mat->GetName();
            const size_t length = std::min(name.size(), sizeof(m_InstanceName) - 1);
            std::memcpy(m_InstanceName, name.data(), length);
            m_InstanceName[length] = '\0';
        }
        ImGui::SetNextItemWidth(160.0f);
        ImGui::InputText("##instance", m_InstanceName, sizeof(m_InstanceName));
        ImGui::SameLine();
        const bool savable = mat->GetParent() && mat->GetParent()->GetDocument();
        ImGui::BeginDisabled(!savable);
        // Deferred: SaveInstanceAsset takes the same mutex this block holds.
        if (ImGui::Button("Save Instance"))
            saveInstanceSlot = selected;
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled(savable ? ".matinstance.json" : "(no graph material)");
        ImGui::Separator();
        bool changed = false;

        glm::vec4 base = mat->GetVector("BaseColorFactor");
        if (ImGui::ColorEdit4("Base Color", &base.x)) { mat->SetVector("BaseColorFactor", base); changed = true; }

        float metallic = mat->GetScalar("Metallic");
        if (ImGui::SliderFloat("Metallic", &metallic, 0.0f, 1.0f)) { mat->SetScalar("Metallic", metallic); changed = true; }

        float roughness = mat->GetScalar("Roughness");
        if (ImGui::SliderFloat("Roughness", &roughness, 0.0f, 1.0f)) { mat->SetScalar("Roughness", roughness); changed = true; }

        float clearcoat = mat->GetScalar("ClearcoatFactor");
        if (ImGui::SliderFloat("Clearcoat", &clearcoat, 0.0f, 1.0f)) { mat->SetScalar("ClearcoatFactor", clearcoat); changed = true; }

        glm::vec3 emissive = glm::vec3(mat->GetVector("EmissiveFactor"));
        if (ImGui::ColorEdit3("Emissive", &emissive.x)) { mat->SetVector("EmissiveFactor", glm::vec4(emissive, 0.0f)); changed = true; }

        if (changed)
            m_Model->MarkMaterialsDirty();
    }
    ImGui::End();

    if (saveInstanceSlot >= 0)
        SaveInstanceAsset((uint32_t)saveInstanceSlot);
    // The draw data is submitted from OnRender (render thread) via EndImGuiFrame.
}

void Dissolve::OnRender(const Timestep frameTime)
{
    EE_PROFILE_ZONE_SCOPED()
    Application::OnRender(frameTime);

    m_CameraController->Update(frameTime);
    m_FrameData.ViewProj = m_CameraController->GetCamera().GetViewProjectionMatrix();
    m_FrameConstantBuffer->UpdateData(&m_FrameData, sizeof(SFrameData));

    m_ParticlesRenderer->Update(frameTime);

    m_GraphicsContext->Clear();

    //DrawGeometry();

    // Node-graph material variants are prepared by the compile worker and installed
    // by MeshRenderer::Render, so nothing to apply here.

    // m_ParticlesRenderer->Render(m_GPUSystem, m_CameraController->GetCamera());
    m_MeshRenderer->Render(m_Model, m_CameraController->GetCamera());
    m_PostProcessor->Apply();

    // Submit the ImGui snapshot built in OnGUI on the render thread, where the
    // render target is in a renderable layout.
    m_GraphicsContext->RenderImGuiFrame();
}

void Dissolve::OnEvent(Event& event)
{
    Application::OnEvent(event);
    m_CameraController->ProcessEvent(event);
}

void Dissolve::DrawGeometry()
{
     const auto renderingInfo = SRenderingInfo
     {
         .ColorAttachment = m_GraphicsContext->GetRenderTarget(),
         .RenderArea = m_DrawExtent
     };

     Viewport viewport = {};
     viewport.X = 0;
     viewport.Y = 0;
     viewport.Width = m_DrawExtent.Width;
     viewport.Height = m_DrawExtent.Height;
     viewport.MinDepth = 0.0f;
     viewport.MaxDepth = 1.0f;

     Rect2D scissor = {};
     scissor.Offset = { 0, 0 };
     scissor.Extent = m_DrawExtent;

     m_Executor.Enqueue([this, renderingInfo, viewport, scissor]()
     {
         const auto cmd = this->m_GraphicsContext->GetSecondaryCommandBuffer();
         cmd->BeginRendering(renderingInfo);
         cmd->SetViewports({ viewport });
         cmd->SetScissors({ scissor });
         pipeline->Bind(cmd);
         cmd->Draw(3);
         cmd->EndRendering();
         this->m_GraphicsContext->EnqueueSecondaryCommandBuffer(cmd);
     }, &m_WaitGroup);

    m_WaitGroup.Wait();
}

Application* Elixir::CreateApplication()
{
    EE_PROFILE_ZONE_SCOPED()
    return new Dissolve();
}
