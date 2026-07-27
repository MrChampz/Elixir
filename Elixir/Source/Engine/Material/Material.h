#pragma once

#include <Engine/Material/MaterialGraph.h>
#include <Engine/Graphics/Texture.h>

namespace Elixir
{
    // A renderer-specific shader permutation supported by a Surface material.
    // It does not change the material domain or graph outputs.
    enum class EMaterialUsage : uint8_t
    {
        ParticleSprite = 0,
        ParticleRibbon,
        ParticleMesh
    };

    enum class EMaterialParameterKind : uint8_t
    {
        Value, Texture
    };

    enum class EMaterialParameterType : uint8_t
    {
        Scalar, Vector, Texture
    };

    // A single named material parameter value. A parameter is one of a scalar, a
    // vector, or a texture; the active kind is given by Type.
    struct SMaterialParam
    {
        EMaterialParameterType Type = EMaterialParameterType::Scalar;
        float Scalar = 0.0f;
        glm::vec4 Vector{ 0.0f };
        Ref<Texture> Texture;

        static SMaterialParam MakeScalar(const float value)
        {
            SMaterialParam param;
            param.Type = EMaterialParameterType::Scalar;
            param.Scalar = value;
            return param;
        }

        static SMaterialParam MakeVector(const glm::vec4& value)
        {
            SMaterialParam param;
            param.Type = EMaterialParameterType::Vector;
            param.Vector = value;
            return param;
        }

        static SMaterialParam MakeTexture(const Ref<Elixir::Texture>& texture)
        {
            SMaterialParam param;
            param.Type = EMaterialParameterType::Texture;
            param.Texture = texture;
            return param;
        }
    };

    struct SMaterialParameterDefinition
    {
        EMaterialParameterKind Kind = EMaterialParameterKind::Value;
        EMaterialGraphValueType ValueType = EMaterialGraphValueType::Float4;
        SMaterialParam DefaultValue;
    };

    // A material template: a named set of parameters with default values (the schema
    // shared by all of its instances). The shading itself is provided by the renderer's shader;
    // a Material describes the parameters that feed it.
    class ELIXIR_API Material
    {
    public:
        explicit Material(std::string name) : m_Name(std::move(name)) {}

        void SetGraph(MaterialGraph graph);
        const MaterialGraph& GetGraph() const { return m_Graph; }

        bool SetUsage(EMaterialUsage usage, bool enabled);
        bool SupportsUsage(EMaterialUsage usage) const;

        bool SetDefaultParam(const std::string& name, const SMaterialParam& value);
        const SMaterialParam* GetDefaultParam(const std::string& name) const;

        bool DefineParameter(
            std::string name,
            const SMaterialParameterDefinition& definition
        );

        const SMaterialParameterDefinition* FindParameter(const std::string& name) const;

        bool IsParameterValueCompatible(
            const std::string& name,
            const SMaterialParam& value
        );
        bool ValidateGraph(std::string* error = nullptr) const;

        const std::string& GetName() const { return m_Name; }
        const auto& GetParameters() const { return m_Parameters; }
        uint32_t GetUsageMask() const { return m_UsageMask; }
        uint32_t GetRevision() const { return m_Revision; }

    private:
        static bool IsValueCompatible(
            const SMaterialParameterDefinition& definition,
            const SMaterialParam& value
        );

        std::string m_Name;
        MaterialGraph m_Graph;
        std::unordered_map<std::string, SMaterialParameterDefinition> m_Parameters;
        uint32_t m_UsageMask = 0;
        uint32_t m_Revision = 1;
    };

    constexpr uint32_t GetMaterialUsageMask(const EMaterialUsage usage)
    {
        return 1u << static_cast<uint32_t>(usage);
    }
}