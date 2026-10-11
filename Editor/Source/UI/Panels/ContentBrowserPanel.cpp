#include "ContentBrowserPanel.h"
#include "../EditorStyles.h"
#include <Engine/Core/Platform.h>
#include <Engine/GUI/Button.h>
#include <Engine/GUI/Icon.h>
#include <Engine/Icon/IconManager.h>
#include <Engine/GUI/ScrollBox.h>
#include <Engine/GUI/TextField.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <utility>

namespace
{
    Ref<GUI::TextBlock> Label(const std::string& value, float size = 11)
    {
        const auto text = CreateRef<GUI::TextBlock>(value);
        text->SetFontSize(size); text->SetColor(EditorStyle::Get().TextPrimary);
        return text;
    }

    Ref<GUI::Button> Action(const std::string& title, std::function<void()> click)
    {
        const auto button = CreateRef<GUI::Button>();
        button->SetStyle(EditorStyle::Get().PanelHeaderButton); button->SetPadding({8, 3});
        button->SetContent(Label(title)); button->OnClick(std::move(click)); return button;
    }

    Ref<GUI::Icon> CreateIcon(const std::string& name, float size)
    {
        const auto icon = CreateRef<GUI::Icon>();
        icon->SetIcon(IconManager::Load((std::filesystem::path(ELIXIR_EDITOR_PROJECT_ROOT) /
            "Assets" / "Icons" / (name + ".svg")).string()));
        icon->SetSize({size, size}); icon->SetStyle(EditorStyle::Get().SecondaryIcon); return icon;
    }

    std::string Fold(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
        return value;
    }

    std::vector<std::filesystem::directory_entry> Entries(const std::filesystem::path& directory, std::error_code& error)
    {
        std::vector<std::filesystem::directory_entry> entries;
        std::filesystem::directory_iterator iterator(directory, error), end;
        while (!error && iterator != end)
        {
            const auto name = iterator->path().filename().string();
            if (!name.empty() && name.front() != '.' && !iterator->is_symlink(error))
                entries.push_back(*iterator);
            if (!error) iterator.increment(error);
        }
        std::sort(entries.begin(), entries.end(), [](const auto& left, const auto& right)
        {
            std::error_code leftError, rightError;
            const bool leftFolder = left.is_directory(leftError), rightFolder = right.is_directory(rightError);
            return leftFolder != rightFolder ? leftFolder : left.path().filename() < right.path().filename();
        });
        return entries;
    }
}

