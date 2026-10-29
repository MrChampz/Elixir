#include "Editor.h"

#include "UI/Panels/ContentBrowserPanel.h"

#include "UI/Panels/ViewportPanel.h"
#include "UI/Panels/VFXPanel.h"
#include "UI/Panels/MeshPanel.h"
#include "UI/Panels/StrandsPanel.h"
#include "Assets/MeshAssets.h"
#include <Engine/Mesh/MeshLoaderRegistry.h>
#include "Environment.h"
#include <iostream>

Editor::Editor(std::filesystem::path asset, bool openStrands)
{
    EE_PROFILE_ZONE_SCOPED()

    m_Window->SetTitle("Editor");

    m_EditorUI = CreateScope<EditorUI>(m_GUIManager.get());
    m_EditorUI->AddMenuItem("File");
    m_EditorUI->AddMenuItem("Edit");
    m_EditorUI->AddMenuItem("Assets");
    m_EditorUI->AddMenuItem("GameObject");
    m_EditorUI->AddMenuItem("Component");
    m_EditorUI->AddMenuItem("Window");
    m_EditorUI->AddMenuItem("Help");

    m_EditorUI->AddTab("Scene", true);

    // Worked example of GUI::Manager's popup layer stack + GUI::ScrollBox: more items than
    // fit the dropdown's fixed height, so opening it actually exercises scrolling.
    m_EditorUI->AddDropdownMenu("Examples", {
        "Scene", "Prefab", "Material", "Texture 2D", "Cubemap",
        "Render Texture", "Animation", "Animator Controller",
        "Audio Mixer", "Physics Material", "Shader Graph",
    });

    auto scenePanel = CreateScope<ViewportPanel>();
    m_ScenePanel = scenePanel.get();
    m_EditorUI->AddPanel(std::move(scenePanel), 0);
    m_EditorUI->SetContentBrowser(CreateScope<ContentBrowserPanel>(std::filesystem::path(ELIXIR_EDITOR_PROJECT_ROOT),
        [this](const std::filesystem::path& path) { m_OpenRequests.push_back(path); }));

    m_GraphicsContext->SetClearColor({ 0.015f, 0.025f, 0.06f, 1.0f });
    LoadDefaultScene();
    LoadStrandScene();
    if (openStrands && m_StrandScene.Panel) m_EditorUI->ActivateTab(1);
    if (!asset.empty()) m_OpenRequests.push_back(std::move(asset));
}

void Editor::LoadStrandScene()
{
    if (!ResizePreview(m_StrandScene, {960, 540, 1})) return;
    m_StrandScene.Manager = CreateScope<Aether::Manager>(m_GraphicsContext.get(), m_ShaderLoader.get(),
        GetMaterialRegistry(), *m_StrandScene.Materials);
    const auto root = std::filesystem::path(ELIXIR_EDITOR_PROJECT_ROOT);
    if (auto mesh = MeshLoaderRegistry::LoadStaticMesh(root / "Assets/Meshes/StrandScene/StrandScene.gltf"))
    {
        m_StrandScene.Mesh = *mesh;
        m_StrandScene.Renderer = CreateScope<StaticMeshRenderer>(*m_GraphicsContext,
            *m_StrandScene.Materials, *MeshLoaderRegistry::GetGeometryPool());
    }
    else EE_CORE_ERROR("Could not load the strand scene reference objects.");
    auto panel = CreateScope<StrandsPanel>(*m_StrandScene.Manager, *m_GraphicsContext,
        root / "Assets/VFX/StrandPlayground.json");
    m_StrandScene.Panel = panel.get();
    m_EditorUI->AddTab("Strands", false);
    m_EditorUI->AddPanel(std::move(panel), 1);
    m_StrandScene.Panel->SetPreviewImage(m_StrandScene.DisplayTarget);
}

