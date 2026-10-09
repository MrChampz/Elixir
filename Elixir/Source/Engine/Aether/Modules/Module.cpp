#include "epch.h"
#include "Module.h"

#include <Engine/Aether/Core/CurveStore.h>
#include <Engine/Aether/Core/OperationRegistry.h>

#include <bit>
#include <algorithm>

namespace Elixir::Aether::Modules
{
    /* ModuleCompileContext */

    ModuleCompileContext::ModuleCompileContext(
        SCompiledModuleData& operations,
        const std::vector<SCompiledParameter>& parameters,
        const std::string_view emitterName,
        const float gravityScale
    ) : m_ModuleData(operations),
        m_Parameters(parameters),
        m_EmitterName(emitterName),
        m_GravityScale(gravityScale) {}

    uint32_t ModuleCompileContext::FindParameter(const std::string_view name) const
    {
        if (name.empty()) return UINT32_MAX;
        return FindScopedParameterIndex(
            m_Parameters,
            std::string(m_EmitterName),
            std::string(name)
        );
    }

    uint32_t ModuleCompileContext::FindCurve(const std::string_view name) const
    {
        if (name.empty()) return UINT32_MAX;
        return FindCurveParameterIndex(
            m_Parameters,
            std::string(m_EmitterName),
            std::string(name)
        );
    }

    SGPUOperand ModuleCompileContext::Literal(
        EOperandType type,
        std::span<const uint32_t> words,
        const std::string_view parameter
    )
    {
        const auto index = FindParameter(parameter);
        if (index != UINT32_MAX)
        {
            const auto declared = m_Parameters[index].Type;

            if (!IsParameterOperandType(declared, type))
            {
                EE_CORE_ERROR("Particle parameter type does not match its binding.");
                return { static_cast<EOperandType>(UINT8_MAX), 0 };
            }

            const auto& definition = m_Parameters[index];
            const auto count = GetParameterWordCount(definition.Type);

            if (std::size_t(definition.WordOffset) + count > OperandIndexMask)
            {
                EE_CORE_ERROR("Particle parameter offset exceeds the operand word range.");
                return { static_cast<EOperandType>(UINT8_MAX), 0 };
            }

            return {
                type,
                EncodeOperandReference(
                    EOperandSource::Parameter,
                    GetParameterOperandFormat(declared),
                    definition.WordOffset
                )
            };
        }

        const auto format = GetOperandTypeFormat(type);

        std::array<uint32_t, 5> key{ uint32_t(type) };
        std::ranges::copy(words, key.begin() + 1);

        if (const auto found = m_Literals.find(key); found != m_Literals.end())
            return {
                type,
                EncodeOperandReference(EOperandSource::Constant, format, found->second)
            };

        if (m_ModuleData.Constants.size() + words.size() > OperandIndexMask)
        {
            EE_CORE_ERROR("Particle constant pool exceeds 26 bits.");
            return { static_cast<EOperandType>(UINT8_MAX), 0 };
        }

        const auto offset = (uint32_t)m_ModuleData.Constants.size();
        m_ModuleData.Constants.insert(m_ModuleData.Constants.end(), words.begin(), words.end());
        m_Literals.emplace(std::move(key), offset);

        return {
            type,
            EncodeOperandReference(EOperandSource::Constant, format, offset)
        };
    }

    SGPUOperand ModuleCompileContext::Scalar(const float value, const std::string_view parameter)
    {
        const uint32_t words[]{ std::bit_cast<uint32_t>(value) };
        return Literal(EOperandType::Scalar, words, parameter);
    }

    SGPUOperand ModuleCompileContext::Vector(
        const glm::vec3 value,
        const std::string_view parameter
    )
    {
        const uint32_t words[]
        {
            std::bit_cast<uint32_t>(value.x),
            std::bit_cast<uint32_t>(value.y),
            std::bit_cast<uint32_t>(value.z)
        };
        return Literal(EOperandType::Vector3, words, parameter);
    }

    SGPUOperand ModuleCompileContext::Value(
        const glm::vec4 value,
        const std::string_view parameter
    )
    {
        const uint32_t words[]
        {
            std::bit_cast<uint32_t>(value.x),
            std::bit_cast<uint32_t>(value.y),
            std::bit_cast<uint32_t>(value.z),
            std::bit_cast<uint32_t>(value.w)
        };
        return Literal(EOperandType::Vector4, words, parameter);
    }

