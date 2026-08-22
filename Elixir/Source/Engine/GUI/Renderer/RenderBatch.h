#pragma once

#include <Engine/Font/Font.h>
#include <Engine/GUI/Definitions.h>
#include <Engine/GUI/Style.h>
#include <Engine/Graphics/Texture.h>

namespace Elixir::GUI
{
    enum class EDrawCommandType : uint8_t
    {
        Rect, Text, DebugRect
    };

    /** @brief Controls how a textured quad maps its texture coordinates. */
    enum class ETextureMapping : uint8_t
    {
        Stretch,
        NineSlice,
    };

    struct SDrawCommand
    {
        EDrawCommandType Type;
        SRect Geometry;
        SColor Color;

        /**
         * When texture is used, this represents the borders of 9-patch texture.
         * Border mapping = (left, top, right, bottom).
         *
         * When not used, this represents the corner radius of the quad.
         * Corner Radius mapping = (top-left, top-right, bottom-right, bottom-left).
         */
        glm::vec4 Border = glm::vec4(0.0f);

        /**
         * Shadow offset (x, y), blur (z) and intensity (w).
         */
        glm::vec4 InsetShadow = glm::vec4(0.0f);

        /**
         * Shadow offset (x, y), blur (z) and intensity (w).
         */
        glm::vec4 DropShadow = glm::vec4(0.0f);

        SOutline Outline;

        // For text rendering
        std::string Text;
        Ref<Font> Font;
        float FontSize = 16.0f;

        // For texture rendering
        Ref<Texture2D> Texture;
        SRect TexCoords;
        ETextureMapping TextureMapping = ETextureMapping::Stretch;

        // Z-order for sorting
        int ZOrder = 0;

        // Scissor rect for clipping (optional)
        SRect ScissorRect;
    };

    /**
     * @brief A maximal contiguous slice of same-type commands inside an already
     * z-sorted RenderBatch.
     *
     * [First, First + Count) indexes into RenderBatch::GetCommands().
     */
    struct SBatchRun
    {
        EDrawCommandType Type;
        uint32_t First;
        uint32_t Count;
    };

    class ELIXIR_API RenderBatch final
    {
      public:
        /**
         * @brief Append another batch's commands to this one.
         *
         * Offsets each command's z-order and applies the ancestor clip rect inherited
         * from the caller's position in the widget tree. A command that already carries its
         * own valid ScissorRect (Button/TextField's ad-hoc self-clip) gets that rect
         * intersected with clipRect; a command with no ScissorRect of its own adopts clipRect
         * verbatim, when clipRect itself is valid.
         *
         * Used to assemble the per-widget command caches into the frame batch.
         *
         * @param other Batch whose commands are copied in.
         * @param zOffset Value added to each appended command's ZOrder.
         * @param clipRect Ancestor clip inherit from the caller; pass the invalid
         * {-1, -1}/{-1, -1} sentinel when there is no active clip (see SRect::IsValid).
         */
        void Append(const RenderBatch& other, int zOffset, const SRect& clipRect);

        void Sort();
        void Clear();

        /**
         * Number of distinct z-layers these commands occupy: max ZOrder + 1, or 0 if empty.
         * Used to advance the layer cursor past a widget's own commands during collection.
         */
        int LayerSpan() const;

        /**
         * @brief Add the command needed to draw a brush.
         *
         * A brush with Texture becomes a nine-patch texture command. Otherwise it becomes a
         * solid rectangle command with the brush's radius, outline and shadows.
         *
         * @param brush Surface description to draw.
         * @param rect Destination rectangle.
         * @param zOrder Draw layer for the command.
         * @param scissorRect Optional clip rectangle.
         */
        void AddBrush(
            const SBrush& brush,
            const SRect& rect,
            int zOrder = 0,
            const SRect& scissorRect = {{ -1, -1 }, { -1, -1 }}
        );

        void AddRect(
            const SRect& rect,
            const SColor& color,
            glm::vec4 cornerRadius,
            glm::vec4 insetShadow,
            glm::vec4 dropShadow,
            SOutline outline,
            int zOrder = 0,
            const SRect& scissorRect = {{ -1, -1 }, { -1, -1 }}
        );

        void AddText(
            const std::string& text,
            const SRect& rect,
            const Ref<Font>& font,
            float fontSize,
            const SColor& color,
            int zOrder = 0,
            const SRect& scissorRect = {{ -1, -1 }, { -1, -1 }}
        );

        void AddTexture(
            const Ref<Texture2D>& texture,
            const SRect& rect,
            const glm::vec4& borders,
            const SColor& tint,
            int zOrder = 0,
            const SRect& scissorRect = {{ -1, -1 }, { -1, -1 }}
        );

        /**
         * @brief Add a tinted icon texture without nine-slice mapping.
         *
         * Icon textures use their complete image area. The caller supplies the tint through
         * color, so one alpha mask can render every interaction state.
         *
         * @param texture Rasterized icon texture.
         * @param rect Destination rectangle.
         * @param color Icon tint.
         * @param zOrder Draw layer for the command.
         * @param scissorRect Optional clip rectangle.
         */
        void AddIcon(
            const Ref<Texture2D>& texture,
            const SRect& rect,
            const SColor& color,
            int zOrder = 0,
            const SRect& scissorRect = {{ -1, -1 }, { -1, -1 }}
        );

        void AddDebugRect(const SRect& rect, const SColor& color = { 1.0f, 0.0f, 0.0f, 1.0f });

        const std::vector<SDrawCommand>& GetCommands() const { return m_Commands; }

        /**
         * @brief Contiguous same-type runs over GetCommands(), in z order.
         *
         * Rebuilt by Sort(); stale (from the previous sort) until Sort() runs again.
         *
         * @return A vector of runs.
         */
        const std::vector<SBatchRun>& GetRuns() const { return m_Runs; }

      private:
        // Scans the (already z-sorted) commands and groups neighboring same-type
        // commands into runs. Called by Sort(), right after the stable_sort.
        void BuildRuns();

        std::vector<SDrawCommand> m_Commands;
        std::vector<SBatchRun> m_Runs;
    };
}
