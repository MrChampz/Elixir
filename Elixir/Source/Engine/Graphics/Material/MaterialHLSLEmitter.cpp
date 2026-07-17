#include "epch.h"
#include "MaterialHLSLEmitter.h"

#include <bit>

namespace Elixir
{
    namespace
    {
        const char* TypeName(const EGraphValueType type)
        {
            switch (type)
            {
                case EGraphValueType::Float: return "float";
                case EGraphValueType::Float2: return "float2";
                case EGraphValueType::Float3: return "float3";
                case EGraphValueType::Float4: return "float4";
                case EGraphValueType::Texture2D: return "uint";
            }
            return "float4";
        }

        const char* ChannelName(const EMaterialChannel channel)
        {
            switch (channel)
            {
                case EMaterialChannel::BaseColor: return "BaseColor";
                case EMaterialChannel::Metallic: return "Metallic";
                case EMaterialChannel::Roughness: return "Roughness";
                case EMaterialChannel::Emissive: return "Emissive";
                case EMaterialChannel::Normal: return "Normal";
                case EMaterialChannel::WorldPositionOffset: return "WorldPositionOffset";
                case EMaterialChannel::Opacity: return "Opacity";
                case EMaterialChannel::SubsurfaceColor: return "SubsurfaceColor";
                case EMaterialChannel::ClearCoat: return "ClearCoat";
                case EMaterialChannel::ClearCoatRoughness: return "ClearCoatRoughness";
                case EMaterialChannel::Sheen: return "Sheen";
            }
            return "BaseColor";
        }

        std::string Num(const float value)
        {
            return std::to_string(value);
        }

        std::string DefaultUV(const EMaterialIRStage stage)
        {
            return stage == EMaterialIRStage::Vertex ? "uv" : "input.TexCoord";
        }

        std::string TextureIndexFromSlot(const int32_t slot)
        {
            return "(uint)(int)GraphParams[" + std::to_string(slot) + "].x";
        }

        int Components(const EGraphValueType type)
        {
            switch (type)
            {
                case EGraphValueType::Float: return 1;
                case EGraphValueType::Float2: return 2;
                case EGraphValueType::Float3: return 3;
                case EGraphValueType::Float4: return 4;
                case EGraphValueType::Texture2D: return 0;
            }
            return 4;
        }

        EGraphValueType WiderType(const EGraphValueType a, const EGraphValueType b)
        {
            return Components(a) >= Components(b) ? a : b;
        }

        std::string Expression(const SMaterialIR& material, const SMaterialIRValue& value)
        {
            if (!value.IsInstruction())
                return value.InlineExpression.empty() ? "0.0" : value.InlineExpression;
            if (value.Instruction >= material.Instructions.size())
                return "0.0";
            return "n" + std::to_string(material.Instructions[value.Instruction].SourceNode);
        }

        std::string Widen(const std::string& expression,
            const EGraphValueType from, const EGraphValueType to)
        {
            if (from == to)
                return expression;
            if (from == EGraphValueType::Texture2D || to == EGraphValueType::Texture2D)
                return to == EGraphValueType::Texture2D ? "0xffffffffu" : "0.0";
            if (from == EGraphValueType::Float)
            {
                const char* swizzle = to == EGraphValueType::Float2 ? ".xx"
                    : to == EGraphValueType::Float3 ? ".xxx" : ".xxxx";
                return "(" + expression + ")" + swizzle;
            }
            if (from == EGraphValueType::Float2 && to == EGraphValueType::Float3)
                return "float3(" + expression + ", 0.0)";
            if (from == EGraphValueType::Float2 && to == EGraphValueType::Float4)
                return "float4(" + expression + ", 0.0, 0.0)";
            if (from == EGraphValueType::Float3 && to == EGraphValueType::Float4)
                return "float4(" + expression + ", 1.0)";
            if (to == EGraphValueType::Float)
                return "(" + expression + ").x";
            if (to == EGraphValueType::Float2)
                return "(" + expression + ").xy";
            if (to == EGraphValueType::Float3)
                return "(" + expression + ").xyz";
            return expression;
        }

        std::string Coerce(const std::string& expression,
            const EGraphValueType from, const EMaterialChannel channel)
        {
            if (from == EGraphValueType::Texture2D)
                return "float3(0.0, 0.0, 0.0)";
            const bool scalar = channel == EMaterialChannel::Metallic
                || channel == EMaterialChannel::Roughness
                || channel == EMaterialChannel::Opacity
                || channel == EMaterialChannel::ClearCoat
                || channel == EMaterialChannel::ClearCoatRoughness
                || channel == EMaterialChannel::Sheen;
            if (scalar)
                return from == EGraphValueType::Float ? expression : "(" + expression + ").x";
            switch (from)
            {
                case EGraphValueType::Float: return expression + ".xxx";
                case EGraphValueType::Float2: return "float3(" + expression + ", 0.0)";
                case EGraphValueType::Float3: return expression;
                case EGraphValueType::Float4: return "(" + expression + ").rgb";
                case EGraphValueType::Texture2D: break;
            }
            return expression;
        }

