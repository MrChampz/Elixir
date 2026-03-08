#pragma once

#include <Engine/Core/Core.h>

namespace Elixir
{
    enum class ECursorShape : uint8_t
    {
        Arrow = 0,
        IBeam,
        Crosshair,
        Hand,
        NotAllowed,
        ResizeHorizontal,
        ResizeVertical,
    };

    class ELIXIR_API Platform
    {
    public:
        static constexpr auto DEFAULT_CURSOR_SHAPE = ECursorShape::Arrow;

        virtual ~Platform() = default;

        static void Initialize(const Window* window);
        static void Shutdown();

        static Platform& Get();

        virtual void SetDefaultCursorShape();
        virtual void SetPreviousCursorShape() = 0;

        virtual void SetCursorShape(ECursorShape shape) = 0;
        virtual ECursorShape GetCursorShape() = 0;

        virtual std::string GetClipboardText() = 0;
        virtual void SetClipboardText(const std::string& text) = 0;

    protected:
        Platform() = default;

        static Scope<Platform> Create(const Window* window);

    private:
        Platform(const Platform&) = delete;
        Platform& operator=(const Platform&) = delete;

        static Scope<Platform> s_Platform;
    };
}