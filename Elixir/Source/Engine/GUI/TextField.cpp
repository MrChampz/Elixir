#include "epch.h"
#include "TextField.h"

#include <Engine/Input/InputCodes.h>
#include <Engine/Font/FontManager.h>
#include <Engine/GUI/Slot.h>

namespace Elixir::GUI
{
    TextField::TextField(const std::string& text)
        : m_Text(text)
    {
        m_Font = FontManager::GetDefaultFont();
        m_DesiredSize = { 120.0f, 30.0f };
        m_CursorPosition = m_Text.size();
    }

    void TextField::Update(const Timestep frameTime)
    {
        Widget::Update(frameTime);
        UpdateCursorState(frameTime);
    }

    glm::vec2 TextField::ComputeDesiredSize()
    {
        return m_DesiredSize;
    }

    void TextField::GenerateDrawCommands(RenderBatch& batch, const int zOrder)
    {
        // Background
        if (m_Background)
        {
            batch.AddTexture(m_Background, m_Geometry, m_BackgroundColor, zOrder);
        }
        else
        {
            batch.AddRect(
                m_Geometry,
                m_BackgroundColor,
                m_CornerRadius,
                m_InsetShadow,
                m_DropShadow,
                m_Outline,
                zOrder
            );
        }

        const auto textSize = MeasureTextSize(m_Text);
        const auto textPos = CalculateTextPosition(textSize);

        // Selection
        if (m_SelectionStart != -1 && m_SelectionStart != m_SelectionEnd)
        {
            const int start = std::min(m_SelectionStart, m_SelectionEnd);
            const int end = std::max(m_SelectionStart, m_SelectionEnd);

            const float selx1 = textPos.x + GetTextWidth(m_Text, 0, start);
            const float selx2 = textPos.x + GetTextWidth(m_Text, 0, end);
            const float selectionHeight = std::min(textSize.y + 4.0f, m_Geometry.Size.y - m_Padding.GetTotalVertical());

            const auto selectionPos = glm::vec2(selx1, textPos.y - 2.0f);
            const auto selectionSize = glm::vec2(selx2 - selx1, selectionHeight);

            batch.AddRect(
                { selectionPos, selectionSize },
                m_SelectionColor,
                {}, {}, {}, {},
                zOrder + 1
            );
        }

        // Text or placeholder
        if (!m_Text.empty())
        {
            batch.AddText(
                m_Text,
                { textPos, textSize },
                m_Font,
                m_FontSize,
                m_TextColor,
                zOrder + 2,
                m_Geometry
            );
        }
        else
        {
            const auto placeholderSize = MeasureTextSize(m_Placeholder);
            const auto placeholderPos = CalculateTextPosition(placeholderSize);

            batch.AddText(
                m_Placeholder,
                { placeholderPos, placeholderSize },
                m_Font,
                m_FontSize,
                m_PlaceholderColor,
                zOrder + 2,
                m_Geometry
            );
        }

        // Cursor
        if (m_CursorVisible) {
            const float textWidth = GetTextWidth(m_Text, 0, m_CursorPosition);
            const float cursorX = textPos.x + textWidth + 0.5f;
            const float cursorHeight = textSize.y + 1.0f;
            const auto cursorPos = glm::vec2(cursorX, textPos.y - 0.5f);
            batch.AddRect(
                { cursorPos, { 1.5f, cursorHeight } },
                m_CursorColor,
                {}, {}, {}, {},
                zOrder + 3
            );
        }
    }

    void TextField::HandleMouseEnter()
    {
        Widget::HandleMouseEnter();
        // TODO: Show input cursor
    }

    void TextField::HandleMouseLeave()
    {
        Widget::HandleMouseLeave();
        // TODO: Show default cursor
    }

    void TextField::HandleMouseDown(const MouseButtonPressedEvent& event)
    {
        Widget::HandleMouseDown(event);

        float x = event.GetX() - m_Geometry.Position.x - m_Padding.Left + m_ScrollOffset;
        m_CursorPosition = GetCharIndexAtX(m_Text, x);
        m_SelectionStart = m_CursorPosition;
        m_SelectionEnd = m_CursorPosition;

        ResetCursorState();
        UpdateScrollOffset();
    }

