#include "epch.h"
#include "Module.h"
#include <bit>
#include <stdexcept>
#include <algorithm>

namespace Elixir::Aether::Modules
{
    /* ModuleCompileContext */

    ModuleCompileContext::ModuleCompileContext(
        SParticleProgram& program,
        const std::vector<Core::SGPUParameter>& parameters,
        const std::string_view emitterName,
        const float gravityScale
    ) : m_Program(program),
        m_Parameters(parameters),
        m_EmitterName(emitterName),
        m_GravityScale(gravityScale) {}

    uint32_t ModuleCompileContext::FindParameter(const std::string_view name) const
    {
        if (name.empty()) return UINT32_MAX;
        return Core::FindScopedParameterIndex(
            m_Parameters,
            std::string(m_EmitterName),
            std::string(name)
        );
    }

    uint32_t ModuleCompileContext::FindCurve(const std::string_view name) const
    {
        if (name.empty()) return UINT32_MAX;
        return Core::FindCurveParameterIndex(
            m_Parameters,
            std::string(m_EmitterName),
            std::string(name)
        );
    }

    namespace
    {
        // Packs a reference without silently truncating its index.
        uint32_t Encode(EOperandSource source, uint32_t index)
        {
            if (index > OperandIndexMask) throw std::length_error("Particle operand index exceeds 29 bits.");
            return (uint32_t(source) << 29) | index;
        }

        // Returns the width implied by a particle destination.
        EOperandType TargetType(Core::EParticleAttribute target)
        {
            switch (target)
            {
                case Core::EParticleAttribute::Position:
                case Core::EParticleAttribute::Velocity:
                case Core::EParticleAttribute::Tangent: return EOperandType::Vector3;
                case Core::EParticleAttribute::Color:
                case Core::EParticleAttribute::None: return EOperandType::Vector4;
                default: return EOperandType::Scalar;
            }
        }
    }

    SModuleOperand ModuleCompileContext::Literal(EOperandType type, std::span<const uint32_t> words, std::string_view parameter)
    {
        const auto index = FindParameter(parameter);
        if (index != UINT32_MAX)
        {
            const auto declared = m_Parameters[index].Type;
            if ((declared == Core::EParameterType::Scalar && type != EOperandType::Scalar) ||
                (declared == Core::EParameterType::Vector && type == EOperandType::Scalar))
                throw std::invalid_argument("Particle parameter type does not match its binding.");
            return { Encode(EOperandSource::Parameter, index), type };
        }
        const auto source = type == EOperandType::Vector3 ? EOperandSource::ConstantVector3 :
            type == EOperandType::Vector4 ? EOperandSource::ConstantVector4 : EOperandSource::Constant;
        std::array<uint32_t, 5> key{ uint32_t(type) };
        std::ranges::copy(words, key.begin() + 1);
        if (const auto found = m_Program.m_Literals.find(key); found != m_Program.m_Literals.end())
            return { Encode(source, found->second), type };
        if (m_Program.Constants.size() + words.size() > OperandIndexMask)
            throw std::length_error("Particle constant pool exceeds 29 bits.");
        const auto offset = uint32_t(m_Program.Constants.size());
        m_Program.Constants.insert(m_Program.Constants.end(), words.begin(), words.end());
        m_Program.m_Literals.emplace(std::move(key), offset);
        return { Encode(source, offset), type };
    }

    SModuleOperand ModuleCompileContext::Scalar(float value, std::string_view parameter)
    {
        const uint32_t words[]{ std::bit_cast<uint32_t>(value) };
        return Literal(EOperandType::Scalar, words, parameter);
    }
    SModuleOperand ModuleCompileContext::Vector(glm::vec3 value, std::string_view parameter)
    {
        const uint32_t words[]{ std::bit_cast<uint32_t>(value.x), std::bit_cast<uint32_t>(value.y), std::bit_cast<uint32_t>(value.z) };
        return Literal(EOperandType::Vector3, words, parameter);
    }
    SModuleOperand ModuleCompileContext::Value(glm::vec4 value, std::string_view parameter)
    {
        const uint32_t words[]{ std::bit_cast<uint32_t>(value.x), std::bit_cast<uint32_t>(value.y), std::bit_cast<uint32_t>(value.z), std::bit_cast<uint32_t>(value.w) };
        return Literal(EOperandType::Vector4, words, parameter);
    }
    SModuleOperand ModuleCompileContext::UInt(uint32_t value)
    {
        if (value <= OperandIndexMask) return { Encode(EOperandSource::ImmediateUInt, value), EOperandType::UInt };
        return Literal(EOperandType::UInt, std::span(&value, 1), "");
    }
    SModuleOperand ModuleCompileContext::Dynamic(EDynamicInput input)
    {
        return { Encode(EOperandSource::DynamicInput, uint32_t(input)), EOperandType::Scalar };
    }
    SModuleOperand ModuleCompileContext::Attribute(Core::EParticleAttribute attribute, EOperandType type)
    {
        return { Encode(EOperandSource::ParticleAttribute, uint32_t(attribute)), type };
    }
    SModuleOperand ModuleCompileContext::Curve(std::string_view name)
    {
        const auto index = FindCurve(name);
        if (index != UINT32_MAX && index >= OperandIndexMask)
            throw std::length_error("Particle curve index collides with the missing-curve sentinel.");
        return { Encode(EOperandSource::Curve, index == UINT32_MAX ? OperandIndexMask : index), EOperandType::Curve };
    }

