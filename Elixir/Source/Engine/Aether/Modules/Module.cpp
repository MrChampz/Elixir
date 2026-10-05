#include "epch.h"
#include "Module.h"

#include <Engine/Aether/Core/CurveStore.h>

#include <bit>
#include <stdexcept>
#include <algorithm>

namespace Elixir::Aether::Modules
{
    /* ModuleCompileContext */

    ModuleCompileContext::ModuleCompileContext(
        Core::SCompiledOperations& program,
        const std::vector<Core::SParameter>& parameters,
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
        uint32_t Encode(Core::EOperandSource source, uint32_t index)
        {
            if (index > Core::OperandIndexMask) throw std::length_error("Particle operand index exceeds 29 bits.");
            return (uint32_t(source) << 29) | index;
        }

        // Returns the width implied by a particle destination.
        Core::EOperandType TargetType(Core::EParticleAttribute target)
        {
            switch (target)
            {
                case Core::EParticleAttribute::Position:
                case Core::EParticleAttribute::Velocity:
                case Core::EParticleAttribute::Tangent: return Core::EOperandType::Vector3;
                case Core::EParticleAttribute::Color:
                case Core::EParticleAttribute::None: return Core::EOperandType::Vector4;
                default: return Core::EOperandType::Scalar;
            }
        }
    }

    Core::SOperand ModuleCompileContext::Literal(Core::EOperandType type, std::span<const uint32_t> words, std::string_view parameter)
    {
        const auto index = FindParameter(parameter);
        if (index != UINT32_MAX)
        {
            const auto declared = m_Parameters[index].Type;
            if ((declared == Core::EParameterType::Scalar && type != Core::EOperandType::Scalar) ||
                (declared == Core::EParameterType::Vector && type == Core::EOperandType::Scalar))
                throw std::invalid_argument("Particle parameter type does not match its binding.");
            return { type, Encode(Core::EOperandSource::Parameter, index) };
        }
        const auto source = type == Core::EOperandType::Vector3 ? Core::EOperandSource::ConstantVector3 :
            type == Core::EOperandType::Vector4 ? Core::EOperandSource::ConstantVector4 : Core::EOperandSource::Constant;
        std::array<uint32_t, 5> key{ uint32_t(type) };
        std::ranges::copy(words, key.begin() + 1);
        if (const auto found = m_Literals.find(key); found != m_Literals.end())
            return { type, Encode(source, found->second) };
        if (m_Program.Constants.size() + words.size() > Core::OperandIndexMask)
            throw std::length_error("Particle constant pool exceeds 29 bits.");
        const auto offset = uint32_t(m_Program.Constants.size());
        m_Program.Constants.insert(m_Program.Constants.end(), words.begin(), words.end());
        m_Literals.emplace(std::move(key), offset);
        return { type, Encode(source, offset) };
    }

