#include "epch.h"
#include "Emitter.h"

#include "System.h"

namespace Elixir::Aether
{
    Emitter::Emitter(
        const std::string& name,
        const uint32_t maxParticles,
        const float spawnRate
    ) : m_Name(name),
        m_MaxParticles(maxParticles),
        m_SpawnRate(spawnRate) {}

    void Emitter::SetBurst(const uint32_t count, const float intervalSeconds)
    {
        m_BurstCount = count;
        m_BurstIntervalSeconds = intervalSeconds;
    }

    SGPUEmitter Emitter::Build(
        const ParameterStore& paramStore,
        const std::vector<SGPUParameter>& params,
        std::vector<SGPUParticleOp>& ops
    ) const
    {
        SGPUEmitter emitter;
        emitter.m_UUID = m_UUID;
        emitter.Name = m_Name;
        emitter.RenderMode = m_RenderMode;
        emitter.BlendMode = m_BlendMode;
        emitter.SpriteTexture = m_SpriteTexture;
        emitter.GradientTexture = m_GradientTexture;
        emitter.NormalTexture = m_NormalTexture;
        emitter.DistortionTexture = m_DistortionTexture;
        emitter.DistortionStrength = m_DistortionStrength;
        emitter.MaxParticles = m_MaxParticles;
        emitter.GravityScale = paramStore.GetFloat("GravityScale", 1.0f);
        emitter.SpawnOpOffset = (uint32_t)ops.size();
        emitter.SpawnRatePerSecond = m_SpawnRate;
        emitter.BurstCount = m_BurstCount;
        emitter.BurstIntervalSeconds = m_BurstIntervalSeconds;
        emitter.FlipbookCols = m_FlipbookCols;
        emitter.FlipbookRows = m_FlipbookRows;
        emitter.FlipbookFrames = m_FlipbookFrames;
        emitter.FlipbookBlend = m_FlipbookBlend;

        const uint32_t spawnRateParamIndex =
            FindScopedParameterIndex(params, m_Name, m_SpawnRateParamName);
        if (spawnRateParamIndex != UINT32_MAX)
            emitter.SpawnRatePerSecond = params[spawnRateParamIndex].Value.x;

        for (const auto& module : m_SpawnModules)
        {
            if (const auto* typed = dynamic_cast<const SetPositionDisk*>(module.get()))
            {
                ops.push_back({
                    EParticleOp::SampleDisk,
                    EParticleAttribute::Position,
                    UINT32_MAX,
                    UINT32_MAX,
                    { typed->GetCenter(), typed->GetRadius() },
                    { typed-> GetNormal(), 0.0f }
                });
            }
            else if (const auto* typed = dynamic_cast<const SetPositionBox*>(module.get()))
            {
                ops.push_back({
                    EParticleOp::SampleBox,
                    EParticleAttribute::Position,
                    UINT32_MAX,
                    UINT32_MAX,
                    { typed->GetMinBounds(), 0.0f },
                    { typed->GetMaxBounds(), 0.0f }
                });
            }
            else if (const auto* typed = dynamic_cast<const SetVelocityCone*>(module.get()))
            {
                ops.push_back({
                    EParticleOp::SampleCone,
                    EParticleAttribute::Velocity,
                    UINT32_MAX,
                    UINT32_MAX,
                    { typed->GetDirection(), typed->GetAngle() },
                    { typed->GetMinSpeed(), typed->GetMaxSpeed(), 0.0f, 0.0f }
                });
            }
            else if (const auto* typed = dynamic_cast<const SetLifetime*>(module.get()))
            {
                ops.push_back({
                    EParticleOp::RandomRange,
                    EParticleAttribute::Lifetime,
                    FindScopedParameterIndex(params, m_Name, typed->GetMinSecondsParamName()),
                    FindScopedParameterIndex(params, m_Name, typed->GetMaxSecondsParamName()),
                    { typed->GetMinSeconds(), 0.0f, 0.0f, 0.0f },
                    { typed->GetMaxSeconds(), 0.0f, 0.0f, 0.0f }
                });
            }
            else if (const auto* typed = dynamic_cast<const SetSize*>(module.get()))
            {
                ops.push_back({
                    EParticleOp::RandomRange,
                    EParticleAttribute::Size,
                    FindScopedParameterIndex(params, m_Name, typed->GetMinSizeParamName()),
                    FindScopedParameterIndex(params, m_Name, typed->GetMaxSizeParamName()),
                    { typed->GetMinSize(), 0.0f, 0.0f, 0.0f },
                    { typed->GetMaxSize(), 0.0f, 0.0f, 0.0f }
                });
            }
            else if (const auto* typed = dynamic_cast<const SetColor*>(module.get()))
            {
                ops.push_back({
                    EParticleOp::SetLiteral,
                    EParticleAttribute::Color,
                    FindScopedParameterIndex(params, m_Name, typed->GetParamName()),
                    UINT32_MAX,
                    typed->GetColor(),
                });
            }
            else if (const auto* typed = dynamic_cast<const SetRotation*>(module.get()))
            {
                ops.push_back({
                    EParticleOp::RandomRange,
                    EParticleAttribute::Rotation,
                    FindScopedParameterIndex(params, m_Name, typed->GetMinRotationParamName()),
                    FindScopedParameterIndex(params, m_Name, typed->GetMaxRotationParamName()),
                    { typed->GetMinRotation(), 0.0f, 0.0f, 0.0f },
                    { typed->GetMaxRotation(), 0.0f, 0.0f, 0.0f }
                });
            }
            else if (const auto* typed = dynamic_cast<const SetScale*>(module.get()))
            {
                ops.push_back({
                    EParticleOp::RandomRange,
                    EParticleAttribute::Scale,
                    FindScopedParameterIndex(params, m_Name, typed->GetMinScaleParamName()),
                    FindScopedParameterIndex(params, m_Name, typed->GetMaxScaleParamName()),
                    { typed->GetMinScale(), 0.0f, 0.0f, 0.0f },
                    { typed->GetMaxScale(), 0.0f, 0.0f, 0.0f }
                });
            }
            else if (const auto* typed = dynamic_cast<const SetPositionOnCircle*>(module.get()))
            {
                ops.push_back({
                    EParticleOp::SetPositionOnCircle,
                    EParticleAttribute::Position,
                    UINT32_MAX,
                    UINT32_MAX,
                    { typed->GetCenter(), typed->GetRadius() },
                    { typed->GetAngularSpeed(), typed->GetStartAngle(), 0.0, 0.0 }
                });
            }
            else if (const auto* typed = dynamic_cast<const SetPositionCircularPath*>(module.get()))
            {
                ops.push_back({
                    EParticleOp::SetPositionCircularPath,
                    EParticleAttribute::Position,
                    UINT32_MAX,
                    UINT32_MAX,
                    { typed->GetBaseOffset(), typed->GetTimeScale() },
                    { typed->GetPrimaryAmplitude(), 0.0 },
                    { typed->GetSecondaryAmplitude(), 0.0 }
                });
            }
            else if (const auto* typed = dynamic_cast<const SetPositionVortexRibbonPath*>(module.get()))
            {
                ops.push_back({
                    EParticleOp::SetPositionVortexRibbonPath,
                    EParticleAttribute::Position,
                    UINT32_MAX,
                    UINT32_MAX,
                    { typed->GetCenter(), 0.0 },
                    {
                        typed->GetOrbitSpeed(),
                        typed->GetBaseRadius(),
                        typed->GetRadiusAmplitude(),
                        typed->GetRadiusSpeed()
                    },
                    {
                        typed->GetPulseAmplitude(),
                        typed->GetPulseSpeed(),
                        typed->GetCurlAmplitude(),
                        typed->GetDepthAmplitude()
                    }
                });
            }
            else if (const auto* typed = dynamic_cast<const SetRibbonId*>(module.get()))
            {
                ops.push_back({
                    EParticleOp::SetLiteral,
                    EParticleAttribute::RibbonId,
                    UINT32_MAX,
                    UINT32_MAX,
                    { (float)typed->GetRibbonId(), 0.0f, 0.0f, 0.0f }
                });
            }
            else if (const auto* typed = dynamic_cast<const SetRibbonIdFromSpawnOrder*>(module.get()))
            {
                ops.push_back({
                    EParticleOp::SetRibbonIdFromSpawnOrder,
                    EParticleAttribute::RibbonId,
                    UINT32_MAX,
                    UINT32_MAX,
                    {
                        (float)typed->GetRibbonCount(),
                        (float)typed->GetFirstRibbonId(),
                        0.0f, 0.0f
                    }
                });
            }
        }

        emitter.SpawnOpCount = (uint32_t)ops.size() - emitter.SpawnOpOffset;
        emitter.UpdateOpOffset = (uint32_t)ops.size();

        for (const auto& module : m_UpdateModules)
        {
            if (const auto* typed = dynamic_cast<const ApplyGravity*>(module.get()))
            {
                ops.push_back({
                    EParticleOp::AddWithDelta,
                    EParticleAttribute::Velocity,
                    FindScopedParameterIndex(params, m_Name, typed->GetParamName()),
                    UINT32_MAX,
                    { typed->GetGravity() * emitter.GravityScale, 0.0f },
                    {}
                });
            }
            else if (const auto* typed = dynamic_cast<const ApplyLinearDrag*>(module.get()))
            {
                ops.push_back({
                    EParticleOp::Dampen,
                    EParticleAttribute::Velocity,
                    FindScopedParameterIndex(params, m_Name, typed->GetParamName()),
                    UINT32_MAX,
                    { typed->GetDragPerSecond(), 0.0f, 0.0f, 0.0f },
                    {}
                });
            }
            else if (const auto* typed = dynamic_cast<const ApplyAngularVelocity*>(module.get()))
            {
                ops.push_back({
                    EParticleOp::AddWithDelta,
                    EParticleAttribute::Rotation,
                    FindScopedParameterIndex(params, m_Name, typed->GetParamName()),
                    UINT32_MAX,
                    { typed->GetRadiansPerSecond(), 0.0f, 0.0f, 0.0f },
                    { (float)((uint32_t)typed->GetInput()), 0.0f, 0.0f, 0.0f }
                });
            }
            else if (const auto* typed = dynamic_cast<const ApplyVortex*>(module.get()))
            {
                const uint32_t tangentialParamIndex = FindScopedParameterIndex(params, m_Name, typed->GetTangentialParamName());
                const uint32_t radialParamIndex = FindScopedParameterIndex(params, m_Name, typed->GetRadialParamName());
                ops.push_back({
                    EParticleOp::ApplyVortex,
                    EParticleAttribute::Velocity,
                    FindScopedParameterIndex(params, m_Name, typed->GetCenterParamName()),
                    FindScopedParameterIndex(params, m_Name, typed->GetNormalParamName()),
                    { typed->GetCenter(), 0.0f },
                    { typed->GetNormal(), 0.0f },
                    {
                        typed->GetTangentialStrength(),
                        typed->GetRadialStrength(),
                        (float)(tangentialParamIndex == UINT32_MAX ? -1 : (int32_t)tangentialParamIndex),
                        (float)(radialParamIndex == UINT32_MAX ? -1 : (int32_t)radialParamIndex),

                    }
                });
            }
            else if (const auto* typed = dynamic_cast<const ColorOverLife*>(module.get()))
            {
                if (!typed->GetCurveName().empty())
                {
                    ops.push_back({
                        EParticleOp::SampleColorCurve,
                        EParticleAttribute::Temp0,
                        FindCurveParameterIndex(params, m_Name, typed->GetCurveName()),
                        UINT32_MAX,
                        { (float)(uint32_t)typed->GetCurveInput(), 0.0f, 0.0f, 0.0f },
                    });
                    ops.push_back({
                        EParticleOp::CopyFromAttribute,
                        EParticleAttribute::Color,
                        UINT32_MAX,
                        UINT32_MAX,
                        { (float)(uint32_t)EParticleAttribute::Temp0, 0.0f, 0.0f, 0.0f },
                    });
                }
                else
                {
                    ops.push_back({
                        EParticleOp::LerpOverLife,
                        EParticleAttribute::Color,
                        FindScopedParameterIndex(params, m_Name, typed->GetStartColorParamName()),
                        FindScopedParameterIndex(params, m_Name, typed->GetEndColorParamName()),
                        typed->GetStartColor(),
                        typed->GetEndColor()
                    });
                }
            }
            else if (const auto* typed = dynamic_cast<const SizeOverLife*>(module.get()))
            {
                ops.push_back({
                    EParticleOp::LerpOverLife,
                    EParticleAttribute::Size,
                    FindScopedParameterIndex(params, m_Name, typed->GetStartSizeParamName()),
                    FindScopedParameterIndex(params, m_Name, typed->GetEndSizeParamName()),
                    { typed->GetStartSize(), 0.0f, 0.0f, 0.0f },
                    { typed->GetEndSize(), 0.0f, 0.0f, 0.0f }
                });
            }
            else if (const auto* typed = dynamic_cast<const ScaleOverLife*>(module.get()))
            {
                if (!typed->GetCurveName().empty())
                {
                    ops.push_back({
                        EParticleOp::SampleCurve,
                        EParticleAttribute::Temp0,
                        FindCurveParameterIndex(params, m_Name, typed->GetCurveName()),
                        UINT32_MAX,
                        { (float)(uint32_t)typed->GetCurveInput(), 0.0f, 0.0f, 0.0f },
                    });

                    const float scaleRange = typed->GetEndScale() - typed->GetStartScale();
                    if (std::abs(scaleRange - 1.0f) > 0.0001f)
                    {
                        ops.push_back({
                            EParticleOp::Mul,
                            EParticleAttribute::Temp0,
                            UINT32_MAX,
                            UINT32_MAX,
                            { scaleRange, 0.0f, 0.0f, 0.0f },
                        });
                    }

                    if (std::abs(typed->GetStartScale()) > 0.0001f)
                    {
                        ops.push_back({
                            EParticleOp::Add,
                            EParticleAttribute::Temp0,
                            UINT32_MAX,
                            UINT32_MAX,
                            { typed->GetStartScale(), 0.0f, 0.0f, 0.0f },
                        });
                    }

                    ops.push_back({
                        EParticleOp::Clamp,
                        EParticleAttribute::Temp0,
                        UINT32_MAX,
                        UINT32_MAX,
                        glm::vec4(0.0f),
                        glm::vec4(4.0f),
                    });

                    ops.push_back({
                        EParticleOp::CopyFromAttribute,
                        EParticleAttribute::Scale,
                        UINT32_MAX,
                        UINT32_MAX,
                        { (float)(uint32_t)EParticleAttribute::Temp0, 0.0f, 0.0f, 0.0f },
                    });
                }
                else
                {
                    ops.push_back({
                        EParticleOp::LerpOverLife,
                        EParticleAttribute::Scale,
                        FindScopedParameterIndex(params, m_Name, typed->GetStartScaleParamName()),
                        FindScopedParameterIndex(params, m_Name, typed->GetEndScaleParamName()),
                        { typed->GetStartScale(), 0.0f, 0.0f, 0.0f },
                        { typed->GetEndScale(), 0.0f, 0.0f, 0.0f }
                    });
                }
            }
            else if (const auto* typed = dynamic_cast<const KillOutsideBounds*>(module.get()))
            {
                ops.push_back({
                    EParticleOp::KillOutsideBounds,
                    EParticleAttribute::Position,
                    UINT32_MAX,
                    UINT32_MAX,
                    { typed->GetMin(), 0.0f },
                    { typed->GetMax(), 0.0f }
                });
            }
        }

        emitter.UpdateOpCount = (uint32_t)ops.size() - emitter.UpdateOpOffset;

        return emitter;
    }
}
