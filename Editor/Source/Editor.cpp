#include "Editor.h"

#include "UI/Panels/ContentBrowserPanel.h"

#include "UI/Panels/ViewportPanel.h"
#include "UI/Panels/VFXPanel.h"
#include <iostream>

Editor::Editor(std::filesystem::path asset)
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

    m_EditorUI->AddPanel(CreateScope<ViewportPanel>(), 0);
    m_EditorUI->AddPanel(CreateScope<ContentBrowserPanel>(std::filesystem::path(ELIXIR_EDITOR_PROJECT_ROOT),
        [this](const std::filesystem::path& path) { m_OpenRequests.push_back(path); }), 0);

    m_GraphicsContext->SetClearColor({ 0.015f, 0.025f, 0.06f, 1.0f });
    if (!asset.empty()) m_OpenRequests.push_back(std::move(asset));
}

Editor::~Editor()
{
    m_EditorUI.reset();
    m_Documents.clear();
}

void Editor::OpenVFXAsset(const std::filesystem::path& asset)
{
    for (auto& document : m_Documents)
        if (document.Path == asset) { m_EditorUI->ActivateTab(document.Tab); return; }
    SDocument document{}; document.Path = asset;
    if (!ResizePreview(document, {568, 320, 1})) return;
    document.Manager = CreateScope<Aether::Manager>(m_GraphicsContext.get(), m_ShaderLoader.get(),
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

bool Editor::ResizePreview(SDocument& document, const Extent3D& extent)
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
        EE_CORE_ERROR("Could not create VFX preview targets.");
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
    if (document.Panel) document.Panel->SetPreviewImage(document.DisplayTarget);
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
    }
    m_CloseRequests.clear();
    m_EditorUI->Update(frameTime);
    for (const auto& path : m_OpenRequests) OpenVFXAsset(path);
    m_OpenRequests.clear();
    for (auto& document : m_Documents)
    {
        m_EditorUI->SetTabTitle(document.Tab, document.Panel->GetTitle());
        if (document.Panel->IsActive()) ResizePreview(document, document.Panel->GetPreviewExtent());
    }

}

void Editor::Render(const Timestep frameTime)
{
    EE_PROFILE_ZONE_SCOPED()
    Application::Render(frameTime);
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
}

void Editor::OnEvent(Event& event)
{
    // Input callbacks can mutate text and widget trees before OnGUI runs.
    if (!m_GraphicsContext->RunRenderTaskAndWait([] {})) return;
    Application::OnEvent(event);
    for (const auto& document : m_Documents)
        document.Panel->ProcessEvent(event, m_GUIManager->GetPopupCount() > 0);
}

Application* Elixir::CreateApplication()
{
    EE_PROFILE_ZONE_SCOPED()
    return new Editor();
}

namespace
{
    // Runs the main editor and optionally opens an asset document tab.
    int RunEditor(const std::filesystem::path& binary, const std::filesystem::path& asset)
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
        Editor editor(asset); editor.Run(); return 0;
    }
}

#ifdef _WIN32
int wmain(int argc, wchar_t* argv[])
{
    if (argc == 1) return RunEditor(argv[0], {});
    if (argc == 3 && std::wstring_view(argv[1]) == L"--vfx") return RunEditor(argv[0], argv[2]);
    return 1;
}
#else
int main(int argc, char* argv[])
{
    if (argc == 1) return RunEditor(argv[0], {});
    if (argc == 3 && std::string_view(argv[1]) == "--vfx") return RunEditor(argv[0], argv[2]);
    return 1;
}
#endif