void Editor::LoadDefaultScene()
{
    const auto environment = Environment::Load(
        *m_GraphicsContext,
        std::filesystem::path(ELIXIR_EDITOR_PROJECT_ROOT) /
            "Assets" / "Textures" / "white_home_studio_2k.hdr"
    );
    if (!environment || !environment->GetEnvironment() || !environment->GetIrradiance() ||
        !environment->GetPrefiltered() || !environment->GetSampler() ||
        !environment->GetSampler()->IsValid())
    {
        EE_CORE_ERROR("Could not create lighting resources for the editor scenes.");
        return;
    }
    m_SceneLighting = {
        .Environment = {
            .Environment = environment->GetEnvironment(),
            .Irradiance = environment->GetIrradiance(),
            .Prefiltered = environment->GetPrefiltered(),
            .Sampler = environment->GetSampler(),
            .Intensity = 1.5f,
            .MaxLod = Environment::GetMaxLod(),
        },
        .DirectionalLightDirection = glm::normalize(glm::vec3{-0.5f, 0.65f, -0.55f}),
        .DirectionalLightColor = {1.0f, 0.96f, 0.9f},
        .DirectionalLightIntensity = 2.2f,
        .DebugView = ESurfaceDebugView::Composite,
    };
    m_SceneLightingReady = true;

    const auto scenePath = std::filesystem::path(ELIXIR_EDITOR_PROJECT_ROOT) /
        "Assets" / "Meshes" / "Sponza" / "glTF" / "Sponza.gltf";
    if (auto loaded = MeshLoaderRegistry::LoadStaticMesh(scenePath))
    {
        m_SceneMesh = std::move(*loaded);
        m_SceneRenderer = CreateScope<StaticMeshRenderer>(*m_GraphicsContext,
            GetMaterialSystem(), *MeshLoaderRegistry::GetGeometryPool());
        const auto extent = m_Window->GetFramebufferExtent();
        const float aspect = extent.Height ? static_cast<float>(extent.Width) / extent.Height : 1.0f;
        const auto& bounds = m_SceneMesh->GetLocalBounds();
        const auto size = bounds.Max - bounds.Min;
        const auto center = (bounds.Min + bounds.Max) * 0.5f;
        m_SceneCamera = CreateScope<ArcBallCameraController>(65.0f, aspect);
        m_SceneCamera->GetPerspectiveCamera().SetProjection(65.0f, aspect, 0.05f,
            glm::max(glm::length(size) * 4.0f, 100.0f));
        m_SceneCamera->SetTarget({center.x, bounds.Min.y + size.y * 0.12f, center.z});
        m_SceneCamera->SetDistance(size.x * 0.35f);
        m_SceneCamera->SetOrbitYaw(0.0f);
        m_SceneCamera->SetOrbitPitch(0.0f);
    }
    else EE_CORE_ERROR("Could not load the default Sponza scene: {}", scenePath.string());
}

Editor::~Editor()
{
    m_EditorUI.reset();
    m_Documents.clear();
    m_MeshDocuments.clear();
    m_StrandScene.Renderer.reset();
    m_StrandScene.Mesh.reset();
    m_StrandScene.Manager.reset();
    m_SceneRenderer.reset();
    m_SceneMesh.reset();
}

void Editor::OpenAsset(const std::filesystem::path& asset)
{
    std::error_code error;
    const auto canonical = std::filesystem::weakly_canonical(asset, error);
    const auto path = error ? asset.lexically_normal() : canonical;
    if (EditorAssets::IsMeshAsset(path)) OpenMeshAsset(path);
    else OpenVFXAsset(path);
}

void Editor::OpenMeshAsset(const std::filesystem::path& asset)
{
    for (auto& document : m_MeshDocuments)
        if (document.Path == asset) { m_EditorUI->ActivateTab(document.Tab); return; }
    const auto loaded = MeshLoaderRegistry::LoadStaticMesh(asset);
    if (!loaded || !*loaded)
    {
        m_EditorUI->SetAssetBrowserStatus("Could not load mesh: " + asset.filename().string());
        return;
    }
    SMeshDocument document{};
    document.Path = asset;
    document.Mesh = *loaded;
    if (!ResizePreview(document, {512, 512, 1})) return;
    document.Renderer = CreateScope<StaticMeshRenderer>(*m_GraphicsContext,
        *document.Materials, *MeshLoaderRegistry::GetGeometryPool());
    auto panel = CreateScope<MeshPanel>(*m_GraphicsContext, document.Mesh, asset);
    document.Panel = panel.get();
    document.Tab = m_EditorUI->AddDocument(std::move(panel), asset.stem().string(),
        [this](int index) { m_CloseRequests.push_back(index); });
    document.Panel->SetPreviewImage(document.DisplayTarget);
    m_MeshDocuments.push_back(std::move(document));
    m_EditorUI->SetAssetBrowserStatus(asset.filename().string());
}