    Core::SOperand ModuleCompileContext::Scalar(float value, std::string_view parameter)
    {
        const uint32_t words[]{ std::bit_cast<uint32_t>(value) };
        return Literal(Core::EOperandType::Scalar, words, parameter);
    }
    Core::SOperand ModuleCompileContext::Vector(glm::vec3 value, std::string_view parameter)
    {
        const uint32_t words[]{ std::bit_cast<uint32_t>(value.x), std::bit_cast<uint32_t>(value.y), std::bit_cast<uint32_t>(value.z) };
        return Literal(Core::EOperandType::Vector3, words, parameter);
    }
    Core::SOperand ModuleCompileContext::Value(glm::vec4 value, std::string_view parameter)
    {
        const uint32_t words[]{ std::bit_cast<uint32_t>(value.x), std::bit_cast<uint32_t>(value.y), std::bit_cast<uint32_t>(value.z), std::bit_cast<uint32_t>(value.w) };
        return Literal(Core::EOperandType::Vector4, words, parameter);
    }
    Core::SOperand ModuleCompileContext::UInt(uint32_t value)
    {
        if (value <= Core::OperandIndexMask) return { Core::EOperandType::UInt, Encode(Core::EOperandSource::ImmediateUInt, value) };
        return Literal(Core::EOperandType::UInt, std::span(&value, 1), "");
    }
    Core::SOperand ModuleCompileContext::Dynamic(EDynamicInput input)
    {
        if (input == EDynamicInput::None) return Scalar(1.0f);
        return { Core::EOperandType::Scalar, Encode(Core::EOperandSource::DynamicInput, uint32_t(input)) };
    }
    Core::SOperand ModuleCompileContext::Attribute(Core::EParticleAttribute attribute, Core::EOperandType type)
    {
        return { type, Encode(Core::EOperandSource::ParticleAttribute, uint32_t(attribute)) };
    }
    Core::SOperand ModuleCompileContext::Curve(std::string_view name)
    {
        const auto index = FindCurve(name);
        if (index != UINT32_MAX && index >= Core::OperandIndexMask)
            throw std::length_error("Particle curve index collides with the missing-curve sentinel.");
        return { Core::EOperandType::Curve, Encode(Core::EOperandSource::Curve, index == UINT32_MAX ? Core::OperandIndexMask : index) };
    }

