#pragma once

namespace Elixir
{
    enum class ESamplerFilter : uint32_t
    {
        Nearest, Linear
    };

    enum class ESamplerMipmapMode : uint32_t
    {
        Nearest, Linear
    };

    enum class ESamplerAddressMode : uint32_t
    {
        Repeat,
        MirroredRepeat,
        ClampToEdge,
        ClampToBorder
    };

    enum class ECompareOp : uint32_t
    {
        Never,
        Less,
        Equal,
        LessOrEqual,
        Greater,
        NotEqual,
        GreaterOrEqual,
        Always
    };

    enum class ESamplerBorderColor : uint32_t
    {
        FloatTransparentBlack,
        IntTransparentBlack,
        FloatOpaqueBlack,
        IntOpaqueBlack,
        FloatOpaqueWhite,
        IntOpaqueWhite
    };

    struct SSamplerCreateInfo
    {
        ESamplerFilter MagFilter            = ESamplerFilter::Linear;
        ESamplerFilter MinFilter            = ESamplerFilter::Linear;
        ESamplerMipmapMode MipmapMode       = ESamplerMipmapMode::Linear;
        ESamplerAddressMode AddressModeU    = ESamplerAddressMode::Repeat;
        ESamplerAddressMode AddressModeV    = ESamplerAddressMode::Repeat;
        ESamplerAddressMode AddressModeW    = ESamplerAddressMode::Repeat;
        float MipLodBias                    = 0.0f;
        bool AnisotropyEnabled              = false;
        float MaxAnisotropy                 = 0.0f;
        bool CompareEnabled                 = false;
        ECompareOp CompareOp                = ECompareOp::Always;
        float MinLod                        = 0.0f;
        float MaxLod                        = 0.0f;
        ESamplerBorderColor BorderColor     = ESamplerBorderColor::FloatTransparentBlack;
        bool UnnormalizedCoordinates        = false;
    };

    class ELIXIR_API Sampler
    {
        friend class Image;
        friend class SamplerBuilder;
      public:
        virtual ~Sampler() = default;

        [[nodiscard]] virtual bool IsValid() const = 0;

        [[nodiscard]] const UUID& GetUUID() const { return m_UUID; }

        [[nodiscard]] ESamplerFilter GetMagFilter() const { return m_MagFilter; }
        [[nodiscard]] ESamplerFilter GetMinFilter() const { return m_MinFilter; }
        [[nodiscard]] ESamplerMipmapMode GetMipmapMode() const { return m_MipmapMode; }
        [[nodiscard]] ESamplerAddressMode GetAddressModeU() const { return m_AddressModeU; }
        [[nodiscard]] ESamplerAddressMode GetAddressModeV() const { return m_AddressModeV; }
        [[nodiscard]] ESamplerAddressMode GetAddressModeW() const { return m_AddressModeW; }
        [[nodiscard]] float GetMipLodBias() const { return m_MipLodBias; }
        [[nodiscard]] float GetMinLod() const { return m_MinLod; }
        [[nodiscard]] float GetMaxLod() const { return m_MaxLod; }
        [[nodiscard]] bool IsAnisotropyEnabled() const { return m_AnisotropyEnabled; }
        [[nodiscard]] float GetMaxAnisotropy() const { return m_MaxAnisotropy; }
        [[nodiscard]] bool IsCompareEnabled() const { return m_CompareEnabled; }
        [[nodiscard]] ECompareOp GetCompareOp() const { return m_CompareOp; }
        [[nodiscard]] ESamplerBorderColor GetBorderColor() const { return m_BorderColor; }
        [[nodiscard]] bool IsUnnormalizedCoordinates() const { return m_UnnormalizedCoordinates; }

        virtual bool operator==(const Sampler& other) const
		{
			return m_UUID == other.m_UUID;
		}

        static Ref<Sampler> Create(
            const GraphicsContext* context,
            const SSamplerCreateInfo& info
        );

      protected:
        explicit Sampler(const GraphicsContext* context, const SSamplerCreateInfo& info);

        virtual void Destroy() = 0;

        UUID m_UUID;

        ESamplerFilter m_MagFilter;
        ESamplerFilter m_MinFilter;

        ESamplerMipmapMode m_MipmapMode;

        ESamplerAddressMode m_AddressModeU;
        ESamplerAddressMode m_AddressModeV;
        ESamplerAddressMode m_AddressModeW;

        float m_MipLodBias;
        float m_MinLod;
        float m_MaxLod;

        bool m_AnisotropyEnabled;
        float m_MaxAnisotropy;

        bool m_CompareEnabled;
        ECompareOp m_CompareOp;

        ESamplerBorderColor m_BorderColor;

        bool m_UnnormalizedCoordinates;

        const GraphicsContext* m_GraphicsContext;
    };
}