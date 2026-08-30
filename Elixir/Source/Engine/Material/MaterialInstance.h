#pragma once

#include <Engine/Material/Material.h>

namespace Elixir
{
    /**
     * @brief Stores parameter overrides for one material.
     *
     * An instance uses the parent material's defaults unless it contains an
     * override for the requested parameter.
     */
    class ELIXIR_API MaterialInstance
    {
    public:
        /**
         * @brief Creates an instance for a parent material.
         * @param parent Material that defines the parameter schema and defaults.
         */
        explicit MaterialInstance(const Ref<Material>& parent) : m_Parent(parent) {}

        /**
         * @brief Sets a scalar parameter override.
         * @param name Parameter name.
         * @param value Scalar value to store.
         * @return `true` if the parent defines a compatible parameter.
         */
        bool SetScalar(const std::string& name, float value);

        /**
         * @brief Returns a resolved scalar parameter value.
         * @param name Parameter name.
         * @return Resolved scalar value, or `0.0f` if the parameter is unavailable.
         */
        float GetScalar(const std::string& name) const;

        /**
         * @brief Sets a vector parameter override.
         * @param name Parameter name.
         * @param value Vector value to store.
         * @return `true` if the parent defines a compatible parameter.
         */
        bool SetVector(const std::string& name, const glm::vec4& value);

        /**
         * @brief Returns a resolved vector parameter value.
         * @param name Parameter name.
         * @return Resolved vector value, or a zero vector if the parameter is unavailable.
         */
        glm::vec4 GetVector(const std::string& name) const;

        /**
         * @brief Sets a texture parameter override.
         * @param name Parameter name.
         * @param texture Texture to store.
         * @return `true` if the parent defines a compatible parameter.
         */
        bool SetTexture(const std::string& name, const Ref<Texture>& texture);

        /**
         * @brief Returns a resolved texture parameter value.
         * @param name Parameter name.
         * @return Resolved texture, or null if the parameter is unavailable.
         */
        Ref<Texture> GetTexture(const std::string& name) const;

        /**
         * @brief Returns the effective value of a parameter.
         *
         * Overrides take precedence over the parent material's default.
         *
         * @param name Parameter name.
         * @return Resolved parameter, or null if it is unavailable.
         */
        const SMaterialParam* GetResolvedParameter(const std::string& name) const;

        /** @brief Returns the parent material. */
        const Ref<Material>& GetParent() const { return m_Parent; }

        /** @brief Returns the revision of this instance. */
        uint32_t GetRevision() const { return m_Revision; }

    private:
        // Stores a compatible parameter override.
        bool SetOverride(const std::string& name, const SMaterialParam& value);

        // Finds an override or the parent material's default value.
        const SMaterialParam* Resolve(const std::string& name) const;

        Ref<Material> m_Parent;
        std::unordered_map<std::string, SMaterialParam> m_Overrides;
        uint32_t m_Revision = 1;
    };
}
