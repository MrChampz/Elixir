#include "epch.h"
#include "MaterialNode.h"

#include <Engine/Materials/Material.h>
#include <Engine/Materials/Compilation/MaterialGraphEmitter.h>
#include <Engine/Materials/MaterialFunction.h>
#include <Engine/Materials/Compilation/TextureSamplingOptions.h>

namespace Elixir::Materials
{
    namespace
    {
        const SMaterialExpression s_ZeroExpression{
            .Code = "0.0",
            .ValueType = EMaterialValueType::Float
        };
    }

    const SMaterialExpression& MaterialEmitContext::Input(const uint32_t slot) const
    {
        return slot < m_Inputs.size() ? m_Inputs[slot] : s_ZeroExpression;
    }

    SMaterialExpression MaterialEmitContext::FunctionInput(
        const std::string& name,
        const EMaterialValueType type
    ) const
    {
        if (!m_Bindings)
            return { "0.0", type };

        const auto found = m_Bindings->FunctionInputs.find(name);
        if (found == m_Bindings->FunctionInputs.end())
            return { "0.0", type };

        return { Widen(found->second, type), type };
    }

    std::string MaterialEmitContext::ValueParameter(const std::string& name) const
    {
        if (m_Bindings && m_Bindings->Values.contains(name))
            return m_Bindings->Values.at(name);

        return "mat." + name;
    }

    std::string MaterialEmitContext::TextureParameter(const std::string& name) const
    {
        if (m_Bindings && m_Bindings->Textures.contains(name))
            return m_Bindings->Textures.at(name);

        return "mat." + name + ".x";
    }

    std::string MaterialEmitContext::Widen(
        const SMaterialExpression& expression,
        const EMaterialValueType type
    )
    {
        if (expression.ValueType == type) return expression.Code;

        if (expression.ValueType == EMaterialValueType::Float)
        {
            const char* swizzle = type == EMaterialValueType::Float2
                ? ".xx"
                : type == EMaterialValueType::Float3 ? ".xxx" : ".xxxx";
            return "(" + expression.Code + ")" + swizzle;
        }

        if (expression.ValueType == EMaterialValueType::Float2 &&
            type == EMaterialValueType::Float3)
            return "float3(" + expression.Code + ", 0.0)";

        if (expression.ValueType == EMaterialValueType::Float2 &&
            type == EMaterialValueType::Float4)
            return "float4(" + expression.Code + ", 0.0, 0.0)";

        if (expression.ValueType == EMaterialValueType::Float3 &&
            type == EMaterialValueType::Float4)
            return "float4(" + expression.Code + ", 1.0)";

        if (type == EMaterialValueType::Float)  return "(" + expression.Code + ").x";
        if (type == EMaterialValueType::Float2) return "(" + expression.Code + ").xy";
        if (type == EMaterialValueType::Float3) return "(" + expression.Code + ").xyz";

        return expression.Code;
    }

    const char* MaterialEmitContext::TypeName(const EMaterialValueType type)
    {
        switch (type)
        {
            case EMaterialValueType::Float:  return "float";
            case EMaterialValueType::Float2: return "float2";
            case EMaterialValueType::Float3: return "float3";
            case EMaterialValueType::Float4: return "float4";
        }

        return "float4";
    }

    int MaterialEmitContext::Components(const EMaterialValueType type)
    {
        switch (type)
        {
            case EMaterialValueType::Float:  return 1;
            case EMaterialValueType::Float2: return 2;
            case EMaterialValueType::Float3: return 3;
            case EMaterialValueType::Float4: return 4;
        }

        return 4;
    }

    EMaterialValueType MaterialEmitContext::Wider(
        const EMaterialValueType left,
        const EMaterialValueType right
    )
    {
        return Components(left) >= Components(right) ? left : right;
    }

    EShaderStage MaterialEmitContext::GetStage() const
    {
        return m_Environment.Stage;
    }

    std::string MaterialEmitContext::Require(
        const std::string& expression,
        const std::string_view name
    ) const
    {
        if (!expression.empty())
            return expression;

        if (m_Diagnostics.empty())
            m_Diagnostics = "Shader input is unavailable: " + std::string(name);

        return "0.0";
    }

    std::string MaterialEmitContext::WorldPosition() const
    {
        return Require(m_Environment.WorldPosition, "world position");
    }

