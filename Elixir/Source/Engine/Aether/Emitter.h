#pragma once

#include <Engine/Core/UUID.h>
#include <Engine/Aether/Modules.h>
#include <Engine/Aether/ParameterStore.h>
#include <Engine/Aether/CurveStore.h>
#include <Engine/Aether/ColorCurveStore.h>

namespace Elixir::Aether
{
    class ParameterStore;

    enum class EParticleRenderMode : uint8_t
    {
        Sprite = 0,
        Ribbon = 1,
        Mesh   = 2
    };

    enum class EParticleBlendMode : uint8_t
    {
        Alpha    = 0, // straight alpha over-blend (smoke, opaque puffs)
        Additive = 1  // additive glow (fire, sparks, embers)
    };

    struct SGPUEmitter
    {
        UUID m_UUID;
        std::string Name;
        EParticleRenderMode RenderMode = EParticleRenderMode::Sprite;
        EParticleBlendMode BlendMode = EParticleBlendMode::Alpha;
        Ref<Texture2D> SpriteTexture;
        // Optional gradient/blackbody LUT: sheet luminance remaps to this ramp's
        // color. Null = disabled (sprite color used as-is).
        Ref<Texture2D> GradientTexture;
        // Optional fake-normal map (radial): lights the flat billboard as a
        // volume for self-shadowing. Null = disabled (unlit).
        Ref<Texture2D> NormalTexture;
        // Optional heat-haze: this map's normal offsets the scene-color lookup so
        // the billboard refracts what is behind it. Null = not a distortion
        // emitter (drawn in the normal pass). Set = drawn in the refraction pass.
        Ref<Texture2D> DistortionTexture;
        float DistortionStrength = 0.02f;

        float SpawnRatePerSecond = 1.0f;
        uint32_t BurstCount = 0u;
        float BurstIntervalSeconds = 0.0f;

        float GravityScale = 1.0f;

        // Flipbook (sub-UV) animation. Defaults describe a single full-texture
        // cell, i.e. animation disabled and identical to a plain sprite.
        uint32_t FlipbookCols = 1u;
        uint32_t FlipbookRows = 1u;
        uint32_t FlipbookFrames = 1u;
        bool FlipbookBlend = false;

        uint32_t ParticleOffset = 0u;
        uint32_t MaxParticles = 0u;
        uint32_t SpawnOpOffset = 0u;
        uint32_t SpawnOpCount = 0u;
        uint32_t UpdateOpOffset = 0u;
        uint32_t UpdateOpCount = 0u;

        bool operator==(const SGPUEmitter& other) const noexcept
        {
            return m_UUID == other.m_UUID;
        }

        auto GetHashParams() const
        {
            return m_UUID;
        }
    };
}

GENERATE_HASH_FUNCTION(Elixir::Aether::SGPUEmitter)

namespace Elixir::Aether
{
    class ELIXIR_API Emitter final
    {
      public:
        Emitter(const std::string& name, uint32_t maxParticles, float spawnRate);

        Emitter(Emitter&&) = default;
        Emitter& operator=(Emitter&&) = default;

        Emitter(const Emitter&) = delete;
        Emitter& operator=(const Emitter&) = delete;

        template <typename Module, typename... Args>
        Module& AddSpawnModule(Args&&... args)
        {
            auto module = CreateScope<Module>(std::forward<Args>(args)...);
            auto& ref = *module;
            m_SpawnModules.push_back(std::move(module));
            return ref;
        }

        template <typename Module, typename... Args>
        Module& AddUpdateModule(Args&&... args)
        {
            auto module = CreateScope<Module>(std::forward<Args>(args)...);
            auto& ref = *module;
            m_UpdateModules.push_back(std::move(module));
            return ref;
        }

        void SetRenderMode(const EParticleRenderMode mode) { m_RenderMode = mode; }
        void SetBlendMode(const EParticleBlendMode mode) { m_BlendMode = mode; }

        void SetBurst(uint32_t count, float intervalSeconds);

        void SetFlipbook(uint32_t cols, uint32_t rows, uint32_t frames, bool blend)
        {
            m_FlipbookCols = cols;
            m_FlipbookRows = rows;
            m_FlipbookFrames = frames;
            m_FlipbookBlend = blend;
        }

        SGPUEmitter Build(
            const ParameterStore& paramStore,
            const std::vector<SGPUParameter>& params,
            std::vector<SGPUParticleOp>& ops
        ) const;

        const std::string& GetName() const { return m_Name; }
        uint32_t GetMaxParticles() const { return m_MaxParticles; }

        const Ref<Texture2D>& GetSpriteTexture() const { return m_SpriteTexture; }
        void SetSpriteTexture(const Ref<Texture2D>& texture) { m_SpriteTexture = texture; }

        const Ref<Texture2D>& GetGradientTexture() const { return m_GradientTexture; }
        void SetGradientTexture(const Ref<Texture2D>& texture) { m_GradientTexture = texture; }

        const Ref<Texture2D>& GetNormalTexture() const { return m_NormalTexture; }
        void SetNormalTexture(const Ref<Texture2D>& texture) { m_NormalTexture = texture; }

        const Ref<Texture2D>& GetDistortionTexture() const { return m_DistortionTexture; }
        void SetDistortionTexture(const Ref<Texture2D>& texture) { m_DistortionTexture = texture; }
        void SetDistortionStrength(const float strength) { m_DistortionStrength = strength; }

        uint32_t GetBurstCount() const { return m_BurstCount; }
        float GetBurstIntervalSeconds() const { return m_BurstIntervalSeconds; }

        ParameterStore& GetParameters() { return m_Parameters; }
        const ParameterStore& GetParameters() const { return m_Parameters; }
        CurveStore& GetCurves() { return m_Curves; }
        const CurveStore& GetCurves() const { return m_Curves; }
        ColorCurveStore& GetColorCurves() { return m_ColorCurves; }
        const ColorCurveStore& GetColorCurves() const { return m_ColorCurves; }

        const std::string& GetSpawnRateParamName() const { return m_SpawnRateParamName; }
        void SetSpawnRateParamName(const std::string& paramName) { m_SpawnRateParamName = paramName; }

      private:
        UUID m_UUID;
        std::string m_Name;
        EParticleRenderMode m_RenderMode = EParticleRenderMode::Sprite;
        EParticleBlendMode m_BlendMode = EParticleBlendMode::Alpha;
        Ref<Texture2D> m_SpriteTexture;
        Ref<Texture2D> m_GradientTexture;
        Ref<Texture2D> m_NormalTexture;
        Ref<Texture2D> m_DistortionTexture;
        float m_DistortionStrength = 0.02f;
        uint32_t m_MaxParticles;

        std::vector<Scope<ParticleSpawnModule>> m_SpawnModules;
        std::vector<Scope<ParticleUpdateModule>> m_UpdateModules;

        std::string m_SpawnRateParamName;
        float m_SpawnRate = 0.0f; // per seconds
        uint32_t m_BurstCount = 0u;
        float m_BurstIntervalSeconds = 0.0f;

        uint32_t m_FlipbookCols = 1u;
        uint32_t m_FlipbookRows = 1u;
        uint32_t m_FlipbookFrames = 1u;
        bool m_FlipbookBlend = false;

        ParameterStore m_Parameters;
        CurveStore m_Curves;
        ColorCurveStore m_ColorCurves;
    };
}