Ref<GUI::Widget> ContentBrowserPanel::Build()
{
    m_Directory = m_ProjectRoot / "Assets" / "VFX";
    m_Root = CreateRef<GUI::Canvas>(); m_Root->SetClipChildren(true);
    m_Root->OnMouseDown([this]
    {
        const auto [x, y] = InputManager::GetMousePosition();
        if (!Contains({x, y})) SetOpen(false);
    });
    m_Surface = CreateRef<GUI::Overlay>();
    auto style = EditorStyle::Get().Panel;
    m_Surface->SetStyle(style); m_Surface->OnClick([] {});
    m_SurfaceSlot = &m_Root->AddChild(m_Surface);
    m_SurfaceSlot->SetAnchors({0, 1, 1, 1}).SetAlignment({0, 1}).SetSize({0, 320});
    const auto column = CreateRef<GUI::VerticalBox>();
    const float borderWidth = style.Normal.Background.Outline.Thickness;
    m_Surface->AddChild(column).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill)
        .SetVerticalAlignment(GUI::EVerticalAlignment::Fill).SetMargin({borderWidth, borderWidth});

    const auto header = CreateRef<GUI::HorizontalBox>(); header->SetPadding({10, 2});
    auto headerStyle = EditorStyle::Get().PanelSection;
    const auto panelRadius = style.Normal.Background.CornerRadius;
    headerStyle.Normal.Background.CornerRadius = {
        std::max(0.0f, panelRadius.x - borderWidth),
        std::max(0.0f, panelRadius.y - borderWidth), 0, 0};
    header->SetStyle(headerStyle);
    column->AddChild(header).SetFixedSize(32).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
    header->AddChild(CreateIcon("hierarchy", 13)).SetFixedSize(16).SetVerticalAlignment(GUI::EVerticalAlignment::Center);
    header->AddChild(Label("Content Browser", 12)).SetFillSize().SetMargin({6, 0});
    header->AddChild(Action("Refresh", [this] { m_Refresh = true; })).SetFixedSize(72);
    const auto close = CreateRef<GUI::Button>();
    close->SetStyle(EditorStyle::Get().PanelHeaderButton);
    close->SetPadding({0, 0});
    close->SetContent(CreateIcon("close", 11));
    close->OnClick([this] { SetOpen(false); });
    header->AddChild(close).SetFixedSize(16).SetVerticalAlignment(GUI::EVerticalAlignment::Fill);

    const auto toolbar = CreateRef<GUI::HorizontalBox>(); toolbar->SetPadding({8, 4});
    column->AddChild(toolbar).SetFixedSize(36).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
    toolbar->AddChild(Action("Up", [this]
    { if (m_Directory != m_ProjectRoot / "Assets") Navigate(m_Directory.parent_path()); })).SetFixedSize(42);
    toolbar->AddChild(Action("Assets", [this] { Navigate(m_ProjectRoot / "Assets"); })).SetFixedSize(64);
    m_Path = Label("/ VFX"); m_Path->SetColor(EditorStyle::Get().TextSecondary);
    toolbar->AddChild(m_Path).SetFillSize().SetMargin({8, 0});
    toolbar->AddChild(Action("All", [this] { m_OnlyVFX = false; m_Refresh = true; })).SetFixedSize(40);
    toolbar->AddChild(Action("VFX", [this] { m_OnlyVFX = true; m_Refresh = true; })).SetFixedSize(44);
    const auto search = CreateRef<GUI::TextField>(); search->SetStyle(EditorStyle::Get().TextField);
    search->SetFontSize(11); search->SetPlaceholder("Search assets...");
    search->OnChange([this](const std::string& value) { m_Filter = value; m_Refresh = true; });
    toolbar->AddChild(search).SetFixedSize(230).SetMargin({8, 0, 0, 0});

    const auto body = CreateRef<GUI::HorizontalBox>();
    column->AddChild(body).SetFillSize().SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
    const auto folders = CreateRef<GUI::ScrollBox>(); folders->SetStyle(EditorStyle::Get().ScrollBar);
    body->AddChild(folders).SetFixedSize(210).SetVerticalAlignment(GUI::EVerticalAlignment::Fill);
    m_Folders = CreateRef<GUI::VerticalBox>(); m_Folders->SetPadding({6, 6}); folders->SetContent(m_Folders);
    const auto divider = CreateRef<GUI::Canvas>(); divider->SetStyle(EditorStyle::Get().PanelHeaderBorder);
    body->AddChild(divider).SetFixedSize(1).SetVerticalAlignment(GUI::EVerticalAlignment::Fill);
    m_AssetScroll = CreateRef<GUI::ScrollBox>(); m_AssetScroll->SetStyle(EditorStyle::Get().ScrollBar);
    body->AddChild(m_AssetScroll).SetFillSize().SetVerticalAlignment(GUI::EVerticalAlignment::Fill);
    m_Assets = CreateRef<GUI::VerticalBox>(); m_Assets->SetPadding({10, 8}); m_AssetScroll->SetContent(m_Assets);
    m_Status = Label("", 10); m_Status->SetColor(EditorStyle::Get().TextSecondary);
    column->AddChild(m_Status).SetFixedSize(24).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill).SetMargin({10, 0});
    m_Animation = CreateScope<GUI::WidgetAnimation>(m_Surface);
    m_Surface->SetVisibility(GUI::EVisibility::Collapsed);
    Refresh(); m_Root->SetVisibility(GUI::EVisibility::Collapsed); return m_Root;
}

