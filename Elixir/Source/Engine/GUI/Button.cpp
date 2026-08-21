#include "epch.h"
#include "Button.h"

#include <Engine/Core/Platform.h>
#include <Engine/Font/FontManager.h>
#include <Engine/GUI/Slot.h>

namespace Elixir::GUI
{
    Button::Button(const std::string& text)
      : m_Text(text)
    {
        m_Font = FontManager::GetDefaultFont();

        SStyleOverride normal;
        normal.BackgroundColor = SColor{ 0.3f, 0.3f, 0.8f, 1.0f };
        normal.ForegroundColor = SColor{ 1.0f, 0.0f, 0.0f, 1.0f };
        normal.CornerRadius = glm::vec4{ 0.0f, 0.0f, 0.0f, 0.0f };
        normal.BackgroundBorders = glm::vec4{ 30.0f, 30.0f, 30.0f, 30.0f };
        SetStyle(EStyleLayer::Normal, normal);

        SStyleOverride hovered;
        hovered.BackgroundColor = SColor{ 1.0f, 0.0f, 0.0f, 1.0f };
        SetStyle(EStyleLayer::Hovered, hovered);
    }

    void Button::SetText(const std::string& text)
    {
        if (m_Text == text) return;
        m_Text = text;
        MarkLayoutDirty();
        MarkRenderDirty(); // the drawn text changes even when geometry does not
    }

    void Button::SetFont(const Ref<Font>& font)
    {
        EE_CORE_ASSERT(font, "Button::SetFont called with a null font");
        if (!font || m_Font == font) return;

        m_Font = font;
        MarkLayoutDirty();
        MarkRenderDirty();
    }

    void Button::SetFontSize(const float size)
    {
        if (m_FontSize == size) return;
        m_FontSize = size;
        MarkLayoutDirty();
        MarkRenderDirty();
    }

    void Button::SetPadding(const SPadding& padding)
    {
        if (m_Padding == padding) return;
        m_Padding = padding;
        MarkLayoutDirty();

        // Padding only affects this button's OWN draw when it renders its own text label.
        // With content, the background is padding-independent and the child re-renders itself
        // when its geometry changes during layout.
        if (!HasContent())
            MarkRenderDirty(); // padding shifts the label position/clip in BuildDrawCommands
    }

    void Button::SetTextColor(const EStyleLayer layer, const SColor& color)
    {
        SetForegroundColor(layer, color);
    }

    glm::vec2 Button::ComputeDesiredSize(const glm::vec2& availableSize)
    {
        const glm::vec2 innerAvailable = availableSize - glm::vec2(
            m_Padding.GetTotalHorizontal(),
            m_Padding.GetTotalVertical()
        );

        glm::vec2 contentSize{ 0.0f, 0.0f };

        if (HasContent())
            contentSize = m_ContentSlot->GetWidget()->Measure(innerAvailable);
        else if (!m_Text.empty())
            contentSize = MeasureTextSize(m_Text);

        const glm::vec2 desiredSize = contentSize + glm::vec2(
            m_Padding.GetTotalHorizontal(),
            m_Padding.GetTotalVertical()
        );

        return glm::max(desiredSize, m_MinDesiredSize);
    }

    void Button::LayoutChildren(const SRect& allocatedSpace)
    {
        if (HasContent())
        {
            const SRect innerSpace = ApplyPadding(allocatedSpace, m_Padding);
            const glm::vec2 childSize = m_ContentSlot->GetWidget()->Measure(innerSpace.Size);

            const SRect childRect  = AlignChild(
                childSize,
                innerSpace,
                m_ContentSlot->GetHorizontalAlignment(),
                m_ContentSlot->GetVerticalAlignment(),
                m_ContentSlot->GetMargin()
            );

            m_ContentSlot->GetWidget()->ArrangeChildren(childRect);
        }
    }

    void Button::BuildDrawCommands(RenderBatch& batch, int zOrder)
    {
        const SResolvedStyle style = GetResolvedStyle();

        // Background
        if (style.BackgroundTexture)
        {
            batch.AddTexture(
                style.BackgroundTexture,
                m_Geometry,
                style.BackgroundBorders,
                style.BackgroundColor,
                zOrder
            );
        }
        else
        {
            batch.AddRect(
                m_Geometry,
                style.BackgroundColor,
                style.CornerRadius,
                style.InsetShadow,
                style.DropShadow,
                style.Outline,
                zOrder
            );
        }

        if (!HasContent() && !m_Text.empty())
        {
            const float availableWidth = m_Geometry.Size.x - m_Padding.GetTotalHorizontal();

            const auto displayText = ProcessText(m_Text, availableWidth);
            const auto textSize = MeasureTextSize(displayText);
            const auto textPos = CalculateTextPosition(textSize);

            batch.AddText(
                displayText,
                { textPos, textSize },
                m_Font,
                m_FontSize,
                style.ForegroundColor,
                zOrder + 1,
                m_Geometry
            );
        }
    }

    std::string Button::ProcessText(const std::string& text, const float availableWidth)
    {
        auto size = FontManager::MeasureText(text, m_Font, m_FontSize);
        if (size.x <= availableWidth)
            return text;

        std::string ellipsis = "...";
        const float ellipsisWidth = FontManager::MeasureText(ellipsis, m_Font, m_FontSize).x;

        if (ellipsisWidth >= availableWidth)
            return ellipsis;

        std::string truncated = text;
        while (!truncated.empty())
        {
            UTF8::UTF8RemoveLastChar(truncated);
            size = FontManager::MeasureText(truncated, m_Font, m_FontSize);
            if (size.x + ellipsisWidth <= availableWidth)
                return truncated + ellipsis;
        }

        return ellipsis;
    }

    glm::vec2 Button::MeasureTextSize(const std::string& text)
    {
        return FontManager::MeasureText(text, m_Font, m_FontSize);
    }

    glm::vec2 Button::CalculateTextPosition(const glm::vec2 textSize)
    {
        const glm::vec2 contentPos = m_Geometry.Position + glm::vec2(
            m_Padding.Left,
            m_Padding.Top
        );
        const glm::vec2 contentSize = m_Geometry.Size - glm::vec2(
            m_Padding.GetTotalHorizontal(),
            m_Padding.GetTotalVertical()
        );

        return contentPos + (contentSize - textSize) * 0.5f;
    }

    void Button::HandleMouseEnter()
    {
        Widget::HandleMouseEnter();
        Platform::Get().SetCursorShape(ECursorShape::Hand);
    }

    void Button::HandleMouseLeave()
    {
        Widget::HandleMouseLeave();
        Platform::Get().SetPreviousCursorShape();
    }

    SInputReply Button::HandleMouseDown(const MouseButtonPressedEvent& event)
    {
        if (!IsEnabled()) return SInputReply::Unhandled();

        // Button is unconditionally interactive - it must win the mouse-down bubble even when
        // it has no OnClick/OnMouseDown/OnMouseUp callback registered (e.g. a subclass that
        // overrides HandleClick() directly instead), and even when its own content (e.g. a
        // TextBlock label) sits deeper in the hit path. Duplicates the "handled" branch of
        // Widget::HandleMouseDown instead of delegating to it, because that branch is now
        // gated on m_On*Callback being set - a gate Button must not depend on.
        m_Pressed = true;
        MarkRenderDirty();
        if (m_OnMouseDownCallback) m_OnMouseDownCallback();
        return SInputReply::HandledAndCaptured();
    }
}