    void TextField::HandleMouseMove(const MouseMovedEvent& event)
    {
        Widget::HandleMouseMove(event);

        // Only extend selection if mouse button is held (widget is pressed)
        if (!IsPressed()) return;

        float x = event.GetX() - m_Geometry.Position.x - m_Padding.Left + m_ScrollOffset;
        m_CursorPosition = GetCharIndexAtX(m_Text, x);
        m_SelectionEnd = m_CursorPosition;

        UpdateScrollOffset();
    }

    void TextField::HandleKeyPressed(const KeyPressedEvent& event)
    {
        Widget::HandleKeyPressed(event);

        if (!m_Focused) return;

        switch (event.GetKeyCode())
        {
            case EE_KEY_LEFT:
                if (event.IsShiftPressed())
                {
                    if (m_SelectionStart == -1) m_SelectionStart = m_CursorPosition;
                    MoveCursorLeft();
                    SelectText(m_SelectionStart, m_CursorPosition);
                }
                else
                {
                    ClearSelection();
                    MoveCursorLeft();
                }
                break;
            case EE_KEY_RIGHT:
                if (event.IsShiftPressed())
                {
                    if (m_SelectionStart == -1) m_SelectionStart = m_CursorPosition;
                    MoveCursorRight();
                    SelectText(m_SelectionStart, m_CursorPosition);
                }
                else
                {
                    ClearSelection();
                    MoveCursorRight();
                }
                break;
            case EE_KEY_HOME:
                MoveCursorToStart();
                break;
            case EE_KEY_END:
                MoveCursorToEnd();
                break;
            case EE_KEY_BACKSPACE:
                ClearPreviousCharacter();
                break;
            case EE_KEY_DELETE:
                ClearNextCharacter();
                break;
            case EE_KEY_A:
                if (event.IsCtrlPressed())
                    SelectWholeText();
                break;
            case EE_KEY_C:
                if (event.IsCtrlPressed())
                    CopyToClipboard(m_Text);
                break;
            case EE_KEY_V:
                if (event.IsCtrlPressed())
                    InsertText(GetFromClipboard());
                break;
            default:
                break;
        }
    }

    void TextField::HandleKeyTyped(const KeyTypedEvent& event)
    {
        Widget::HandleKeyTyped(event);

        if (!m_Focused) return;

        // Delete selection first
        if (m_SelectionStart != -1)
        {
            m_Text.erase(m_SelectionStart, m_SelectionEnd - m_SelectionStart);
            m_CursorPosition = m_SelectionStart;
            ClearSelection();
        }

        ResetCursorState();

        // Insert UTF-8 character at cursor position
        const auto c = UTF8::CodepointToUTF8(event.GetKeyCode());
        InsertText(c);
    }

    void TextField::HandleFocus()
    {
        Widget::HandleFocus();
        ResetCursorState();
    }

    void TextField::HandleLostFocus()
    {
        Widget::HandleLostFocus();
        m_CursorVisible = false;
    }

    glm::vec2 TextField::MeasureTextSize(const std::string& text)
    {
        return FontManager::MeasureText(text, m_Font, m_FontSize);
    }

    glm::vec2 TextField::CalculateTextPosition(const glm::vec2 textSize)
    {
        const glm::vec2 availableSpace = m_Geometry.Size - glm::vec2(
            m_Padding.GetTotalHorizontal(),
            m_Padding.GetTotalVertical()
        );

        const auto padding = glm::vec2(m_Padding.Left, m_Padding.Top);
        glm::vec2 textPos = m_Geometry.Position + padding;
        textPos.x -= m_ScrollOffset;
        textPos.y += (availableSpace.y - textSize.y) * 0.5f;

        return textPos;
    }

    float TextField::GetTextWidth(
        const std::string& text,
        const int startIndex,
        const int endIndex
    ) const
    {
        const auto str = text.substr(startIndex, endIndex - startIndex);
        return FontManager::MeasureText(str, m_Font, m_FontSize).x;
    }

