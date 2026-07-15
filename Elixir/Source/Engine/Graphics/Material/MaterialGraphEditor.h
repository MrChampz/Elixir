#pragma once

#include <Engine/Graphics/Material/MaterialGraph.h>

#include <glm/glm.hpp>

#include <vector>
#include <string>
#include <unordered_map>

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

        // The graph's blend mode (drives the renderer's pipeline/pass selection).
        [[nodiscard]] EMaterialBlendMode BlendMode() const { return (EMaterialBlendMode)m_BlendMode; }

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
            char Code[256] = "a";                         // Custom (raw HLSL over a,b,c)
            int TexSlot = 0;                              // TextureSample (0=base..4=occlusion)
            int InputCount = 0;
            int Inputs[3] = { -1, -1, -1 };               // source node ids
        };

        int AddNode(EMaterialNodeType type, const glm::vec2& pos);
        void DeleteNode(int id); // removes the node and clears links referencing it
        const SNode* Find(int id) const;

        // Parse a saved graph file into raw nodes/channels (no state change).
        bool ParseGraphFile(const std::string& path, std::vector<SNode>& nodes, int channels[11],
            int& targetMaterial, int& nextId) const;

        // Flatten the graph by inlining FunctionCall nodes (loading their saved
        // sub-graphs, wiring FunctionInput placeholders to the call's inputs, and
        // redirecting the call's output to the function's Base Color result).
        void Expand(std::vector<SNode>& outNodes, int outChannels[11]) const;

        // A hash of the graph state that affects the compiled shader (structure,
        // wiring, channels, baked constants, target slot). Excludes exposed-parameter
        // values, which update live without recompiling. Drives live-preview.
        [[nodiscard]] size_t Signature() const;

        std::vector<SNode> m_Nodes;
        int m_NextId = 1;

        // The seven output channels (BaseColor, Metallic, Roughness, Emissive, Normal,
        // World Pos Offset, Opacity): node id feeding each, or -1.
        int m_Channels[11] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };

        // The material slot the graph is applied to on Apply.
        int m_TargetMaterial = 0;

        // Blend: 0=Opaque, 1=Masked, 2=Translucent, 3=Additive; cutoff used by Masked.
        int m_BlendMode = 0;
        float m_AlphaCutoff = 0.5f;

        // Shading model: 0=DefaultLit, 1=Unlit, 2=Subsurface, 3=ClearCoat, 4=Cloth.
        int m_ShadingModel = 0;

        // File name (without extension/dir) for Save/Load.
        char m_FileName[128] = "material";

        // Live overrides for exposed parameters, keyed by name. Lets parameters that
        // live inside functions (and top-level ones) be edited live without touching
        // their source node/file. Empty entry -> use the node's own value.
        std::unordered_map<std::string, glm::vec4> m_ParamOverrides;

        // Drag-to-connect: the node whose output pin is being dragged (-1 = none).
        int m_LinkFrom = -1;

        // Live preview: auto-recompile a short debounce after the graph changes.
        bool m_LivePreview = true;
        size_t m_LastSig = 0;
        double m_DirtySince = -1.0; // < 0 = not dirty
    };
}
