#pragma once

#include <Engine/Material/Material.h>

namespace Elixir
{
    class ELIXIR_API MaterialInstance
    {
    public:
        explicit MaterialInstance(const Ref<Material>& parent) : m_Parent(parent) {}

        bool SetScalar(const std::string& name, float value);
        float GetScalar(const std::string& name) const;

        bool SetVector(const std::string& name, const glm::vec4& value);
        glm::vec4 GetVector(const std::string& name) const;

        bool SetTexture(const std::string& name, const Ref<Texture>& texture);
        Ref<Texture> GetTexture(const std::string& name) const;

        const Ref<Material>& GetParent() const { return m_Parent; }
        uint32_t GetRevision() const { return m_Revision; }

        const SMaterialParam* GetResolvedParameter(const std::string& name) const;

    private:
        bool SetOverride(const std::string& name, const SMaterialParam& value);

        // Override if present, else the parent's default (or null).
        const SMaterialParam* Resolve(const std::string& name) const;

        Ref<Material> m_Parent;
        std::unordered_map<std::string, SMaterialParam> m_Overrides;
        uint32_t m_Revision = 1;
    };
}