        std::string ConstantExpr(const SMaterialIRInstruction& instruction)
        {
            const glm::vec4& value = instruction.ConstantValue;
            switch (instruction.Type)
            {
                case EGraphValueType::Float: return Num(value.x);
                case EGraphValueType::Float2:
                    return "float2(" + Num(value.x) + ", " + Num(value.y) + ")";
                case EGraphValueType::Float3:
                    return "float3(" + Num(value.x) + ", " + Num(value.y)
                        + ", " + Num(value.z) + ")";
                case EGraphValueType::Float4:
                    return "float4(" + Num(value.x) + ", " + Num(value.y)
                        + ", " + Num(value.z) + ", " + Num(value.w) + ")";
                case EGraphValueType::Texture2D: return "0xffffffffu";
            }
            return "0.0";
        }

        std::string EmitInstruction(
            const SMaterialIR& material, const SMaterialIRInstruction& instruction)
        {
            const auto input = [&](const size_t index)
            {
                return index < instruction.Inputs.size()
                    ? Expression(material, instruction.Inputs[index]) : std::string("0.0");
            };
            const auto inputType = [&](const size_t index)
            {
                return index < instruction.Inputs.size()
                    ? instruction.Inputs[index].Type : EGraphValueType::Float;
            };
            const auto connected = [&](const size_t index)
            {
                return index < instruction.InputConnected.size()
                    && instruction.InputConnected[index];
            };
            const auto binary = [&](const char* op)
            {
                return "(" + Widen(input(0), inputType(0), instruction.Type) + " " + op + " "
                    + Widen(input(1), inputType(1), instruction.Type) + ")";
            };

            switch (instruction.Op)
            {
                case EMaterialIROp::Constant: return ConstantExpr(instruction);
                case EMaterialIROp::Scalar: return Num(instruction.ConstantValue.x);
                case EMaterialIROp::Bool:
                    return instruction.ConstantValue.x >= 0.5f ? "1.0" : "0.0";
                case EMaterialIROp::GraphParameterScalar:
                    return "GraphParams[" + std::to_string(instruction.ParameterSlot) + "].x";
                case EMaterialIROp::GraphParameterColor:
                    return "GraphParams[" + std::to_string(instruction.ParameterSlot) + "]";
                case EMaterialIROp::MaterialParameter: return "mat." + instruction.Name;
                case EMaterialIROp::TextureParameter:
                {
                    const std::string index = TextureIndexFromSlot(instruction.ParameterSlot);
                    const std::string uv = !connected(0)
                            ? DefaultUV(material.Stage)
                            : Widen(input(0), inputType(0), EGraphValueType::Float2);
                    return "(" + index + " == 0xffffffffu ? float3(1.0, 1.0, 1.0) : SampleTex("
                        + index + ", " + uv + "))";
                }
                case EMaterialIROp::TextureObjectParameter:
                    return TextureIndexFromSlot(instruction.ParameterSlot);
                case EMaterialIROp::TextureObjectSample:
                {
                    const std::string index = connected(0)
                            ? input(0) : std::string("0xffffffffu");
                    const std::string uv = !connected(1)
                            ? DefaultUV(material.Stage)
                            : Widen(input(1), inputType(1), EGraphValueType::Float2);
                    const std::string mip = !connected(2)
                            ? Num(instruction.ConstantValue.x)
                            : Widen(input(2), inputType(2), EGraphValueType::Float);

                    const bool point = instruction.TextureSampleFilter == ETextureSampleFilter::Point;
                    const bool clamp = instruction.TextureSampleAddress == ETextureSampleAddress::Clamp;
                    const std::string sampler = point
                        ? (clamp ? "texSamplerPointClamp" : "texSamplerPoint")
                        : (clamp ? "texSamplerClamp" : "texSampler");

                    std::string sample;
                    if (material.Stage == EMaterialIRStage::Vertex
                        || instruction.TextureSampleMipMode == ETextureSampleMipMode::Level)
                    {
                        const std::string level = instruction.TextureSampleMipMode
                            == ETextureSampleMipMode::Auto ? std::string("0.0") : mip;
                        sample = "textures[" + index + "].SampleLevel(" + sampler
                            + ", " + uv + ", " + level + ")";
                    }
                    else if (instruction.TextureSampleMipMode == ETextureSampleMipMode::Bias)
                        sample = "textures[" + index + "].SampleBias(" + sampler
                            + ", " + uv + ", " + mip + ")";
                    else
                        sample = "textures[" + index + "].Sample(" + sampler + ", " + uv + ")";

                    const bool normal = instruction.TextureSampleType == ETextureSampleType::Normal;
                    const std::string fallback = normal
                        ? std::string("float4(0.5, 0.5, 1.0, 1.0)")
                        : std::string("float4(1.0, 1.0, 1.0, 1.0)");
                    const std::string raw = "(" + index + " == 0xffffffffu ? "
                        + fallback + " : " + sample + ")";

                    if (instruction.TextureSampleOutput == ETextureSampleOutput::RGBA)
                    {
                        std::string expression = raw;
                        if (normal)
                            expression = "(" + expression
                                + " * float4(2.0, 2.0, 2.0, 1.0)"
                                  " - float4(1.0, 1.0, 1.0, 0.0))";
                        return expression;
                    }
                    if (instruction.TextureSampleOutput == ETextureSampleOutput::RGB)
                    {
                        std::string expression = "(" + raw + ").rgb";
                        if (normal)
                            expression = "(" + expression + " * 2.0 - 1.0)";
                        return expression;
                    }
                    const char channel = instruction.TextureSampleOutput == ETextureSampleOutput::R ? 'r'
                        : instruction.TextureSampleOutput == ETextureSampleOutput::G ? 'g'
                        : instruction.TextureSampleOutput == ETextureSampleOutput::B ? 'b' : 'a';
                    std::string expression = "(" + raw + ")." + std::string(1, channel);
                    if (normal && channel != 'a')
                        expression = "(" + expression + " * 2.0 - 1.0)";
                    return expression;
                }
                case EMaterialIROp::LegacyTextureSample:
                {
                    const std::string uv = !connected(0)
                            ? DefaultUV(material.Stage)
                            : Widen(input(0), inputType(0), EGraphValueType::Float2);
                    const std::string& index = instruction.LegacyTextureExpression;
                    return "(" + index
                        + " == 0xffffffffu ? float3(1.0, 1.0, 1.0) : SampleTex("
                        + index + ", " + uv + "))";
                }
                case EMaterialIROp::TexCoord: return DefaultUV(material.Stage);
                case EMaterialIROp::Position:
                    return material.Stage == EMaterialIRStage::Vertex ? "P" : "input.WorldPos";
                case EMaterialIROp::Time: return "Time";
                case EMaterialIROp::Sine: return "sin(" + input(0) + ")";
                case EMaterialIROp::Panner:
                {
                    const std::string uv = !connected(0)
                            ? DefaultUV(material.Stage)
                            : Widen(input(0), inputType(0), EGraphValueType::Float2);
                    const std::string speed = "float2(" + Num(instruction.ConstantValue.x)
                        + ", " + Num(instruction.ConstantValue.y) + ")";
                    return "(" + uv + " + Time * " + speed + ")";
                }
                case EMaterialIROp::Checker:
                {
                    const bool hasInput = connected(0);
                    const std::string scale = Num(instruction.ConstantValue.x);
                    if (hasInput && inputType(0) == EGraphValueType::Float3)
                        return "Checker3(" + input(0) + ", " + scale + ")";
                    return "Checker(" + (hasInput
                        ? Widen(input(0), inputType(0), EGraphValueType::Float2)
                        : DefaultUV(material.Stage)) + ", " + scale + ")";
                }
                case EMaterialIROp::Noise:
                {
                    const bool hasInput = connected(0);
                    const std::string scale = Num(instruction.ConstantValue.x);
                    if (hasInput && inputType(0) == EGraphValueType::Float3)
                        return "ValueNoise3(" + input(0) + ", " + scale + ")";
                    return "ValueNoise(" + (hasInput
                        ? Widen(input(0), inputType(0), EGraphValueType::Float2)
                        : DefaultUV(material.Stage)) + ", " + scale + ")";
                }
                case EMaterialIROp::Multiply: return binary("*");
                case EMaterialIROp::Add: return binary("+");
                case EMaterialIROp::Subtract: return binary("-");
                case EMaterialIROp::Divide: return binary("/");
                case EMaterialIROp::Power:
                    return "pow(" + Widen(input(0), inputType(0), instruction.Type) + ", "
                        + Widen(input(1), inputType(1), instruction.Type) + ")";
                case EMaterialIROp::Dot:
                {
                    const EGraphValueType width = WiderType(inputType(0), inputType(1));
                    return "dot(" + Widen(input(0), inputType(0), width) + ", "
                        + Widen(input(1), inputType(1), width) + ")";
                }
                case EMaterialIROp::Lerp:
                    return "lerp(" + Widen(input(0), inputType(0), instruction.Type) + ", "
                        + Widen(input(1), inputType(1), instruction.Type) + ", "
                        + Widen(input(2), inputType(2), instruction.Type) + ")";
                case EMaterialIROp::OneMinus: return "(1.0 - " + input(0) + ")";
                case EMaterialIROp::Saturate: return "saturate(" + input(0) + ")";
                case EMaterialIROp::Fresnel:
                    return material.Stage == EMaterialIRStage::Vertex
                        ? "0.0" : "pow(saturate(1.0 - dot(N, V)), 5.0)";
                case EMaterialIROp::StaticSwitch:
                    return "(" + input(2) + " >= 0.5 ? "
                        + Widen(input(0), inputType(0), instruction.Type) + " : "
                        + Widen(input(1), inputType(1), instruction.Type) + ")";
                case EMaterialIROp::ComponentMask:
                {
                    const uint8_t mask = instruction.ComponentMask & 0x0f;
                    std::string swizzle;
                    if (mask & 0x1) swizzle += 'r';
                    if (mask & 0x2) swizzle += 'g';
                    if (mask & 0x4) swizzle += 'b';
                    if (mask & 0x8) swizzle += 'a';
                    if (std::popcount((unsigned int)mask) == 0)
                        return "0.0";
                    return "(" + Widen(input(0), inputType(0), EGraphValueType::Float4)
                        + ")." + swizzle;
                }
                case EMaterialIROp::AppendVector:
                {
                    const int components = Components(inputType(0)) + Components(inputType(1));
                    if (components < 2 || components > 4)
                        return "float4(0.0, 0.0, 0.0, 0.0)";
                    return std::string(TypeName(instruction.Type))
                        + "(" + input(0) + ", " + input(1) + ")";
                }
                case EMaterialIROp::Custom: break;
                case EMaterialIROp::FunctionFallback:
                    return "float4(0.0, 0.0, 0.0, 1.0)";
            }
            return "0.0";
        }
    }