    int TextField::GetCharIndexAtX(const std::string& text, const float x) const
    {
        if (text.empty() || x <= 0) return 0;

        int index = 0;
        float accumX = 0.0f;

        while (index < text.size())
        {
            // Get byte length of current UTF-8 character
            const int charLen = UTF8::UTF8CharLength(text[index]);
            const float charWidth = GetTextWidth(text, index, index + charLen);

            // Check if click is within this character
            if (x < accumX + charWidth * 0.5f)
                return index;

            accumX += charWidth;
            index += charLen;
        }

        return (int)text.size(); // click past end
    }

    void TextField::UpdateCursorState(const Timestep frameTime)
    {
        if (!m_Focused) return;

        m_BlinkTimer += frameTime.GetSeconds();
        if (m_BlinkTimer >= m_BlinkInterval)
        {
            m_BlinkTimer -= m_BlinkInterval;
            m_CursorVisible = !m_CursorVisible;
        }
    }

    void TextField::ResetCursorState()
    {
        m_BlinkTimer = 0.0f;
        m_CursorVisible = true;
    }

    void TextField::UpdateScrollOffset()
    {
        const float innerWidth = m_Geometry.Size.x - m_Padding.GetTotalHorizontal();
        const float cursorX = GetTextWidth(m_Text, 0, m_CursorPosition);

        // Cursor went past the right edge -> scroll right
        if (cursorX - m_ScrollOffset > innerWidth)
            m_ScrollOffset = cursorX - innerWidth;

        // Cursor went past the left edge -> scroll left
        if (cursorX - m_ScrollOffset < 0)
            m_ScrollOffset = cursorX;

        // Clamp: never scroll past the end of the text
        const float textWidth = GetTextWidth(m_Text, 0, m_Text.size());
        const float maxScroll = std::max(0.0f, textWidth - innerWidth);
        m_ScrollOffset = std::clamp(m_ScrollOffset, 0.0f, maxScroll);
    }

    void TextField::SelectText(const size_t start, const size_t end)
    {
        m_SelectionStart = start;
        m_SelectionEnd = end;
    }

    void TextField::SelectWholeText()
    {
        SelectText(0, m_Text.size());
        m_CursorPosition = m_SelectionEnd;
    }

    void TextField::ClearSelection()
    {
        m_SelectionStart = m_SelectionEnd = -1;
    }

    void TextField::MoveCursorLeft()
    {
        if (m_CursorPosition <= 0) return;
        m_CursorPosition--;
        UpdateScrollOffset();
    }

    void TextField::MoveCursorRight()
    {
        if (m_CursorPosition >= m_Text.size()) return;
        m_CursorPosition++;
        UpdateScrollOffset();
    }

    void TextField::MoveCursorToStart()
    {
        m_CursorPosition = 0;
        UpdateScrollOffset();
    }

    void TextField::MoveCursorToEnd()
    {
        m_CursorPosition = m_Text.size();
        UpdateScrollOffset();
    }

    void TextField::InsertText(const std::string& text)
    {
        m_Text.insert(m_CursorPosition, text);
        m_CursorPosition += text.size();
        UpdateScrollOffset();

        // Fire input changed callback
        if (m_OnChangeCallback) m_OnChangeCallback(m_Text);
    }

    void TextField::ClearNextCharacter()
    {
        if (m_CursorPosition >= m_Text.size()) return;

        const auto len = UTF8::UTF8CharLength(m_Text[m_CursorPosition]);
        m_Text.erase(m_CursorPosition, len);
        UpdateScrollOffset();

        // Fire input changed callback
        if (m_OnChangeCallback) m_OnChangeCallback(m_Text);
    }

    void TextField::ClearPreviousCharacter()
    {
        if (m_Text.empty() || m_CursorPosition <= 0) return;

        const auto len = UTF8::UTF8PrevCharLength(m_Text, m_CursorPosition);
        m_Text.erase(m_CursorPosition - len, len);
        m_CursorPosition -= len;
        UpdateScrollOffset();

        // Fire input changed callback
        if (m_OnChangeCallback) m_OnChangeCallback(m_Text);
    }

    void TextField::CopyToClipboard(const std::string& text)
    {
        // TODO: Platform-specific clipboard handling
    }

    std::string TextField::GetFromClipboard()
    {
        // TODO: Platform-specific clipboard handling
        return "Test";
    }
}