    SGPUOperand ModuleCompileContext::UInt(uint32_t value, const std::string_view parameter)
    {
        if (!parameter.empty())
            return Literal(EOperandType::UInt, std::span(&value, 1), parameter);

        if (value <= OperandIndexMask)
            return {
                EOperandType::UInt,
                EncodeOperandReference(EOperandSource::Immediate, EOperandFormat::UInt, value)
            };

        return Literal(EOperandType::UInt, std::span(&value, 1), "");
    }

    SGPUOperand ModuleCompileContext::Dynamic(EDynamicInput input)
    {
        if (input == EDynamicInput::None) return Scalar(1.0f);
        return {
            EOperandType::Scalar,
            EncodeOperandReference(
                EOperandSource::DynamicInput,
                EOperandFormat::Scalar,
                uint32_t(input)
            )
        };
    }

    SGPUOperand ModuleCompileContext::Attribute(
        const EParticleAttribute attribute,
        const EOperandType type
    )
    {
        return {
            type,
            EncodeOperandReference(
                EOperandSource::ParticleAttribute,
                GetAttributeOperandFormat(attribute),
                uint32_t(attribute)
            )
        };
    }

    SGPUOperand ModuleCompileContext::Curve(const std::string_view name)
    {
        const auto parameter = FindCurve(name);
        const auto index = parameter < m_Parameters.size()
            ? m_Parameters[parameter].WordOffset
            : UINT32_MAX;

        if (index != UINT32_MAX && index >= OperandIndexMask)
        {
            EE_CORE_ERROR("Particle curve index collides with the missing-curve sentinel.");
            return { static_cast<EOperandType>(UINT8_MAX), 0 };
        }

        return {
            EOperandType::Curve,
            EncodeOperandReference(
                EOperandSource::Parameter,
                EOperandFormat::Curve,
                index == UINT32_MAX ? OperandIndexMask : index
            )
        };
    }

    void ModuleCompileContext::Emit(
        const EOperationType type,
        const EParticleAttribute target,
        const std::initializer_list<SGPUOperand> operands
    )
    {
        const auto* operation = FindOperation(type);
        if (!operation)
        {
            EE_CORE_ERROR("Unknown particle operation.");
            return;
        }

        const auto inputs = operation->GetDefinition().Inputs;
        using enum EOperandType;

        if (operands.size() > UINT32_MAX)
        {
            EE_CORE_ERROR("Particle operation exceeds the 32-bit argument-count range.");
            return;
        }

        if (inputs.size() != operands.size())
        {
            EE_CORE_ERROR("Particle operation argument count does not match its definition.");
            return;
        }

        if (!operation->IsValidTarget(target))
        {
            EE_CORE_ERROR("Invalid destination for the particle operation.");
            return;
        }

        const auto targetType = GetAttributeOperandType(target);
        const bool isTempTarget = target >= EParticleAttribute::Temp0 &&
            target <= EParticleAttribute::Temp3;

        const SOperandValidationContext validationContext{
            m_ModuleData.Constants,
            m_Parameters
        };

        size_t i = 0;
        for (const auto& operand : operands)
        {
            const auto expected = inputs[i++];
            const auto actual = operand.Type;

            const auto isNumeric = actual == Scalar || actual == Vector3 || actual == Vector4;
            const auto isValidTempTarget = isTempTarget && isNumeric;
            const auto matchesTarget = actual == targetType || isValidTempTarget;
            const bool matches = expected == actual || (expected == Target && matchesTarget);

            if (!matches)
            {
                EE_CORE_ERROR("Particle operation argument type does not match its definition.");
                return;
            }

            if (!IsValidOperand(operand, validationContext))
            {
                EE_CORE_ERROR("Particle operand reference does not match its source.");
                return;
            }
        }

        if (m_ModuleData.Operands.size() + operands.size() > UINT32_MAX)
        {
            EE_CORE_ERROR("Particle operand table exceeds uint32 range.");
            return;
        }

        const auto offset = uint32_t(m_ModuleData.Operands.size());

        for (const auto& operand : operands)
            m_ModuleData.Operands.push_back(operand.Reference);

        m_ModuleData.Operations.push_back({
            type,
            target,
            offset,
            uint32_t(operands.size())
        });
    }

    /* Module */

    Module::Module(const EModulePhase phase) : m_Phase(phase) {}

    Module::~Module() = default;
}