    std::string MaterialHLSLEmitter::Emit(const SMaterialIR& material)
    {
        std::string body;
        size_t nextOutput = 0;
        const auto emitReadyOutputs = [&](const size_t instructionCount, std::string& destination)
        {
            while (nextOutput < material.Outputs.size()
                && material.Outputs[nextOutput].AfterInstructionCount == instructionCount)
            {
                const SMaterialIROutput& output = material.Outputs[nextOutput++];
                const std::string value = Expression(material, output.Value);
                if (output.Channel == EMaterialChannel::WorldPositionOffset)
                    destination += "    wpo = "
                        + Coerce(value, output.Value.Type, output.Channel) + ";\n";
                else
                    destination += "    surface." + std::string(ChannelName(output.Channel))
                        + " = " + Coerce(value, output.Value.Type, output.Channel) + ";\n";
            }
        };

        emitReadyOutputs(0, body);
        for (size_t instructionIndex = 0;
            instructionIndex < material.Instructions.size(); ++instructionIndex)
        {
            const SMaterialIRInstruction& instruction = material.Instructions[instructionIndex];
            const std::string variable = "n" + std::to_string(instruction.SourceNode);
            if (instruction.Op == EMaterialIROp::Custom)
            {
                body += "    " + std::string(TypeName(instruction.Type))
                    + " " + variable + ";\n    {\n";
                const char* names[3] = { "a", "b", "c" };
                for (size_t input = 0; input < 3; ++input)
                {
                    const EGraphValueType type = input < instruction.Inputs.size()
                        ? instruction.Inputs[input].Type : EGraphValueType::Float;
                    const std::string expression = input < instruction.Inputs.size()
                        ? Expression(material, instruction.Inputs[input]) : std::string("0.0");
                    body += "        " + std::string(TypeName(type)) + " "
                        + names[input] + " = " + expression + ";\n";
                }
                const std::string code = instruction.CustomCode.empty()
                    ? "a" : instruction.CustomCode;
                body += "        " + variable + " = (" + code + ");\n    }\n";
            }
            else
                body += "    " + std::string(TypeName(instruction.Type)) + " "
                    + variable + " = " + EmitInstruction(material, instruction) + ";\n";
            emitReadyOutputs(instructionIndex + 1, body);
        }

        if (material.Stage == EMaterialIRStage::Pixel
            && material.BlendMode == EMaterialBlendMode::Masked)
            body += "    clip(surface.Opacity - " + Num(material.AlphaCutoff) + ");\n";
        return body;
    }
}
