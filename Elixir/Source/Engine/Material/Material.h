#pragma once

#include <Engine/Material/MaterialGraph.h>
#include <Engine/Material/MaterialParameter.h>

namespace Elixir
{
    class MaterialInstance;

    /**
     * @brief Identifies a shader permutation supported by a material.
     *
     * A usage selects a renderer-specific implementation without changing the
     * material graph or its parameter schema.
     */
    enum class EMaterialUsage : uint8_t
    {
        /** Material for particle sprites. */
        ParticleSprite = 0,

        /** Material for particle ribbons. */
        ParticleRibbon,

        /** Material for particle meshes. */
        ParticleMesh,

        /** Number of supported usages. */
        Count
    };

    /**
     * @brief Defines the numeric value type used by a material.
     */
    enum class EMaterialValueType : uint8_t
    {
        Float, Float2, Float3, Float4,
    };

    /**
     * @brief Defines one parameter in a material schema.
     */
    struct SMaterialParameterDefinition
    {
        /** Parameter category. */
        EMaterialParameterKind Kind = EMaterialParameterKind::Value;

        /** Expected type for value parameters. */
        EMaterialValueType ValueType = EMaterialValueType::Float4;

        /** Value used when an instance does not provide an override. */
        SMaterialParameter DefaultValue;
    };

    /**
     * @brief Defines a material graph, parameter schema, and supported usages.
     *
     * Instances created from a material inherit its parameter definitions and
     * default values. Changing the graph, schema, or usages increments the
     * material revision.
     */
    class ELIXIR_API Material : public std::enable_shared_from_this<Material>
    {
    public:
        /**
         * @brief Creates a material with a name.
         * @param name Material name.
         */
        explicit Material(std::string name) : m_Name(std::move(name)) {}

        /**
         * @brief Creates an instance of this material.
         * @return A material instance initialized from this material.
         * @pre This material is owned by a Ref.
         */
        Ref<MaterialInstance> CreateInstance();

        /**
         * @brief Replaces the material graph.
         * @param graph Graph to store.
         */
        void SetGraph(MaterialGraph graph);

        /**
         * @brief Returns the material graph.
         * @return Read-only material graph.
         */
        const MaterialGraph& GetGraph() const { return m_Graph; }

        /**
         * @brief Enables or disables a material usage.
         * @param usage Usage to update.
         * @param enabled Whether the usage is supported.
         * @return `true` if the supported-usage set changed.
         */
        bool SetUsage(EMaterialUsage usage, bool enabled);

        /**
         * @brief Checks whether a material usage is supported.
         * @param usage Usage to check.
         * @return `true` if the usage is enabled.
         */
        bool SupportsUsage(EMaterialUsage usage) const;

        /**
         * @brief Updates a parameter default value.
         * @param name Parameter name.
         * @param value Compatible value to store.
         * @return `true` if the parameter exists and accepts @p value.
         */
        bool SetDefaultParameter(const std::string& name, const SMaterialParameter& value);

        /**
         * @brief Finds a parameter default value.
         * @param name Parameter name.
         * @return Default value, or null if the parameter does not exist.
         */
        const SMaterialParameter* GetDefaultParameter(const std::string& name) const;

        /**
         * @brief Adds a parameter to the material schema.
         * @param name Unique parameter name.
         * @param definition Parameter definition.
         * @return `true` if the parameter was added.
         * @pre @p name is not empty.
         * @pre `definition.DefaultValue` matches @p definition.
         */
        bool DefineParameter(
            std::string name,
            const SMaterialParameterDefinition& definition
        );

        /**
         * @brief Finds a parameter definition.
         * @param name Parameter name.
         * @return Definition, or null if the parameter does not exist.
         */
        const SMaterialParameterDefinition* FindParameter(const std::string& name) const;

        /**
         * @brief Checks whether a value matches a named parameter.
         * @param name Parameter name.
         * @param value Value to check.
         * @return `true` if the parameter exists and accepts @p value.
         */
        bool IsParameterValueCompatible(
            const std::string& name,
            const SMaterialParameter& value
        ) const;

        /**
         * @brief Validates parameter references in the material graph.
         * @param[out] error Optional destination for a validation error message.
         * @return `true` if all graph parameter references are valid.
         */
        bool ValidateGraph(std::string* error = nullptr) const;

        /** @brief Returns the material name. */
        const std::string& GetName() const { return m_Name; }

        /** @brief Returns the parameter schema. */
        const auto& GetParameters() const { return m_Parameters; }

        /** @brief Returns the bit mask of supported usages. */
        uint32_t GetUsageMask() const { return m_UsageMask; }

        /** @brief Returns the current material revision. */
        uint32_t GetRevision() const { return m_Revision; }

    private:
        /** Checks whether a value matches a parameter definition. */
        static bool IsValueCompatible(
            const SMaterialParameterDefinition& definition,
            const SMaterialParameter& value
        );

        std::string m_Name;
        MaterialGraph m_Graph;
        std::unordered_map<std::string, SMaterialParameterDefinition> m_Parameters;
        uint32_t m_UsageMask = 0;
        uint32_t m_Revision = 1;
    };

    /**
     * @brief Returns the bit mask for one material usage.
     * @param usage Material usage.
     * @return Bit mask that represents @p usage.
     */
    constexpr uint32_t GetMaterialUsageMask(const EMaterialUsage usage)
    {
        return 1u << static_cast<uint32_t>(usage);
    }
}