    std::string MaterialEmitContext::TexCoord(const uint32_t index) const
    {
        if (index < m_Environment.TexCoords.size())
            return Require(m_Environment.TexCoords[index], "texture coordinates");
        return Require({}, "texture coordinate channel");
    }

    std::string MaterialEmitContext::Time() const
    {
        return Require(m_Environment.Time, "time");
    }

    std::string MaterialEmitContext::Color() const
    {
        return Require(m_Environment.Color, "color");
    }

    std::string MaterialEmitContext::Normal() const
    {
        return Require(m_Environment.Normal, "normal");
    }

    std::string MaterialEmitContext::ViewDirection() const
    {
        return Require(m_Environment.ViewDirection, "view direction");
    }

    SMaterialExpression MaterialEmitContext::DefaultInput(
        const SMaterialNodeInput& input
    ) const
    {
        if (input.DefaultSource == EMaterialInputDefaultSource::TexCoord)
            return { TexCoord(), EMaterialValueType::Float2 };

        if (input.DefaultSource == EMaterialInputDefaultSource::TexCoord1)
            return { TexCoord(1), EMaterialValueType::Float2 };

        return { input.DefaultExpression, input.DefaultValueType };
    }

    std::string MaterialEmitContext::SampleTexture(
        const std::string& name,
        const SMaterialExpression& uv,
        const SMaterialExpression& mip,
        const STextureSamplingOptions& options
    ) const
    {
        const auto textures = Require(m_Environment.TextureArray, "texture array");

        const auto texture = textures + "[" + TextureParameter(name) + "]";
        const auto uvExp = Widen(uv, EMaterialValueType::Float2);
        const auto mipExp = Widen(mip, EMaterialValueType::Float);

        const size_t samplerIndex = (options.Filter == ETextureFilter::Point ? 2 : 0) +
                                    (options.Address == ETextureAddress::Clamp ? 1 : 0);

        const auto sampler = Require(
            m_Environment.TextureSamplers[samplerIndex],
            "sampler"
        );

        if (options.MipMode == ETextureMipMode::Bias)
        {
            if (GetStage() != EShaderStage::Pixel)
            {
                if (m_Diagnostics.empty())
                    m_Diagnostics = "Texture mip bias requires Pixel stage.";

                return "float4(0, 0, 0, 0)";
            }

            return texture + ".SampleBias(" + sampler + ", " + uvExp + ", " + mipExp + ")";
        }

        if (GetStage() != EShaderStage::Pixel ||
            options.MipMode == ETextureMipMode::Level)
        {
            const auto level = options.MipMode == ETextureMipMode::Auto
                ? "0.0"
                : mipExp;
            return texture + ".SampleLevel(" + sampler + ", " + uvExp + ", " + level + ")";
        }

        return texture + ".Sample(" + sampler + ", " + uvExp + ")";
    }

    SMaterialExpression MaterialEmitContext::CallFunction(
        const MaterialFunction& function
    ) const
    {
        auto bindings = m_Bindings ? *m_Bindings : SMaterialGraphBindings{};
        bindings.FunctionInputs.clear();

        const auto& inputs = function.GetInputs();
        for (uint32_t slot = 0; slot < inputs.size(); ++slot)
            bindings.FunctionInputs.emplace(inputs[slot].Name, Input(slot));

        Compilation::MaterialGraphEmitter emitter(function.m_Graph, m_Environment, bindings);
        const auto expression = emitter.GenerateExpression(function.m_Output);

        if (m_Diagnostics.empty())
            m_Diagnostics = emitter.GetDiagnostics();

        return expression;
    }

    bool MaterialEmitContext::IsValidStage(const EShaderStage stage)
    {
        switch (stage)
        {
            case EShaderStage::Vertex:
            case EShaderStage::Hull:
            case EShaderStage::Domain:
            case EShaderStage::Geometry:
            case EShaderStage::Pixel:
            case EShaderStage::Compute:
                return true;
            default:
                return false;
        }
    }

    MaterialEmitContext::MaterialEmitContext(
        const std::vector<SMaterialExpression>& inputs,
        const SMaterialGraphBindings* bindings,
        const SMaterialEmissionEnvironment& environment,
        std::string& diagnostics
    ) : m_Inputs(inputs),
        m_Bindings(bindings),
        m_Environment(environment),
        m_Diagnostics(diagnostics) {}
}
