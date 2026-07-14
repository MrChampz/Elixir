#pragma once

#include <Engine/Graphics/Texture.h>

#include <glm/glm.hpp>

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

    // A material template: a named set of parameters with default values (the schema
    // shared by all of its instances). Analogous to an Unreal parent material. The
    // shading itself is provided by the renderer's shader; a Material describes the
    // parameters that feed it.
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

        // Built-in metallic-roughness PBR template: declares every parameter the
        // Model shader consumes, with glTF-default values.
        static Ref<Material> CreateStandardPBR();

      private:
        std::string m_Name;
        std::unordered_map<std::string, SMaterialParam> m_Defaults;
    };

    // An instance of a Material: overrides a subset of the parent's parameters. Any
    // parameter not overridden falls back to the parent default, so instances are
    // cheap to author and never recompile a shader.
    class ELIXIR_API MaterialInstance
    {
      public:
        explicit MaterialInstance(const Ref<Material>& parent) : m_Parent(parent) {}

        void SetScalar(const std::string& name, float value) { m_Overrides[name] = SMaterialParam::MakeScalar(value); }
        void SetVector(const std::string& name, const glm::vec4& value) { m_Overrides[name] = SMaterialParam::MakeVector(value); }
        void SetTexture(const std::string& name, const Ref<Texture>& value) { m_Overrides[name] = SMaterialParam::MakeTexture(value); }

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

        [[nodiscard]] const Ref<Material>& GetParent() const { return m_Parent; }

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
    };
}