bool Editor::IsSceneActive() const
{
    return (!m_StrandScene.Panel || !m_StrandScene.Panel->IsActive()) &&
        std::none_of(m_Documents.begin(), m_Documents.end(),
        [](const auto& document) { return document.Panel->IsActive(); }) &&
        std::none_of(m_MeshDocuments.begin(), m_MeshDocuments.end(),
        [](const auto& document) { return document.Panel->IsActive(); });
}

void Editor::OpenVFXAsset(const std::filesystem::path& asset)
{
    for (auto& document : m_Documents)
        if (document.Path == asset) { m_EditorUI->ActivateTab(document.Tab); return; }
    SDocument document{}; document.Path = asset;
    if (!ResizePreview(document, {568, 320, 1})) return;
    document.Manager = CreateScope<EditorVFXManager>(m_GraphicsContext.get(), m_ShaderLoader.get(),
        GetMaterialRegistry(), *document.Materials);
    const auto extent = m_Window->GetFramebufferExtent();
    auto panel = CreateScope<VFXPanel>(*document.Manager, *m_GraphicsContext,
        std::filesystem::path(ELIXIR_EDITOR_PROJECT_ROOT),
        extent.Height ? static_cast<float>(extent.Width) / extent.Height : 1.0f,
        [this](const std::string& status) { m_EditorUI->SetAssetBrowserStatus(status); });
    auto* preview = panel.get();
    const int tab = m_EditorUI->AddDocument(std::move(panel), asset.stem().string(),
        [this](const int index) { m_CloseRequests.push_back(index); });
    preview->SetPreviewImage(document.DisplayTarget);
    preview->Open(asset);
    document.Panel = preview; document.Tab = tab;
    m_Documents.push_back(std::move(document));
}

bool Editor::ResizePreview(SPreview& document, const Extent3D& extent)
{
    if (!extent.Width || !extent.Height) return true;
    if (document.HDRTarget && document.HDRTarget->GetExtent().Width == extent.Width &&
        document.HDRTarget->GetExtent().Height == extent.Height) return true;
    auto createImage = [this, extent](const EImageFormat format, const EImageUsage usage, const EImageLayout layout)
    {
        return Image::Create(m_GraphicsContext.get(), {
            .Width = extent.Width, .Height = extent.Height, .Depth = 1, .Type = EImageType::_2D,
            .Format = format, .Usage = usage, .InitialLayout = layout});
    };
    const auto colorUsage = EImageUsage::ColorAttachment | EImageUsage::Sampled |
        EImageUsage::TransferSrc | EImageUsage::TransferDst;
    auto hdr = createImage(EImageFormat::R16G16B16A16_SFLOAT, colorUsage, EImageLayout::General);
    auto display = createImage(EImageFormat::R8G8B8A8_SRGB, colorUsage, EImageLayout::General);
    auto depth = createImage(m_GraphicsContext->GetDepthStencilRenderTarget()->GetFormat(),
        EImageUsage::DepthStencilAttachment, EImageLayout::DepthStencilAttachment);
    if (!hdr || !display || !depth)
    {
        EE_CORE_ERROR("Could not create asset preview targets.");
        return false;
    }
    if (!document.Materials)
        document.Materials = CreateScope<Materials::MaterialSystem>(m_GraphicsContext.get(),
            Materials::SMaterialSystemConfig{}, m_ShaderLoader.get(), hdr);
    if (!document.PostProcess)
        document.PostProcess = CreateScope<PostProcessor>(m_GraphicsContext.get(), m_ShaderLoader.get(), extent);
    else
    {
        if (!m_GraphicsContext->RunRenderTaskAndWait([&document, extent] { document.PostProcess->Resize(extent); })) return false;
    }
    if (!document.Materials->SetRenderTargets(hdr, depth)) return false;
    document.HDRTarget = std::move(hdr); document.DisplayTarget = std::move(display); document.DepthTarget = std::move(depth);
    return true;
}

