#include "ContentBrowserPanel.h"
#include "../EditorStyles.h"
#include "../../Assets/VFXAssets.h"
#include <Engine/GUI/ScrollBox.h>
#include <Engine/GUI/TextField.h>
#include <algorithm>
#include <cctype>

Ref<GUI::Widget> ContentBrowserPanel::Build()
{
    const auto root = CreateRef<GUI::Canvas>();
    const auto surface = CreateRef<GUI::Overlay>(); surface->SetStyle(EditorStyle::Get().Panel);
    surface->SetPadding({12, 8});
    root->AddChild(surface).SetAnchors({0, 1, 1, 1}).SetOffsets(10, 0, -10, 0)
        .SetPosition({0, -190}).SetSize({0, 180});
    const auto column = CreateRef<GUI::VerticalBox>();
    surface->AddChild(column).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill)
        .SetVerticalAlignment(GUI::EVerticalAlignment::Fill);
    const auto header = CreateRef<GUI::HorizontalBox>();
    column->AddChild(header).SetFixedSize(30).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
    const auto title = CreateRef<GUI::TextBlock>("CONTENT BROWSER  /  Assets / VFX"); title->SetFontSize(12);
    header->AddChild(title).SetFillSize();
    const auto search = CreateRef<GUI::TextField>(); search->SetStyle(EditorStyle::Get().TextField);
    search->SetFontSize(11); search->SetPlaceholder("Search effects...");
    search->OnChange([this](const std::string& value) { m_Filter = value; m_Refresh = true; });
    header->AddChild(search).SetFixedSize(220);
    const auto refresh = CreateRef<GUI::TextBlock>("Refresh"); refresh->SetFontSize(11);
    refresh->OnClick([this] { m_Refresh = true; }); header->AddChild(refresh).SetFixedSize(70);
    const auto scroll = CreateRef<GUI::ScrollBox>(); scroll->SetStyle(EditorStyle::Get().ScrollBar);
    scroll->SetScrollAxis(GUI::EScrollAxis::Horizontal);
    column->AddChild(scroll).SetFillSize().SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
    m_Assets = CreateRef<GUI::HorizontalBox>(); scroll->SetContent(m_Assets);
    m_Status = CreateRef<GUI::TextBlock>("Click an effect to open a VFX tab"); m_Status->SetFontSize(10);
    column->AddChild(m_Status).SetFixedSize(20);
    Refresh(); return root;
}

void ContentBrowserPanel::OnUpdate(Timestep) { if (std::exchange(m_Refresh, false)) Refresh(); }

void ContentBrowserPanel::Refresh()
{
    m_Assets->ClearChildren();
    const auto assets = EditorAssets::FindVFXAssets(m_ProjectRoot);
    for (const auto& path : assets.Files)
    {
        auto name = path.lexically_relative(m_ProjectRoot / "Assets" / "VFX").generic_string();
        auto folded = name, filter = m_Filter;
        const auto lower = [](unsigned char character) { return static_cast<char>(std::tolower(character)); };
        std::transform(folded.begin(), folded.end(), folded.begin(), lower);
        std::transform(filter.begin(), filter.end(), filter.begin(), lower);
        if (!filter.empty() && folded.find(filter) == std::string::npos) continue;
        const auto card = CreateRef<GUI::VerticalBox>(); card->SetStyle(EditorStyle::Get().InspectorField);
        card->SetPadding({12, 10});
        m_Assets->AddChild(card).SetFixedSize(180).SetMargin({4, 8})
            .SetVerticalAlignment(GUI::EVerticalAlignment::Fill);
        const auto kind = CreateRef<GUI::TextBlock>("VFX SYSTEM"); kind->SetFontSize(10);
        kind->SetColor(EditorStyle::Get().Accent); card->AddChild(kind).SetFixedSize(24);
        const auto label = CreateRef<GUI::TextBlock>(name); label->SetFontSize(12);
        card->AddChild(label).SetFillSize(); card->OnClick([this, path] { m_Open(path); });
    }
    m_Status->SetText(assets.Error.empty() ? "Click an effect to open a VFX tab" : assets.Error);
}
