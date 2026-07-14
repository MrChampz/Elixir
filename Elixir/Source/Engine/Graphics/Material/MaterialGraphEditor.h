#pragma once

#include <Engine/Graphics/Material/MaterialGraph.h>

#include <glm/glm.hpp>

#include <vector>
#include <string>

namespace Elixir
{
    // A minimal ImGui node-graph editor: draggable nodes, drag-to-connect pins,
    // and a fixed "Output" node driving the surface channels. Build() turns the
    // current state into a MaterialGraph (to be compiled by MaterialCompiler).
    class ELIXIR_API MaterialGraphEditor
    {
      public:
        MaterialGraphEditor();

        // Draws the editor window. Returns true the frame "Apply" is pressed.
        bool Draw();

        // Convert the current visual state into a compilable graph.
        [[nodiscard]] MaterialGraph Build() const;

      private:
        struct SNode
        {
            int Id = 0;
            EMaterialNodeType Type = EMaterialNodeType::Constant;
            EGraphValueType OutputType = EGraphValueType::Float4;
            glm::vec2 Pos{ 0.0f, 0.0f };

            glm::vec4 Constant{ 1.0f, 1.0f, 1.0f, 1.0f }; // Constant
            char Param[64] = "BaseColorFactor";           // Parameter
            int TexSlot = 0;                              // TextureSample (0=base..4=occlusion)
            int InputCount = 0;
            int Inputs[3] = { -1, -1, -1 };               // source node ids
        };

        int AddNode(EMaterialNodeType type, const glm::vec2& pos);
        const SNode* Find(int id) const;

        std::vector<SNode> m_Nodes;
        int m_NextId = 1;

        // The four surface channels (BaseColor, Metallic, Roughness, Emissive):
        // node id feeding each, or -1.
        int m_Channels[4] = { -1, -1, -1, -1 };

        // Drag-to-connect: the node whose output pin is being dragged (-1 = none).
        int m_LinkFrom = -1;
    };
}
