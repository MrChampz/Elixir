#pragma once

#include <Engine/Graphics/Material/MaterialGraph.h>
#include <Engine/Graphics/Material/MaterialGraphDocument.h>
#include <Engine/Graphics/Texture.h>

#include <glm/glm.hpp>

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Elixir
{
    // A single named material parameter value. A parameter is one of a scalar, a
    // vector, or a texture; the active kind is given by Type.
    struct SMaterialParam
    {
        enum class EType : uint8_t { Scalar, Vector, Texture };

        EType Type = EType::Scalar;
        float Scalar = 0.0f;
        glm::vec4 Vector{ 0.0f };
        Ref<Texture> TextureRef;

        static SMaterialParam MakeScalar(float v) { SMaterialParam p; p.Type = EType::Scalar; p.Scalar = v; return p; }
        static SMaterialParam MakeVector(const glm::vec4& v) { SMaterialParam p; p.Type = EType::Vector; p.Vector = v; return p; }
        static SMaterialParam MakeTexture(const Ref<Texture>& t) { SMaterialParam p; p.Type = EType::Texture; p.TextureRef = t; return p; }
    };

    // One exposed graph parameter and the cbGraphParams slot assigned to it. The
    // default value lives in Material::m_Defaults under Name; instances override that
    // same value instead of maintaining a separate graph-only parameter store.
    struct SMaterialGraphParameter
    {
        std::string Name;
        SMaterialParam::EType Type = SMaterialParam::EType::Scalar;
        uint32_t Slot = 0;
    };

    // A material template: a named set of parameters with default values (the schema
    // shared by all of its instances). Analogous to an Unreal parent material. The
    // material may also own the graph that defines its shading. Built-in materials
    // without a graph continue to use the renderer's fixed shader.
    class ELIXIR_API Material
    {
      public:
        explicit Material(std::string name) : m_Name(std::move(name)) {}

        void SetDefault(const std::string& name, const SMaterialParam& value) { m_Defaults[name] = value; }

        [[nodiscard]] const SMaterialParam* GetDefault(const std::string& name) const
        {
            const auto it = m_Defaults.find(name);
            return it != m_Defaults.end() ? &it->second : nullptr;
        }

        [[nodiscard]] const std::string& GetName() const { return m_Name; }
        [[nodiscard]] const std::unordered_map<std::string, SMaterialParam>& GetDefaults() const { return m_Defaults; }

        void SetStaticDefault(const std::string& name, bool value) { m_StaticDefaults[name] = value; }
        [[nodiscard]] const bool* GetStaticDefault(const std::string& name) const
        {
            const auto it = m_StaticDefaults.find(name);
            return it != m_StaticDefaults.end() ? &it->second : nullptr;
        }
        [[nodiscard]] const std::unordered_map<std::string, bool>& GetStaticDefaults() const
        {
            return m_StaticDefaults;
        }

        void SetGraph(MaterialGraph graph, std::vector<SMaterialGraphParameter> parameters)
        {
            m_Graph = std::move(graph);
            m_GraphParameters = std::move(parameters);
        }
        [[nodiscard]] bool HasGraph() const { return m_Graph.has_value(); }
        [[nodiscard]] const MaterialGraph* GetGraph() const { return m_Graph ? &*m_Graph : nullptr; }
        [[nodiscard]] const std::vector<SMaterialGraphParameter>& GetGraphParameters() const { return m_GraphParameters; }

        // The authored graph this material was built from, kept so the material can be
        // saved and reopened for editing. GetGraph() is the compiled result of it.
        void SetDocument(SMaterialGraphDocument document) { m_Document = std::move(document); }
        [[nodiscard]] const SMaterialGraphDocument* GetDocument() const
        {
            return m_Document ? &*m_Document : nullptr;
        }

        // Built-in metallic-roughness PBR template: declares every parameter the
        // Model shader consumes, with glTF-default values.
        static Ref<Material> CreateStandardPBR();

      private:
        std::string m_Name;
        std::unordered_map<std::string, SMaterialParam> m_Defaults;
        std::unordered_map<std::string, bool> m_StaticDefaults;
        std::optional<MaterialGraph> m_Graph;
        std::optional<SMaterialGraphDocument> m_Document;
        std::vector<SMaterialGraphParameter> m_GraphParameters;
    };

    // An instance of a Material: overrides a subset of the parent's parameters. Any
    // parameter not overridden falls back to the parent default. Runtime parameters
    // update buffers directly; static bool overrides select a cached shader variant.
    class ELIXIR_API MaterialInstance
    {
      public:
        explicit MaterialInstance(const Ref<Material>& parent) : m_Parent(parent) {}

        void SetScalar(const std::string& name, float value) { m_Overrides[name] = SMaterialParam::MakeScalar(value); }
        void SetVector(const std::string& name, const glm::vec4& value) { m_Overrides[name] = SMaterialParam::MakeVector(value); }
        void SetTexture(const std::string& name, const Ref<Texture>& value) { m_Overrides[name] = SMaterialParam::MakeTexture(value); }
        void SetStaticBool(const std::string& name, bool value) { m_StaticOverrides[name] = value; }

        void SetParent(const Ref<Material>& parent);

        // Merge values declared by this instance's parent from another instance.
        // Used when a compiled graph material and its edited values are installed
        // together at a render-frame boundary.
        void ApplyCompatibleOverridesFrom(const MaterialInstance& source);

        [[nodiscard]] float GetScalar(const std::string& name) const
        {
            const auto* p = Resolve(name);
            return p ? p->Scalar : 0.0f;
        }
        [[nodiscard]] glm::vec4 GetVector(const std::string& name) const
        {
            const auto* p = Resolve(name);
            return p ? p->Vector : glm::vec4(0.0f);
        }
        [[nodiscard]] Ref<Texture> GetTexture(const std::string& name) const
        {
            const auto* p = Resolve(name);
            return p ? p->TextureRef : nullptr;
        }

        [[nodiscard]] const SMaterialParam* GetParameter(const std::string& name) const { return Resolve(name); }
        [[nodiscard]] bool GetStaticBool(const std::string& name) const
        {
            const auto it = m_StaticOverrides.find(name);
            if (it != m_StaticOverrides.end())
                return it->second;
            const bool* value = m_Parent ? m_Parent->GetStaticDefault(name) : nullptr;
            return value ? *value : false;
        }

        // Build the graph permutation selected by this instance's compile-time values.
        [[nodiscard]] MaterialGraph BuildGraphVariant() const;
        [[nodiscard]] size_t StaticVariantKey() const;

        // Pack graph parameters in the exact slot layout owned by the parent Material.
        // Returns the highest populated slot count (bounded by maxCount).
        uint32_t CollectGraphParams(glm::vec4* out, uint32_t maxCount) const;

        [[nodiscard]] const Ref<Material>& GetParent() const { return m_Parent; }
        [[nodiscard]] const std::unordered_map<std::string, SMaterialParam>& GetOverrides() const { return m_Overrides; }
        [[nodiscard]] const std::unordered_map<std::string, bool>& GetStaticOverrides() const
        {
            return m_StaticOverrides;
        }

        void SetName(std::string name) { m_Name = std::move(name); }
        [[nodiscard]] const std::string& GetName() const { return m_Name; }

      private:
        std::string m_Name;
        // Override if present, else the parent's default (or null).
        [[nodiscard]] const SMaterialParam* Resolve(const std::string& name) const
        {
            const auto it = m_Overrides.find(name);
            if (it != m_Overrides.end())
                return &it->second;
            return m_Parent ? m_Parent->GetDefault(name) : nullptr;
        }

        Ref<Material> m_Parent;
        std::unordered_map<std::string, SMaterialParam> m_Overrides;
        std::unordered_map<std::string, bool> m_StaticOverrides;
    };
}