    void ModuleCompileContext::Emit(Core::EParticleOp type, Core::EParticleAttribute target, std::initializer_list<Core::SOperand> operands)
    {
        std::span<const Core::EOperandType> contract;
        using enum Core::EOperandType;
        switch (type)
        {
            case Core::EParticleOp::SetLiteral:
            {
                static constexpr Core::EOperandType types[]{ Target };
                contract = types;
                break;
            }
            case Core::EParticleOp::RandomRange:
            {
                static constexpr Core::EOperandType types[]{ Target, Target };
                contract = types;
                break;
            }
            case Core::EParticleOp::SampleDisk:
            {
                static constexpr Core::EOperandType types[]{ Vector3, Scalar, Vector3 };
                contract = types;
                break;
            }
            case Core::EParticleOp::SampleCone:
            {
                static constexpr Core::EOperandType types[]{ Vector3, Scalar, Scalar, Scalar };
                contract = types;
                break;
            }
            case Core::EParticleOp::SampleBox:
            {
                static constexpr Core::EOperandType types[]{ Vector3, Vector3 };
                contract = types;
                break;
            }
            case Core::EParticleOp::AddWithDelta:
            {
                static constexpr Core::EOperandType types[]{ Target, Scalar };
                contract = types;
                break;
            }
            case Core::EParticleOp::Dampen:
            {
                static constexpr Core::EOperandType types[]{ Scalar };
                contract = types;
                break;
            }
            case Core::EParticleOp::LerpOverLife:
            {
                static constexpr Core::EOperandType types[]{ Target, Target };
                contract = types;
                break;
            }
            case Core::EParticleOp::KillOutsideBounds:
            {
                static constexpr Core::EOperandType types[]{ Vector3, Vector3 };
                contract = types;
                break;
            }
            case Core::EParticleOp::AddFromAttribute:
            {
                static constexpr Core::EOperandType types[]{ Target, Scalar };
                contract = types;
                break;
            }
            case Core::EParticleOp::SetPositionOnCircle:
            {
                static constexpr Core::EOperandType types[]{ Vector3, Scalar, Scalar, Scalar };
                contract = types;
                break;
            }
            case Core::EParticleOp::SetPositionCircularPath:
            {
                static constexpr Core::EOperandType types[]{ Vector3, Scalar, Vector3, Vector3 };
                contract = types;
                break;
            }
            case Core::EParticleOp::SetPositionVortexRibbonPath:
            {
                static constexpr Core::EOperandType types[]{ Vector3, Scalar, Scalar, Scalar, Scalar, Scalar, Scalar, Scalar, Scalar };
                contract = types;
                break;
            }
            case Core::EParticleOp::SetRibbonIdFromSpawnOrder:
            {
                static constexpr Core::EOperandType types[]{ UInt, UInt };
                contract = types;
                break;
            }
            case Core::EParticleOp::SampleCurve:
            {
                static constexpr Core::EOperandType types[]{ Curve, Scalar };
                contract = types;
                break;
            }
            case Core::EParticleOp::SampleColorCurve:
            {
                static constexpr Core::EOperandType types[]{ Curve, Scalar };
                contract = types;
                break;
            }
            case Core::EParticleOp::Add:
            {
                static constexpr Core::EOperandType types[]{ Target };
                contract = types;
                break;
            }
            case Core::EParticleOp::Mul:
            {
                static constexpr Core::EOperandType types[]{ Target };
                contract = types;
                break;
            }
            case Core::EParticleOp::Clamp:
            {
                static constexpr Core::EOperandType types[]{ Target, Target };
                contract = types;
                break;
            }
            case Core::EParticleOp::CopyFromAttribute:
            {
                static constexpr Core::EOperandType types[]{ Target };
                contract = types;
                break;
            }
            case Core::EParticleOp::ApplyVortex:
            {
                static constexpr Core::EOperandType types[]{ Vector3, Vector3, Scalar, Scalar };
                contract = types;
                break;
            }
            default: throw std::invalid_argument("Unknown particle operation.");
        }
        if (operands.size() > UINT32_MAX) throw std::length_error("Particle operation exceeds the 32-bit argument-count range.");
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
            const auto source = Core::EOperandSource(operand.Reference >> 29);
            const auto index = operand.Reference & Core::OperandIndexMask;
            bool valid = false;
            switch (source)
            {
                case Core::EOperandSource::Constant:
                    valid = (actual == Scalar || actual == UInt) && index < m_Program.Constants.size(); break;
                case Core::EOperandSource::ConstantVector3:
                    valid = actual == Vector3 && size_t(index) + 3 <= m_Program.Constants.size(); break;
                case Core::EOperandSource::ConstantVector4:
                    valid = actual == Vector4 && size_t(index) + 4 <= m_Program.Constants.size(); break;
                case Core::EOperandSource::Parameter:
                    valid = (actual == Scalar || actual == Vector3 || actual == Vector4) && index < m_Parameters.size();
                    if (valid)
                    {
                        const auto declared = m_Parameters[index].Type;
                        valid = declared == Core::EParameterType::Unspecified ||
                            (declared == Core::EParameterType::Scalar ? actual == Scalar : actual != Scalar);
                    }
                    break;
                case Core::EOperandSource::DynamicInput:
                    valid = actual == Scalar && index <= uint32_t(EDynamicInput::ParticleSeed); break;
                case Core::EOperandSource::ParticleAttribute:
                    valid = index > uint32_t(Core::EParticleAttribute::None) && index <= uint32_t(Core::EParticleAttribute::Temp3) &&
                        (actual == TargetType(Core::EParticleAttribute(index)) ||
                            (index >= uint32_t(Core::EParticleAttribute::Temp0) && (actual == Scalar || actual == Vector3 || actual == Vector4))); break;
                case Core::EOperandSource::ImmediateUInt:
                    valid = actual == UInt; break;
                case Core::EOperandSource::Curve:
                    valid = actual == Curve && (index == Core::OperandIndexMask || index < m_Parameters.size()); break;
            }
            if (!valid) throw std::invalid_argument("Particle operand reference does not match its source.");
        }
        if (m_Program.Operands.size() + operands.size() > UINT32_MAX) throw std::length_error("Particle operand table exceeds uint32 range.");
        const auto offset = uint32_t(m_Program.Operands.size());
        for (const auto& operand : operands)
            m_Program.Operands.push_back(operand.Reference);
        m_Program.Ops.push_back({ type, target, offset, uint32_t(operands.size()) });
    }

    /* Module */

    Module::Module(const EModulePhase phase) : m_Phase(phase) {}

    Module::~Module() = default;
}