void Editor::OnGUI(const Timestep frameTime)
{
    EE_PROFILE_ZONE_SCOPED()
    // Finish the previous widget traversal before updating retained UI state.
    if (!m_GraphicsContext->RunRenderTaskAndWait([] {})) return;
    Application::OnGUI(frameTime);

    // Complete GPU uses before destroying a document's material and simulation resources.
    if (!m_CloseRequests.empty() && !m_GraphicsContext->RunRenderTaskAndWait(
        [this] { m_GraphicsContext->WaitForSubmittedFrames(); })) return;
    for (const int tab : m_CloseRequests)
    {
        m_EditorUI->CloseDocument(tab);
        std::erase_if(m_Documents, [tab](const auto& document) { return document.Tab == tab; });
        std::erase_if(m_MeshDocuments, [tab](const auto& document) { return document.Tab == tab; });
    }
    m_CloseRequests.clear();
    if (m_StrandScene.Panel)
        m_StrandScene.Panel->SetInputBlocked(m_GUIManager->GetPopupCount() > 0 ||
            m_EditorUI->IsContentBrowserHovered());
    m_EditorUI->Update(frameTime);
    for (const auto& path : m_OpenRequests) OpenAsset(path);
    m_OpenRequests.clear();
    for (auto& document : m_Documents)
    {
        m_EditorUI->SetTabTitle(document.Tab, document.Panel->GetTitle());
        if (document.Panel->IsActive())
        {
            const auto previous = document.DisplayTarget;
            if (ResizePreview(document, document.Panel->GetPreviewExtent()) && previous != document.DisplayTarget)
                document.Panel->SetPreviewImage(document.DisplayTarget);
        }
    }

    if (m_StrandScene.Panel && m_StrandScene.Panel->IsActive())
    {
        const auto previous = m_StrandScene.DisplayTarget;
        if (ResizePreview(m_StrandScene, m_StrandScene.Panel->GetPreviewExtent()) && previous != m_StrandScene.DisplayTarget)
            m_StrandScene.Panel->SetPreviewImage(m_StrandScene.DisplayTarget);
    }
    for (auto& document : m_MeshDocuments)
    {
        if (!document.Panel->IsActive()) continue;
        const auto previous = document.DisplayTarget;
        if (ResizePreview(document, document.Panel->GetPreviewExtent()) && previous != document.DisplayTarget)
            document.Panel->SetPreviewImage(document.DisplayTarget);
    }
}

