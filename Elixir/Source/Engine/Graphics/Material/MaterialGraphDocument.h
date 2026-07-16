#pragma once

#include <Engine/Core/Core.h>
#include <Engine/Graphics/Material/MaterialGraph.h>

#include <glm/glm.hpp>

#include <filesystem>
#include <optional>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace Elixir
{
    // Only referenced here; including Material.h would be circular, since a Material
    // owns the document it is built from.
    class Material;
    class MaterialInstance;

    // One authored node. Plain data (no lambdas, fixed-size text) so the document is
    // both what the editor draws and what the asset layer serializes.
    struct SMaterialGraphNode
    {
        int Id = 0;
        // Visible top-level node that receives diagnostics. Nodes expanded from a
        // function inherit the FunctionCall's id.
        int DiagnosticId = -1;
        std::vector<std::string> FunctionStack;
        EMaterialNodeType Type = EMaterialNodeType::Constant;
        EGraphValueType OutputType = EGraphValueType::Float4; // numeric value or Texture2D handle
        glm::vec2 Pos{ 0.0f, 0.0f };

        glm::vec4 Constant{ 1.0f, 1.0f, 1.0f, 1.0f }; // Constant
        char Param[64] = "BaseColorFactor";           // Parameter
        char Code[256] = "a";                         // Custom (raw HLSL over a,b,c)
        int TexSlot = 0;                              // TextureSample (0=base..4=occlusion)
        ETextureSampleType TextureSampleType = ETextureSampleType::Color;
        ETextureSampleAddress TextureSampleAddress = ETextureSampleAddress::Wrap;
        ETextureSampleFilter TextureSampleFilter = ETextureSampleFilter::Linear;
        ETextureSampleMipMode TextureSampleMipMode = ETextureSampleMipMode::Auto;
        ETextureSampleOutput TextureSampleOutput = ETextureSampleOutput::RGB;
        uint8_t ComponentMask = 0x1;                  // ComponentMask: RGBA bits
        int InputCount = 0;
        int Inputs[3] = { -1, -1, -1 };               // source node ids
    };

    // A labelled rectangle drawn behind the nodes to group/annotate the graph.
    struct SMaterialGraphComment
    {
        glm::vec2 Pos{ 0.0f, 0.0f };
        glm::vec2 Size{ 220.0f, 140.0f };
        char Text[128] = "Comment";
    };

    // An authored material graph: the whole editable document, and nothing that is
    // merely how it is being looked at (pan, selection, link-in-progress). It is what
    // Save writes, what Load returns, and what one undo step snapshots. A Material
    // owns one the way an Unreal UMaterial owns its expression graph.
    struct SMaterialGraphDocument
    {
        std::vector<SMaterialGraphNode> Nodes;
        std::vector<SMaterialGraphComment> Comments;

        // Live values for exposed parameters, keyed by name. Lets parameters that live
        // inside functions (and top-level ones) be edited without touching their source
        // node/file. A missing entry -> use the node's own value. Authoring state: the
        // saved instance, not the material, carries these to the runtime.
        std::unordered_map<std::string, glm::vec4> Overrides;

        // Instance-selected compile-time values while editing. Like Overrides, these
        // participate in undo but are not serialized into the parent Material.
        std::unordered_map<std::string, bool> StaticValues;

        // The node id feeding each surface channel (EMaterialChannel order), or -1.
        int Channels[11] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };

        // The material slot the graph is applied to on Apply. Editing state that rides
        // along in undo; deliberately not serialized, since which slot an asset happens
        // to be previewed on is not a property of the material.
        int TargetMaterial = 0;

        // Blend: 0=Opaque, 1=Masked, 2=Translucent, 3=Additive; cutoff used by Masked.
        int BlendMode = 0;
        float AlphaCutoff = 0.5f;

        // Shading model: 0=DefaultLit, 1=Unlit, 2=Subsurface, 3=ClearCoat, 4=Cloth.
        int ShadingModel = 0;

        int NextId = 1;

        // Flatten the graph by inlining FunctionCall nodes (loading their saved
        // sub-graphs, wiring FunctionInput placeholders to the call's inputs, and
        // redirecting the call's output to the function's Base Color result).
        void Expand(std::vector<SMaterialGraphNode>& outNodes, int outChannels[11]) const;

        // Convert the authored graph into a compilable one.
        [[nodiscard]] MaterialGraph Build(
            const std::unordered_map<std::string, bool>& staticOverrides = {}) const;

        // Build a Material that owns this graph and its exposed-parameter layout.
        // Defaults from base are copied so graph Parameter nodes can still read the
        // StandardPBR/glTF fields used by the fixed GPU material structure.
        [[nodiscard]] Ref<Material> BuildMaterial(
            const std::string& name, const Ref<Material>& base = nullptr) const;

        // Write this document's exposed-parameter values into a runtime instance.
        void ApplyParametersTo(MaterialInstance& instance) const;

        // Hash of the fields that can affect the generated shader, skipping node
        // positions/comments and HLSL generation. Gates the expensive Signature().
        [[nodiscard]] size_t StructHash() const;

        // Hash of the whole document, used to detect an undo-worthy change.
        [[nodiscard]] size_t Hash() const;
    };

    // Reads and writes SMaterialGraphDocument as JSON. Kept apart from the graph editor
    // so that loading a material asset does not depend on the authoring tool.
    class ELIXIR_API MaterialGraphDocument
    {
      public:
        // Writes the document's fields (no enclosing braces, no version) so a material
        // asset can embed them alongside its own.
        static void WriteFields(
            std::ostream& out, const SMaterialGraphDocument& document, const std::string& indent = "  ");

        static bool Save(const std::filesystem::path& path, const SMaterialGraphDocument& document);

        // logErrors is off for speculative loads, such as resolving a FunctionCall whose
        // referenced graph may simply not exist yet.
        static std::optional<SMaterialGraphDocument> Load(
            const std::filesystem::path& path, bool logErrors = true);
    };
}
