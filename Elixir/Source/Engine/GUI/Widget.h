#pragma once

                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    #include <Engine/Core/Timer.h>
#include <Engine/GUI/Types.h>
#include <Engine/GUI/RenderBatch.h>

namespace Elixir::GUI
{
    class Manager;

    class ELIXIR_API Widget
    {
        friend class Manager;
      public:
        virtual ~Widget() = default;

        /**
         * Handles the update logic for the widget.
         * @param frameTime time since the last tick.
         */
        virtual void Tick(Timestep frameTime);

        //void AddChild(const Ref<Widget>& child);

        /**
         * Generate the draw commands for this widget.
         * @param batch batch to add draw commands to.
         * @param zOrder z-order for layering.
         */
        virtual void GenerateDrawCommands(RenderBatch& batch, int zOrder = 0) = 0;

        /**
         * Compute how much space this widget wants.
         * @return a 2d vector representing width and height.
         */
        virtual glm::vec2 ComputeDesiredSize() = 0;

        /**
         * Arrange this widget in the given space.
         * @param allocatedSpace the space available for this widget.
         */
        virtual void ArrangeChildren(const SRect& allocatedSpace)
        {
            m_Geometry = allocatedSpace;
        }

        /**
         * Get the final computed geometry.
         * @return rect representing the widget geometry.
         */
        SRect GetGeometry() const { return m_Geometry; }

        //const std::vector<Ref<Widget>>& GetChildren() const { return m_Children; }

      protected:
        SRect m_Geometry{};
        glm::vec2 m_DesiredSize{};

        std::vector<Ref<Widget>> m_Children;
    };
}