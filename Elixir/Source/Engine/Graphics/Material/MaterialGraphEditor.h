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
        // materialCount bounds the target-material selector.
        bool Draw(int materialCount);

        // The material slot the compiled graph should be applied to.
        [[nodiscard]] int TargetMaterial() const { return m_TargetMaterial; }

        // Serialize / restore the full editor state (nodes, wiring, channels) to a
        // JSON file. Returns false on IO/parse failure.
        bool Save(const std::string& path) const;
        bool Load(const std::string& path);

        // Convert the current visual state into a compilable graph.
        [[nodiscard]] MaterialGraph Build() const;

        // Max number of exposed parameters (matches GraphParams[] in the shader).
        static constexpr int MAX_PARAMS = 8;

        // Fill `out` with the current exposed-parameter values in slot order (the
        // same order Build() assigns). Returns the number written. Lets the app push
        // live values into the shader's cbGraphParams without recompiling.
        int CollectParams(glm::vec4* out, int maxCount) const;

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
        void DeleteNode(int id); // removes the node and clears links referencing it
        const SNode* Find(int id) const;

        // A hash of the graph state that affects the compiled shader (structure,
        // wiring, channels, baked constants, target slot). Excludes exposed-parameter
        // values, which update live without recompiling. Drives live-preview.
        [[nodiscard]] size_t Signature() const;

        std::vector<SNode> m_Nodes;
        int m_NextId = 1;

        // The six output channels (BaseColor, Metallic, Roughness, Emissive, Normal,
        // World Pos Offset): node id feeding each, or -1.
        int m_Channels[6] = { -1, -1, -1, -1, -1, -1 };

        // The material slot the graph is applied to on Apply.
        int m_TargetMaterial = 0;

        // File name (without extension/dir) for Save/Load.
        char m_FileName[128] = "material";

        // Drag-to-connect: the node whose output pin is being dragged (-1 = none).
        int m_LinkFrom = -1;

        // Live preview: auto-recompile a short debounce after the graph changes.
        bool m_LivePreview = true;
        size_t m_LastSig = 0;
        double m_DirtySince = -1.0; // < 0 = not dirty
    };
}