void ContentBrowserPanel::SetOpen(const bool open)
{
    if (m_IsOpen == open) return;
    m_IsOpen = open;
    if (open)
    {
        m_Root->SetVisibility(GUI::EVisibility::Visible);
        m_Refresh = true;
    }
    AnimateOverlay(m_Surface, *m_Animation,
        open ? GUI::EVisibility::Visible : GUI::EVisibility::Collapsed, {0, 12});
    m_Animation->OnFinished([this, open]
    {
        m_Surface->SetVisibility(open ? GUI::EVisibility::Visible : GUI::EVisibility::Collapsed);
        if (!open) m_Root->SetVisibility(GUI::EVisibility::Collapsed);
    });
}

bool ContentBrowserPanel::Contains(const glm::vec2& point) const
{
    if (m_Surface->GetVisibility() == GUI::EVisibility::Collapsed) return false;
    auto rect = m_Surface->GetGeometry();
    rect.Position += m_Surface->GetRenderOffset();
    return rect.Contains(point);
}

void ContentBrowserPanel::OnUpdate(const Timestep frameTime)
{
    m_Animation->Update(frameTime);
    if (m_Root->GetVisibility() == GUI::EVisibility::Collapsed) return;
    const float height = std::min(320.0f, std::max(0.0f, m_Root->GetGeometry().Size.y));
    if (height != m_DrawerHeight)
    { m_DrawerHeight = height; m_SurfaceSlot->SetSize({0, height}); }
    const int columns = std::max(1, static_cast<int>((m_AssetScroll->GetGeometry().Size.x - 20) / 126));
    if (columns != m_Columns) { m_Columns = columns; m_Refresh = true; }
    if (m_IsOpen && std::exchange(m_Refresh, false)) Refresh();
}

void ContentBrowserPanel::Navigate(const std::filesystem::path& directory)
{
    m_Directory = directory; m_SelectedAsset.clear(); m_Refresh = true;
    m_AssetScroll->SetScrollOffset({0, 0});
}

void ContentBrowserPanel::AddFolders(const std::filesystem::path& directory, const int depth)
{
    const auto row = Action(directory == m_ProjectRoot / "Assets" ? "Assets" : directory.filename().string(),
        [this, directory] { Navigate(directory); });
    row->SetPadding({8.0f + depth * 12.0f, 2});
    const auto content = CreateRef<GUI::HorizontalBox>();
    content->AddChild(CreateIcon("hierarchy", 11)).SetFixedSize(13).SetVerticalAlignment(GUI::EVerticalAlignment::Center);
    content->AddChild(Label(directory == m_ProjectRoot / "Assets" ? "Assets" : directory.filename().string()))
        .SetFillSize().SetMargin({5, 0});
    row->SetContent(content);
    row->GetContentSlot()->SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
    if (directory == m_Directory)
    {
        auto selected = EditorStyle::Get().PanelHeaderButton;
        selected.Normal.Background = EditorStyle::Get().Selection.Normal.Background; row->SetStyle(selected);
    }
    m_Folders->AddChild(row).SetFixedSize(26).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
    const auto relative = m_Directory.lexically_relative(directory);
    const bool selectedBranch = !relative.empty() && *relative.begin() != "..";
    if (depth >= 4 || (depth > 0 && !selectedBranch)) return;
    std::error_code error;
    for (const auto& entry : Entries(directory, error))
        if (entry.is_directory(error)) AddFolders(entry.path(), depth + 1);
}