    void ModuleCompileContext::Emit(Core::EParticleOp type, Core::EParticleAttribute target, std::initializer_list<SModuleOperand> operands)
    {
        std::span<const EOperandType> contract;
        using enum EOperandType;
        switch (type)
        {
#define AETHER_OPERATION(Name, Opcode, ...) case Core::EParticleOp::Name: { static constexpr EOperandType types[]{ __VA_ARGS__ }; contract = types; break; }
#define AETHER_ARGUMENT(Op, Name, Index)
#include <Engine/Aether/Core/ParticleOperations.def>
#undef AETHER_ARGUMENT
#undef AETHER_OPERATION
            default: throw std::invalid_argument("Unknown particle operation.");
        }
        if (contract.size() != operands.size()) throw std::invalid_argument("Particle operation argument count does not match its contract.");
        if (target > Core::EParticleAttribute::Temp3)
            throw std::invalid_argument("Unknown particle operation target.");
        const auto targetType = type == Core::EParticleOp::SetLiteral && target == Core::EParticleAttribute::RibbonId
            ? UInt : TargetType(target);
        const bool temporaryTarget = target >= Core::EParticleAttribute::Temp0 && target <= Core::EParticleAttribute::Temp3;
        size_t i = 0;
        for (const auto& operand : operands)
        {
            const auto expected = contract[i++];
            const auto actual = operand.Type;
            const bool matches = expected == actual ||
                (expected == Target && (actual == targetType ||
                    (temporaryTarget && (actual == Scalar || actual == Vector3 || actual == Vector4))));
            if (!matches) throw std::invalid_argument("Particle operation argument type does not match its contract.");
            const auto source = EOperandSource(operand.Reference >> 29);
            const auto index = operand.Reference & OperandIndexMask;
            bool valid = false;
            switch (source)
            {
                case EOperandSource::Constant:
                    valid = (actual == Scalar || actual == UInt) && index < m_Program.Constants.size(); break;
                case EOperandSource::ConstantVector3:
                    valid = actual == Vector3 && size_t(index) + 3 <= m_Program.Constants.size(); break;
                case EOperandSource::ConstantVector4:
                    valid = actual == Vector4 && size_t(index) + 4 <= m_Program.Constants.size(); break;
                case EOperandSource::Parameter:
                    valid = (actual == Scalar || actual == Vector3 || actual == Vector4) && index < m_Parameters.size();
                    if (valid)
                    {
                        const auto declared = m_Parameters[index].Type;
                        valid = declared == Core::EParameterType::Unspecified ||
                            (declared == Core::EParameterType::Scalar ? actual == Scalar : actual != Scalar);
                    }
                    break;
                case EOperandSource::DynamicInput:
                    valid = actual == Scalar && index <= uint32_t(EDynamicInput::ParticleSeed); break;
                case EOperandSource::ParticleAttribute:
                    valid = index > uint32_t(Core::EParticleAttribute::None) && index <= uint32_t(Core::EParticleAttribute::Temp3) &&
                        (actual == TargetType(Core::EParticleAttribute(index)) ||
                            (index >= uint32_t(Core::EParticleAttribute::Temp0) && (actual == Scalar || actual == Vector3 || actual == Vector4))); break;
                case EOperandSource::ImmediateUInt:
                    valid = actual == UInt; break;
                case EOperandSource::Curve:
                    valid = actual == Curve && (index == OperandIndexMask || index < m_Parameters.size()); break;
            }
            if (!valid) throw std::invalid_argument("Particle operand reference does not match its source.");
        }
        if (m_Program.Operands.size() + operands.size() > UINT32_MAX) throw std::length_error("Particle operand table exceeds uint32 range.");
        const auto offset = uint32_t(m_Program.Operands.size());
        for (const auto& operand : operands) m_Program.Operands.push_back(operand.Reference);
        m_Program.Ops.push_back({ type, target, offset, uint32_t(operands.size()) });
    }

    /* Module */

    Module::Module(const EModulePhase phase) : m_Phase(phase) {}

    Module::~Module() = default;
}