void Editor::Render(const Timestep frameTime)
{
    EE_PROFILE_ZONE_SCOPED()
    Application::Render(frameTime);
    const bool sceneActive = IsSceneActive();
    if (sceneActive && m_SceneLightingReady && m_SceneRenderer && m_SceneCamera)
    {
        const auto extent = m_SceneTarget->GetExtent();
        if (extent.Height) m_SceneCamera->GetPerspectiveCamera().SetAspectRatio(static_cast<float>(extent.Width) / extent.Height);
        m_SceneRenderer->BeginFrame(m_SceneCamera->GetCamera(), m_SceneLighting);
        m_SceneRenderer->Render(m_SceneMesh);
        m_SceneRenderer->EndFrame();
    }
    if (m_StrandScene.Panel)
    {
        const bool active = m_StrandScene.Panel->IsActive();
        m_StrandScene.Materials->BeginFrame();
        m_StrandScene.Manager->BeginFrame(Timestep(active ? std::clamp(frameTime.GetSeconds(), 0.0f, 0.05f) : 0.0f));
        if (active)
        {
            m_GraphicsContext->Clear(m_StrandScene.HDRTarget);
            const auto& camera = m_StrandScene.Panel->GetCamera();
            if (m_SceneLightingReady && m_StrandScene.Renderer &&
                m_StrandScene.Panel->ShouldShowReferenceObjects())
            {
                m_StrandScene.Renderer->BeginFrame(camera, m_SceneLighting);
                m_StrandScene.Renderer->Render(m_StrandScene.Mesh);
                m_StrandScene.Renderer->EndFrame();
            }
            m_StrandScene.Manager->Render(camera, m_StrandScene.HDRTarget);
        }
        m_StrandScene.Materials->RenderFrame();
        if (active)
        {
            m_StrandScene.PostProcess->Apply(m_StrandScene.HDRTarget, m_StrandScene.DisplayTarget);
            const auto command = m_GraphicsContext->GetSecondaryCommandBuffer();
            command->Begin({.ColorAttachment = m_StrandScene.DisplayTarget, .RenderArea = m_StrandScene.DisplayTarget->GetExtent()});
            m_StrandScene.DisplayTarget->Barrier(command.get());
            command->End();
            m_GraphicsContext->EnqueueSecondaryCommandBuffer(command);
        }
    }
    for (const auto& document : m_Documents)
    {
        document.Materials->BeginFrame();
        if (document.Panel->IsActive()) m_GraphicsContext->Clear(document.HDRTarget);
        document.Panel->Render(frameTime, document.HDRTarget);
        document.Materials->RenderFrame();
        if (!document.Panel->IsActive()) continue;
        document.PostProcess->Apply(document.HDRTarget, document.DisplayTarget);
        const auto command = m_GraphicsContext->GetSecondaryCommandBuffer();
        command->Begin({.ColorAttachment = document.DisplayTarget, .RenderArea = document.DisplayTarget->GetExtent()});
        document.DisplayTarget->Barrier(command.get());
        command->End();
        m_GraphicsContext->EnqueueSecondaryCommandBuffer(command);
    }
    for (const auto& document : m_MeshDocuments)
    {
        if (!document.Panel->IsActive()) continue;
        document.Materials->BeginFrame();
        m_GraphicsContext->Clear(document.HDRTarget);
        if (m_SceneLightingReady)
        {
            document.Renderer->BeginFrame(document.Panel->GetCamera(), m_SceneLighting);
            document.Renderer->Render(document.Mesh);
            document.Renderer->EndFrame();
        }
        document.Materials->RenderFrame();
        document.PostProcess->Apply(document.HDRTarget, document.DisplayTarget);
        const auto command = m_GraphicsContext->GetSecondaryCommandBuffer();
        command->Begin({.ColorAttachment = document.DisplayTarget, .RenderArea = document.DisplayTarget->GetExtent()});
        document.DisplayTarget->Barrier(command.get());
        command->End();
        m_GraphicsContext->EnqueueSecondaryCommandBuffer(command);
    }
}

void Editor::OnEvent(Event& event)
{
    // Input callbacks can mutate text and widget trees before OnGUI runs.
    if (!m_GraphicsContext->RunRenderTaskAndWait([] {})) return;
    bool contentBrowserDismissed = false;
    if (event.GetEventType() == EventType::MouseButtonPressed &&
        static_cast<const MouseButtonPressedEvent&>(event).GetMouseButton() == EE_MOUSE_BUTTON_LEFT)
    {
        const auto [x, y] = InputManager::GetMousePosition();
        contentBrowserDismissed = m_EditorUI->DismissContentBrowserOutside({x, y});
        if (contentBrowserDismissed) event.Handled = true;
    }
    Application::OnEvent(event);
    if (m_StrandScene.Panel)
        m_StrandScene.Panel->ProcessEvent(event, contentBrowserDismissed || m_GUIManager->GetPopupCount() > 0 ||
            m_EditorUI->IsContentBrowserHovered());
    ProcessSceneCameraEvent(event, contentBrowserDismissed || m_GUIManager->GetPopupCount() > 0 ||
        m_EditorUI->IsContentBrowserHovered());
    for (const auto& document : m_Documents)
        document.Panel->ProcessEvent(event, contentBrowserDismissed || m_GUIManager->GetPopupCount() > 0 || m_EditorUI->IsContentBrowserHovered());
    for (const auto& document : m_MeshDocuments)
        document.Panel->ProcessEvent(event, contentBrowserDismissed || m_GUIManager->GetPopupCount() > 0 ||
            m_EditorUI->IsContentBrowserHovered());
}

