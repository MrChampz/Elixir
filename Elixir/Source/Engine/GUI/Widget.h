#pragma once
#include "RenderBatch.h"
#include "Engine/Core/Timer.h"

namespace Elixir::GUI
{
    class ELIXIR_API Widget
    {
      public:
        virtual ~Widget() = default;

        virtual void Tick(Timestep frameTime);

        virtual void GenerateDrawCommands(RenderBatch& batch, int zOrder = 0) = 0;
        virtual glm::vec2 ComputeDesiredSize() { return m_DesiredSize; }

        void AddChild(const Ref<Widget>& child);

        void SetGeometry(const SRect& rect) { m_Geometry = rect; }
        SRect GetGeometry() const { return m_Geometry; }

        const std::vector<Ref<Widget>>& GetChildren() const { return m_Children; }

        void SetVisibility(const bool visible) { m_IsVisible = visible; }
        bool IsVisible() const { return m_IsVisible; }

      protected:
        SRect m_Geometry{};
        glm::vec2 m_DesiredSize{100, 30};

        Widget* m_Parent = nullptr;
        std::vector<Ref<Widget>> m_Children;

        bool m_IsVisible = true;
    };
}