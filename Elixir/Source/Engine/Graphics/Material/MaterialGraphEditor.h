#pragma once

#include <Engine/Graphics/Material/Material.h>
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

        // Build a Material asset that owns the graph and its exposed-parameter layout.
        // Defaults from base are copied so graph Parameter nodes can still read the
        // StandardPBR/glTF fields used by the fixed GPU material structure.
        [[nodiscard]] Ref<Material> BuildMaterial(const Ref<Material>& base = nullptr) const;

        // Synchronize the editor controls with a graph-backed instance and write edits
        // back to that instance. MaterialInstance remains the runtime source of truth.
        void SyncParametersFrom(const MaterialInstance& instance);
        void ApplyParametersTo(MaterialInstance& instance) const;

        // Max number of exposed parameters (matches GraphParams[] in the shader).
        static constexpr int MAX_PARAMS = 8;

      private:
        struct SNode
        {
            int Id = 0;
            // Visible top-level node that receives diagnostics. Nodes expanded from a
            // function inherit the FunctionCall's id.
            int DiagnosticId = -1;
            std::vector<std::string> FunctionStack;
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

        // A labelled rectangle drawn behind the nodes to group/annotate the graph.
        struct SComment
        {
            glm::vec2 Pos{ 0.0f, 0.0f };
            glm::vec2 Size{ 220.0f, 140.0f };
            char Text[128] = "Comment";
        };

        int AddNode(EMaterialNodeType type, const glm::vec2& pos);
        void DeleteNode(int id); // removes the node and clears links referencing it
        const SNode* Find(int id) const;

        // Parse a saved graph file into raw nodes/channels (no state change).
        bool ParseGraphFile(const std::string& path, std::vector<SNode>& nodes, int channels[11],
            int& targetMaterial, int& nextId, bool logErrors = true) const;

        // Flatten the graph by inlining FunctionCall nodes (loading their saved
        // sub-graphs, wiring FunctionInput placeholders to the call's inputs, and
        // redirecting the call's output to the function's Base Color result).
        void Expand(std::vector<SNode>& outNodes, int outChannels[11]) const;

        // A hash of the graph state that affects the compiled shader (structure,
        // wiring, channels, baked constants, target slot). Excludes exposed-parameter
        // values, which update live without recompiling. Drives live-preview.
        [[nodiscard]] size_t Signature() const;

        // Cheap per-frame hash of the shader-affecting editor state (no HLSL
        // generation), used to gate the expensive Signature() so it only runs when
        // the graph actually changed.
        [[nodiscard]] size_t StructHash() const;

        // --- Undo / redo ---
        // A full snapshot of the editable document state (not view state like pan).
        struct SUndoState
        {
            std::vector<SNode> Nodes;
            int Channels[11] = {};
            int TargetMaterial = 0, BlendMode = 0, ShadingModel = 0, NextId = 1;
            float AlphaCutoff = 0.5f;
            std::unordered_map<std::string, glm::vec4> Overrides;
            std::vector<SComment> Comments;
        };
        [[nodiscard]] SUndoState Capture() const;
        void Restore(const SUndoState& s);
        [[nodiscard]] static size_t HashState(const SUndoState& s);
        void CommitUndoIfSettled(bool interacting);

        std::vector<SUndoState> m_Undo, m_Redo;
        SUndoState m_Committed;      // the last committed (stable) state
        size_t m_CommittedHash = 0;

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

        // Canvas pan offset (drag empty background or middle-mouse to move all nodes).
        glm::vec2 m_Pan{ 0.0f, 0.0f };

        // Selection (click / shift-click node headers) and copy/paste clipboard.
        std::vector<int> m_Selected;
        std::vector<SNode> m_Clipboard;

        // Comment boxes (annotations behind the nodes).
        std::vector<SComment> m_Comments;

        // Live preview: auto-recompile a short debounce after the graph changes.
        bool m_LivePreview = true;
        size_t m_LastSig = 0;
        size_t m_LastStructHash = 0;
        double m_DirtySince = -1.0; // < 0 = not dirty

        std::vector<SMaterialGraphDiagnostic> m_Diagnostics;
    };
}
