#pragma once

#include <Engine/Font/Font.h>
#include <Engine/GUI/Widget.h>

namespace Elixir::GUI
{
    /** @brief Text-field visual data for one interactive state. */
    struct STextFieldAppearance : SAppearance
    {
        SColor Foreground{};
    };

    /** @brief Complete visual style for TextField. */
    struct STextFieldStyle final : SStyle, TStateStyles<STextFieldAppearance>{};

    class ELIXIR_API TextField : public Widget
    {
      public:
        /**
         * @brief Construct a text field with a copy of the current default text-field style.
         * @param text Initial text.
         */
        explicit TextField(const std::string& text = "");

        /**
         * @brief Replace this text field's complete style.
         * @param style Style to copy.
         */
        void SetStyle(const STextFieldStyle& style);

        void Update(Timestep frameTime) override;

        /* Callbacks */

        void OnChange(const std::function<void(const std::string&)>& callback)
        {
            m_OnChangeCallback = callback;
        }

        const Ref<Font>& GetFont() const { return m_Font; }
        void SetFont(const Ref<Font>& font);

        float GetFontSize() const { return m_FontSize; }
        void SetFontSize(float size);

        const std::string& GetText() const { return m_Text; }
        void SetText(const std::string& text);

        /**
         * @brief Set one legacy layer's text color.
         * @param layer Layer to change.
         * @param color New text color.
         */
        void SetTextColor(EStyleLayer layer, const SColor& color);

        const std::string& GetPlaceholder() const { return m_Placeholder; }
        void SetPlaceholder(const std::string& placeholder);

        SColor GetPlaceholderColor() const { return m_PlaceholderColor; }
        void SetPlaceholderColor(const SColor& color);

        SPadding GetPadding() const { return m_Padding; }
        void SetPadding(const SPadding& padding);

        SColor GetCursorColor() const { return m_CursorColor; }
        void SetCursorColor(const SColor& color);

        SColor GetSelectionColor() const { return m_SelectionColor; }
        void SetSelectionColor(const SColor& color);

        bool CanHandleMouseInput() const override { return IsEnabled(); }

      protected:
        glm::vec2 ComputeDesiredSize(const glm::vec2& availableSize) override;
        void LayoutChildren(const SRect& allocatedSpace) override;
        void BuildDrawCommands(RenderBatch& batch, int zOrder) override;

        void HandleMouseEnter() override;
        void HandleMouseLeave() override;
        SInputReply HandleMouseDown(const MouseButtonPressedEvent& event) override;
        SInputReply HandleMouseMove(const MouseMovedEvent& event) override;
        SInputReply HandleKeyPressed(const KeyPressedEvent& event) override;
        SInputReply HandleKeyTyped(const KeyTypedEvent& event) override;
        void HandleFocus() override;
        void HandleLostFocus() override;

        const SAppearance& GetResolvedAppearance() const override;
        SBrush& GetMutableBackgroundBrush(EStyleLayer layer) override;

        virtual glm::vec2 MeasureTextSize(const std::string& text);
        virtual glm::vec2 CalculateTextPosition(glm::vec2 textSize);

        float GetTextWidth(const std::string& text, int startIndex, int endIndex) const;
        int GetCharIndexAtX(const std::string& text, float x) const;

        void UpdateCursorState(Timestep frameTime);
        void ResetCursorState();

        void UpdateScrollOffset();

        void SelectText(size_t start, size_t end);
        void SelectWholeText();
        void ClearSelection();

        void MoveCursorLeft();
        void MoveCursorRight();
        void MoveCursorToStart();
        void MoveCursorToEnd();

        void InsertText(const std::string& text);
        void DeleteSelectedText();
        void ClearNextCharacter();
        void ClearPreviousCharacter();

        static void CopyToClipboard(const std::string& text);
        static std::string GetFromClipboard();

      private:
        Ref<Font> m_Font;
        float m_FontSize = 16.0f;

        std::string m_Text;

        std::string m_Placeholder;
        SColor m_PlaceholderColor{0.3f, 0.3f, 0.3f, 1.0f};

        SPadding m_Padding = { 5.0f, 5.0f, 5.0f, 5.0f };

        // Cursor blinking related stuff
        float m_BlinkTimer = 0.0f;
        float m_BlinkInterval = 0.5f; // Cursor blink interval in seconds
        bool m_CursorVisible = false;
        size_t m_CursorPosition = 0;
        SColor m_CursorColor = { 0.0f, 0.0f, 0.0f, 1.0f };

        // Horizontal scroll offset for text that exceeds the field width
        float m_ScrollOffset = 0.0f;

        // Selection
        size_t m_SelectionStart = -1;
        size_t m_SelectionEnd = -1;
        SColor m_SelectionColor = { 0.3f, 0.5f, 1.0f, 0.4f };

        glm::vec2 m_MinDesiredSize{ 120.0f, 30.0f };
        STextFieldStyle m_Style;

        // Callbacks
        std::function<void(const std::string&)> m_OnChangeCallback;
    };
}
