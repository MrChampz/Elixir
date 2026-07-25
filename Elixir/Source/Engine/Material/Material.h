#pragma once

#include <Engine/Graphics/Texture.h>

namespace Elixir
{
    enum class EMaterialParamType : uint8_t
    {
        Scalar, Vector, Texture
    };

    // A single named material parameter value. A parameter is one of a scalar, a
    // vector, or a texture; the active kind is given by Type.
    struct SMaterialParam
    {
        EMaterialParamType Type = EMaterialParamType::Scalar;
        float Scalar = 0.0f;
        glm::vec4 Vector{ 0.0f };
        Ref<Texture> Texture;

        static SMaterialParam MakeScalar(const float value)
        {
            SMaterialParam param;
            param.Type = EMaterialParamType::Scalar;
            param.Scalar = value;
            return param;
        }

        static SMaterialParam MakeVector(const glm::vec4& value)
        {
            SMaterialParam param;
            param.Type = EMaterialParamType::Vector;
            param.Vector = value;
            return param;
        }

        static SMaterialParam MakeTexture(const Ref<Elixir::Texture>& texture)
        {
            SMaterialParam param;
            param.Type = EMaterialParamType::Texture;
            param.Texture = texture;
            return param;
        }
    };

    // A material template: a named set of parameters with default values (the schema
    // shared by all of its instances). The shading itself is provided by the renderer's shader;
    // a Material describes the parameters that feed it.
    class ELIXIR_API Material
    {
    public:
        explicit Material(std::string name) : m_Name(std::move(name)) {}

        void SetDefaultParam(const std::string& name, const SMaterialParam& value);

        const SMaterialParam* GetDefaultParam(const std::string& name) const;

        const std::string& GetName() const { return m_Name; }
        const std::unordered_map<std::string, SMaterialParam>& GetDefaultParams() const { return m_DefaultParams; }

    private:
        std::string m_Name;
        std::unordered_map<std::string, SMaterialParam> m_DefaultParams;
    };
}