void Editor::ProcessSceneCameraEvent(Event& event, const bool inputBlocked)
{
    if (!m_SceneCamera) return;
    if (event.GetEventType() == EventType::MouseButtonReleased)
    {
        m_SceneCamera->ProcessEvent(event);
        const auto button = static_cast<const MouseButtonReleasedEvent&>(event).GetMouseButton();
        if (button == EE_MOUSE_BUTTON_LEFT) m_SceneOrbitDragging = false;
        if (button == EE_MOUSE_BUTTON_MIDDLE) m_ScenePanDragging = false;
        return;
    }
    const bool sceneActive = IsSceneActive();
    if (!sceneActive || inputBlocked)
    {
        MouseButtonReleasedEvent orbit(EE_MOUSE_BUTTON_LEFT), pan(EE_MOUSE_BUTTON_MIDDLE);
        m_SceneCamera->ProcessEvent(orbit);
        m_SceneCamera->ProcessEvent(pan);
        m_SceneOrbitDragging = false; m_ScenePanDragging = false;
        return;
    }
    const auto [x, y] = InputManager::GetMousePosition();
    const bool dragging = m_SceneOrbitDragging || m_ScenePanDragging;
    if (!(dragging && event.GetEventType() == EventType::MouseMoved) &&
        (event.Handled || !m_ScenePanel || !m_ScenePanel->CanNavigateAt({x, y}))) return;
    if (event.GetEventType() == EventType::MouseButtonPressed)
    {
        const auto button = static_cast<const MouseButtonPressedEvent&>(event).GetMouseButton();
        if (button == EE_MOUSE_BUTTON_LEFT) m_SceneOrbitDragging = true;
        if (button == EE_MOUSE_BUTTON_MIDDLE) m_ScenePanDragging = true;
        if (!m_SceneOrbitDragging && !m_ScenePanDragging) return;
        m_SceneCamera->ProcessEvent(event);
        MouseMovedEvent origin(x, y);
        m_SceneCamera->ProcessEvent(origin);
        event.Handled = true;
    }
    else if ((dragging && event.GetEventType() == EventType::MouseMoved) ||
        event.GetEventType() == EventType::MouseScrolled)
    {
        m_SceneCamera->ProcessEvent(event);
        event.Handled = true;
    }
}

Application* Elixir::CreateApplication()
{
    EE_PROFILE_ZONE_SCOPED()
    return new Editor();
}

namespace
{
    // Runs the main editor and optionally opens an asset document tab.
    int RunEditor(const std::filesystem::path& binary, const std::filesystem::path& asset, bool openStrands = false)
    {
        std::error_code error;
        const auto executable = std::filesystem::canonical(binary, error);
        if (error) { std::cerr << "Could not resolve Editor executable: " << error.message(); return 1; }
        // Resolve runtime shaders and UI assets from the executable's output directory.
        auto runtime = executable.parent_path();
#ifdef __APPLE__
        if (runtime.filename() == "MacOS" && runtime.parent_path().filename() == "Contents")
            runtime = runtime.parent_path().parent_path().parent_path();
#endif
        std::filesystem::current_path(runtime, error);
        if (error) { std::cerr << "Could not open Editor runtime directory: " << error.message(); return 1; }
        Log::Init();
        Editor editor(asset, openStrands); editor.Run(); return 0;
    }
}

#ifdef _WIN32
int wmain(int argc, wchar_t* argv[])
{
    if (argc == 1) return RunEditor(argv[0], {});
    if (argc == 2 && std::wstring_view(argv[1]) == L"--strands") return RunEditor(argv[0], {}, true);
    if (argc == 3 && std::wstring_view(argv[1]) == L"--vfx") return RunEditor(argv[0], argv[2]);
    return 1;
}
#else
int main(int argc, char* argv[])
{
    if (argc == 1) return RunEditor(argv[0], {});
    if (argc == 2 && std::string_view(argv[1]) == "--strands") return RunEditor(argv[0], {}, true);
    if (argc == 3 && std::string_view(argv[1]) == "--vfx") return RunEditor(argv[0], argv[2]);
    return 1;
}
#endif
