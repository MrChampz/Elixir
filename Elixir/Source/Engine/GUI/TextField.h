#pragma once

#include <Engine/Font/Font.h>
#include <Engine/GUI/Widget.h>

namespace Elixir::GUI
{
    class ELIXIR_API TextField : public Widget
    {
      public:
        explicit TextField(const std::string& text = "");

        void Update(Timestep frameTime) override;

        glm::vec2 ComputeDesiredSize() override;
        void GenerateDrawCommands(RenderBatch& batch, int zOrder) override;

        /* Callbacks */

        void OnChange(const std::function<void(const std::string&)>& callback) { m_OnChangeCallback = callback; }

        const Ref<Font>& GetFont() const { return m_Font; }
        void SetFont(const Ref<Font>& font) { m_Font = font; }

        float GetFontSize() const { return m_FontSize; }
        void SetFontSize(const float size) { m_FontSize = size; }

        const std::string& GetText() const { return m_Text; }
        void SetText(const std::string& text) { m_Text = text; }

        SColor GetTextColor() const { return m_TextColor; }
        void SetTextColor(const SColor& color) { m_TextColor = color; }

        const std::string& GetPlaceholder() const { return m_Placeholder; }
        void SetPlaceholder(const std::string& placeholder) { m_Placeholder = placeholder; }

        SColor GetPlaceholderColor() const { return m_PlaceholderColor; }
        void SetPlaceholderColor(const SColor& color) { m_PlaceholderColor = color; }

        SPadding GetPadding() const { return m_Padding; }
        void SetPadding(const SPadding& padding) { m_Padding = padding; }

        /**
         * Get corner radius for each corner individually.
         * @return vector (top-left, top-right, bottom-right, bottom-left)
         */
        glm::vec4 GetCornerRadius() const { return m_CornerRadius; }

        /**
         * Set the same radius for all corners.
         * @param radius corner radius in pixels
         */
        void SetCornerRadius(const float radius)
        {
            SetCornerRadius({ radius, radius, radius, radius });
        }

        /**
         * Set a radius for each corner individually.
         * @param radius vector (top-left, top-right, bottom-right, bottom-left)
         */
        void SetCornerRadius(const glm::vec4& radius) { m_CornerRadius = radius; }

        SColor GetBackgroundColor() const { return m_BackgroundColor; }
        void SetBackgroundColor(const SColor& color) { m_BackgroundColor = color; }

        const glm::vec4& GetBackgroundBorders() const { return m_BackgroundBorders; }
        void SetBackgroundBorders(const glm::vec4& borders) { m_BackgroundBorders = borders; }

        const Ref<Texture2D>& GetBackground() const { return m_Background; }
        void SetBackground(const Ref<Texture2D>& texture) { m_Background = texture; }

        SColor GetCursorColor() const { return m_CursorColor; }
        void SetCursorColor(const SColor& color) { m_CursorColor = color; }

        SColor GetSelectionColor() const { return m_SelectionColor; }
        void SetSelectionColor(const SColor& color) { m_SelectionColor = color; }

    protected:
        void HandleMouseEnter() override;
        void HandleMouseLeave() override;
        void HandleMouseDown(const MouseButtonPressedEvent& event) override;
        void HandleMouseMove(const MouseMovedEvent& event) override;
        void HandleKeyPressed(const KeyPressedEvent& event) override;
        void HandleKeyTyped(const KeyTypedEvent& event) override;
        void HandleFocus() override;
        void HandleLostFocus() override;

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
        void ClearNextCharacter();
        void ClearPreviousCharacter();

        static void CopyToClipboard(const std::string& text);
        static std::string GetFromClipboard();

      private:
        Ref<Font> m_Font;
        float m_FontSize = 16.0f;

        std::string m_Text;
        SColor m_TextColor{0.0f, 0.0f, 0.0f, 1.0f};

        std::string m_Placeholder;
        SColor m_PlaceholderColor{0.3f, 0.3f, 0.3f, 1.0f};

        SPadding m_Padding = { 5.0f, 5.0f, 5.0f, 5.0f };

        // top-left, top-right, bottom-right, bottom-left
        glm::vec4 m_CornerRadius = {0.0f, 0.0f, 0.0f, 0.0f};

        // Colors for different states
        SColor m_BackgroundColor{1.0f, 1.0f, 1.0f, 1.0f};

        // When texture is used, this represents the borders of 9-patch texture.
        // Border mapping = (left, top, right, bottom).
        glm::vec4 m_BackgroundBorders = {30.0f, 30.0f, 30.0f, 30.0f};

        // Textures for different states
        Ref<Texture2D> m_Background;

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

        // Callbacks
        std::function<void(const std::string&)> m_OnChangeCallback;
    };
}