void ContentBrowserPanel::Refresh()
{
    m_Assets->ClearChildren(); m_Folders->ClearChildren();
    AddFolders(m_ProjectRoot / "Assets", 0);
    const auto relative = m_Directory.lexically_relative(m_ProjectRoot / "Assets").generic_string();
    m_Path->SetText(relative == "." ? "/" : "/ " + relative);
    std::error_code error;
    const auto entries = Entries(m_Directory, error);
    Ref<GUI::HorizontalBox> row;
    size_t count = 0;
    const auto filter = Fold(m_Filter);
    for (const auto& entry : entries)
    {
        std::error_code entryError;
        const bool folder = entry.is_directory(entryError);
        const auto path = entry.path();
        const auto name = path.filename().string();
        const auto extension = Fold(path.extension().string());
        const auto assetRelative = path.lexically_relative(m_ProjectRoot / "Assets" / "VFX");
        const bool effect = !folder && extension == ".json" && !assetRelative.empty() &&
            *assetRelative.begin() != "..";
        if ((!filter.empty() && Fold(name).find(filter) == std::string::npos) ||
            (m_OnlyVFX && !folder && !effect)) continue;
        if (count % m_Columns == 0)
        {
            row = CreateRef<GUI::HorizontalBox>();
            m_Assets->AddChild(row).SetFixedSize(144).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
        }
        const auto card = CreateRef<GUI::VerticalBox>(); card->SetPadding({8, 8});
        card->OnMouseEnter([] { Platform::Get().SetCursorShape(ECursorShape::Hand); });
        card->OnMouseLeave([] { Platform::Get().SetPreviousCursorShape(); });
        card->SetStyle(path == m_SelectedAsset ? EditorStyle::Get().Selection : EditorStyle::Get().InspectorField);
        row->AddChild(card).SetFixedSize(118).SetVerticalAlignment(GUI::EVerticalAlignment::Fill).SetMargin({0, 0, 8, 8});
        const auto thumbnail = CreateRef<GUI::Overlay>(); thumbnail->SetStyle(EditorStyle::Get().PanelSection);
        card->AddChild(thumbnail).SetFixedSize(74).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
        if (folder)
            thumbnail->AddChild(CreateIcon("hierarchy", 28)).SetHorizontalAlignment(GUI::EHorizontalAlignment::Center)
                .SetVerticalAlignment(GUI::EVerticalAlignment::Center);
        else
        {
            const auto kind = Label(effect ? "VFX" : extension.empty() ? "FILE" : extension.substr(1), 18);
            kind->SetColor(effect ? EditorStyle::Get().Accent : EditorStyle::Get().TextSecondary);
            thumbnail->AddChild(kind).SetHorizontalAlignment(GUI::EHorizontalAlignment::Center)
                .SetVerticalAlignment(GUI::EVerticalAlignment::Center);
        }
        const auto stripe = CreateRef<GUI::Canvas>(); auto stripeStyle = EditorStyle::Get().Transparent;
        stripeStyle.Normal.Background.Color = effect ? EditorStyle::Get().Accent : EditorStyle::Get().TextSecondary;
        stripe->SetStyle(stripeStyle); card->AddChild(stripe).SetFixedSize(2).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
        card->AddChild(Label(folder ? name : path.stem().string(), 11)).SetFixedSize(24)
            .SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
        const auto type = Label(folder ? "Folder" : effect ? "VFX System" : "Asset", 9);
        type->SetColor(EditorStyle::Get().TextSecondary); card->AddChild(type).SetFixedSize(14);
        card->OnClick([this, path, folder, effect]
        {
            if (folder) Navigate(path);
            else if (effect) { m_Open(path); SetOpen(false); }
            else { m_SelectedAsset = path; m_Refresh = true; }
        });
        ++count;
    }
    m_Status->SetText(error ? "Could not read assets: " + error.message() :
        std::to_string(count) + " items" + (m_SelectedAsset.empty() ? "" : "  |  " + m_SelectedAsset.filename().string()));
    if (!error && count == 0)
        m_Assets->AddChild(Label(m_Filter.empty() ? "This folder is empty." : "No assets match your search."))
            .SetFixedSize(30).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
}
