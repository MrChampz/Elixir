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

    enum class ESamplerBorderColor : uint32_t
    {
        FloatTransparentBlack,
        IntTransparentBlack,
        FloatOpaqueBlack,
        IntOpaqueBlack,
        FloatOpaqueWhite,
        IntOpaqueWhite
    };

    class ELIXIR_API Sampler
    {
        friend class Image;
        friend class SamplerBuilder;
      public:
        virtual ~Sampler() = default;

        virtual void Destroy() = 0;

        [[nodiscard]] virtual bool IsDestroyed() const = 0;

        [[nodiscard]] const UUID& GetUUID() const { return m_UUID; }

        [[nodiscard]] ESamplerFilter GetMagFilter() const { return m_MagFilter; }
        void SetMagFilter(const ESamplerFilter filter) { m_MagFilter = filter; }

        [[nodiscard]] ESamplerFilter GetMinFilter() const { return m_MinFilter; }
        void SetMinFilter(const ESamplerFilter filter) { m_MinFilter = filter; }

        [[nodiscard]] ESamplerMipmapMode GetMipmapMode() const { return m_MipmapMode; }
        void SetMipmapMode(const ESamplerMipmapMode mode) { m_MipmapMode = mode; }

        [[nodiscard]] ESamplerAddressMode GetAddressModeU() const { return m_AddressModeU; }
        void SetAddressModeU(const ESamplerAddressMode mode) { m_AddressModeU = mode; }

        [[nodiscard]] ESamplerAddressMode GetAddressModeV() const { return m_AddressModeV; }
        void SetAddressModeV(const ESamplerAddressMode mode) { m_AddressModeV = mode; }

        [[nodiscard]] ESamplerAddressMode GetAddressModeW() const { return m_AddressModeW; }
        void SetAddressModeW(const ESamplerAddressMode mode) { m_AddressModeW = mode; }

        [[nodiscard]] float GetMipLodBias() const { return m_MipLodBias; }
        void SetMipLodBias(const float bias) { m_MipLodBias = bias; }

        [[nodiscard]] float GetMinLod() const { return m_MinLod; }
        void SetMinLod(const float lod) { m_MinLod = lod; }

        [[nodiscard]] float GetMaxLod() const { return m_MaxLod; }
        void SetMaxLod(const float lod) { m_MaxLod = lod; }

        [[nodiscard]] bool IsAnisotropyEnabled() const { return m_AnisotropyEnabled; }
        void SetAnisotropyEnabled(const bool enabled) { m_AnisotropyEnabled = enabled; }

        [[nodiscard]] float GetMaxAnisotropy() const { return m_MaxAnisotropy; }
        void SetMaxAnisotropy(const float anisotropy) { m_MaxAnisotropy = anisotropy; }

        [[nodiscard]] ESamplerBorderColor GetBorderColor() const { return m_BorderColor; }
        void SetBorderColor(const ESamplerBorderColor color) { m_BorderColor = color; }

        virtual bool operator==(const Sampler& other) const
		{
			return m_UUID == other.m_UUID;
		}

        static Ref<Sampler> Create(const GraphicsContext* context);

      protected:
        explicit Sampler(const GraphicsContext* context);

        virtual void UpdateSampler() = 0;

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

        ESamplerBorderColor m_BorderColor;

        const GraphicsContext* m_GraphicsContext;
    };
}