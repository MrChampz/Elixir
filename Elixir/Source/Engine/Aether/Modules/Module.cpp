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
        const EOperandInput input,
        std::span<const uint32_t> words,
        const std::string_view parameter
    )
    {
        if (!m_Valid)
            return { static_cast<EOperandInput>(UINT8_MAX), 0 };

        const auto index = FindParameter(parameter);
        if (index != UINT32_MAX)
        {
            const auto declared = m_Parameters[index].Type;

            if (!IsParameterOperandType(declared, input))
            {
                EE_CORE_ERROR("Particle parameter type does not match its binding.");
                m_Valid = false;
                return { static_cast<EOperandInput>(UINT8_MAX), 0 };
            }

            const auto& definition = m_Parameters[index];
            if (input == EOperandInput::UInt && declared == EParameterType::Float &&
                !IsValidUIntScalar(definition.Value.x))
            {
                EE_CORE_ERROR("Unsigned particle bindings require an integer in [0, 2^32).");
                m_Valid = false;
                return { static_cast<EOperandInput>(UINT8_MAX), 0 };
            }
            const auto count = GetParameterWordCount(definition.Type);

            if (std::size_t(definition.WordOffset) + count > OperandIndexMask)
            {
                EE_CORE_ERROR("Particle parameter offset exceeds the operand word range.");
                m_Valid = false;
                return { static_cast<EOperandInput>(UINT8_MAX), 0 };
            }

            return {
                input,
                EncodeOperandReference(
                    EOperandSource::Parameter,
                    GetParameterOperandFormat(declared),
                    definition.WordOffset
                )
            };
        }

        const auto format = GetOperandTypeFormat(input);

        std::array<uint32_t, 5> key{ uint32_t(input) };
        std::ranges::copy(words, key.begin() + 1);

        if (const auto found = m_Literals.find(key); found != m_Literals.end())
            return {
                input,
                EncodeOperandReference(EOperandSource::Constant, format, found->second)
            };

        if (m_ModuleData.Constants.size() + words.size() > OperandIndexMask)
        {
            EE_CORE_ERROR("Particle constant pool exceeds 26 bits.");
            m_Valid = false;
            return { static_cast<EOperandInput>(UINT8_MAX), 0 };
        }

        const auto offset = (uint32_t)m_ModuleData.Constants.size();
        m_ModuleData.Constants.insert(m_ModuleData.Constants.end(), words.begin(), words.end());
        m_Literals.emplace(std::move(key), offset);

        return {
            input,
            EncodeOperandReference(EOperandSource::Constant, format, offset)
        };
    }

    SGPUOperand ModuleCompileContext::Scalar(const float value, const std::string_view parameter)
    {
        const uint32_t words[]{ std::bit_cast<uint32_t>(value) };
        return Literal(EOperandInput::Float, words, parameter);
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
        return Literal(EOperandInput::Float3, words, parameter);
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
        return Literal(EOperandInput::Float4, words, parameter);
    }

    SGPUOperand ModuleCompileContext::UInt(uint32_t value, const std::string_view parameter)
    {
        if (!parameter.empty())
            return Literal(EOperandInput::UInt, std::span(&value, 1), parameter);

        if (value <= OperandIndexMask)
            return {
                EOperandInput::UInt,
                EncodeOperandReference(EOperandSource::Immediate, EOperandFormat::UInt, value)
            };

        return Literal(EOperandInput::UInt, std::span(&value, 1), "");
    }

    SGPUOperand ModuleCompileContext::Dynamic(EDynamicInput input)
    {
        if (input == EDynamicInput::None) return Scalar(1.0f);
        return {
            EOperandInput::Float,
            EncodeOperandReference(
                EOperandSource::DynamicInput,
                EOperandFormat::Float,
                uint32_t(input)
            )
        };
    }

    SGPUOperand ModuleCompileContext::Attribute(
        const EParticleAttribute attribute,
        const EOperandInput type
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
            m_Valid = false;
            return { static_cast<EOperandInput>(UINT8_MAX), 0 };
        }

        return {
            EOperandInput::Curve,
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
        if (!m_Valid) return;

        const auto* operation = FindOperation(type);
        if (!operation)
        {
            EE_CORE_ERROR("Unknown particle operation.");
            m_Valid = false;
            return;
        }

        const auto inputs = operation->GetDefinition().Inputs;
        using enum EOperandInput;

        if (operands.size() > UINT32_MAX)
        {
            EE_CORE_ERROR("Particle operation exceeds the 32-bit argument-count range.");
            m_Valid = false;
            return;
        }

        if (inputs.size() != operands.size())
        {
            EE_CORE_ERROR("Particle operation argument count does not match its definition.");
            m_Valid = false;
            return;
        }

        if (!operation->IsValidTarget(target))
        {
            EE_CORE_ERROR("Invalid destination for the particle operation.");
            m_Valid = false;
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

            const auto isNumeric = actual == Float || actual == Float3 || actual == Float4;
            const auto isValidTempTarget = isTempTarget && isNumeric;
            const auto matchesTarget = actual == targetType || isValidTempTarget;
            const bool matches = expected == actual || (expected == Target && matchesTarget);

            if (!matches)
            {
                EE_CORE_ERROR("Particle operation argument type does not match its definition.");
                m_Valid = false;
                return;
            }

            if (!IsValidOperand(operand, validationContext))
            {
                EE_CORE_ERROR("Particle operand reference does not match its source.");
                m_Valid = false;
                return;
            }
        }

        if (m_ModuleData.Operands.size() + operands.size() > UINT32_MAX)
        {
            EE_CORE_ERROR("Particle operand table exceeds uint32 range.");
            m_Valid = false;
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

    bool Module::SetTarget(const EModuleTarget target)
    {
        if (!SupportsTarget(target) || (m_Phase != EModulePhase::Update && target != EModuleTarget::Particle))
        {
            EE_CORE_ERROR("Module does not support the requested execution target.");
            return false;
        }
        m_Target = target;
        return true;
    }
}
