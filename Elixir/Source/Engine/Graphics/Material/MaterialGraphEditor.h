#pragma once

#include <Engine/Graphics/Material/Material.h>
#include <Engine/Graphics/Material/MaterialGraph.h>
#include <Engine/Graphics/Material/MaterialGraphDocument.h>

#include <glm/glm.hpp>

#include <filesystem>
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
        [[nodiscard]] int TargetMaterial() const { return m_Document.TargetMaterial; }

        // The graph's blend mode (drives the renderer's pipeline/pass selection).
        [[nodiscard]] EMaterialBlendMode BlendMode() const { return (EMaterialBlendMode)m_Document.BlendMode; }
        [[nodiscard]] EMaterialDomain Domain() const { return (EMaterialDomain)m_Document.Domain; }
        [[nodiscard]] EMaterialUsage Usages() const { return (EMaterialUsage)m_Document.Usages; }

        // The authored graph being edited. Reading it yields something serializable
        // without the editor; setting it drops the view state that no longer applies.
        [[nodiscard]] const SMaterialGraphDocument& GetDocument() const { return m_Document; }
        void SetDocument(const SMaterialGraphDocument& document);

        [[nodiscard]] bool SavedThisFrame() const { return m_SavedThisFrame; }
        [[nodiscard]] bool LoadedThisFrame() const { return m_LoadedThisFrame; }
        [[nodiscard]] std::string AssetName() const { return m_FileName; }
        [[nodiscard]] std::filesystem::path MaterialPath() const
        {
            return std::filesystem::path("./Assets/Materials") / (AssetName() + ".material.json");
        }

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
        // The document owns the node/comment types; these keep the drawing code reading
        // in the editor's own terms.
        using SNode = SMaterialGraphNode;
        using SComment = SMaterialGraphComment;

        int AddNode(EMaterialNodeType type, const glm::vec2& pos);
        void DeleteNode(int id); // removes the node and clears links referencing it
        const SNode* Find(int id) const;

        // A hash of the graph state that affects the compiled shader (structure,
        // wiring, channels, baked constants, target slot). Excludes exposed-parameter
        // values, which update live without recompiling. Drives live-preview.
        [[nodiscard]] size_t Signature() const;

        // --- Undo / redo ---
        // One step is a whole document; view state (pan, selection) is deliberately not
        // part of it, so undo never moves the camera out from under the user.
        void Restore(const SMaterialGraphDocument& document);
        void CommitUndoIfSettled(bool interacting);

        std::vector<SMaterialGraphDocument> m_Undo, m_Redo;
        SMaterialGraphDocument m_Committed; // the last committed (stable) state
        size_t m_CommittedHash = 0;

        // Everything being authored: nodes, wiring, channels, comments, blend and
        // shading. The editor draws this; the asset layer serializes it.
        SMaterialGraphDocument m_Document;

        // File name (without extension/dir) for Save/Load.
        char m_FileName[128] = "material";
        bool m_SavedThisFrame = false;
        bool m_LoadedThisFrame = false;

        // Drag-to-connect: the node whose output pin is being dragged (-1 = none).
        int m_LinkFrom = -1;

        // Canvas pan offset (drag empty background or middle-mouse to move all nodes).
        glm::vec2 m_Pan{ 0.0f, 0.0f };

        // Selection (click / shift-click node headers) and copy/paste clipboard.
        std::vector<int> m_Selected;
        std::vector<SNode> m_Clipboard;

        // Live preview: auto-recompile a short debounce after the graph changes.
        bool m_LivePreview = true;
        size_t m_LastSig = 0;
        size_t m_LastStructHash = 0;
        double m_DirtySince = -1.0; // < 0 = not dirty

        std::vector<SMaterialGraphDiagnostic> m_Diagnostics;
    };
}
