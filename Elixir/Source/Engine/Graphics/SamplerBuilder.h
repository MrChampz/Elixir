#pragma once

namespace Elixir
{
    class ELIXIR_API SamplerBuilder
    {
      public:
        SamplerBuilder();

        virtual Ref<Sampler> Build(const GraphicsContext* context);
        virtual SamplerBuilder& Clear();

        virtual SamplerBuilder& SetMagFilter(ESamplerFilter filter);
        virtual SamplerBuilder& SetMinFilter(ESamplerFilter filter);

        virtual SamplerBuilder& SetMipmapMode(ESamplerMipmapMode mode);

        virtual SamplerBuilder& SetAddressModeU(ESamplerAddressMode mode);
        virtual SamplerBuilder& SetAddressModeV(ESamplerAddressMode mode);
        virtual SamplerBuilder& SetAddressModeW(ESamplerAddressMode mode);

        virtual SamplerBuilder& SetMipLodBias(float bias);
        virtual SamplerBuilder& SetMinLod(float lod);
        virtual SamplerBuilder& SetMaxLod(float lod);

        virtual SamplerBuilder& SetAnisotropyEnable(bool enable);
        virtual SamplerBuilder& SetMaxAnisotropy(float maxAnisotropy);

        virtual SamplerBuilder& SetBorderColor(ESamplerBorderColor color);

    protected:
        ESamplerFilter m_MagFilter;
        ESamplerFilter m_MinFilter;

        ESamplerMipmapMode m_MipmapMode;

        ESamplerAddressMode m_AddressModeU;
        ESamplerAddressMode m_AddressModeV;
        ESamplerAddressMode m_AddressModeW;

        float m_MipLodBias;
        float m_MinLod;
        float m_MaxLod;

        bool m_AnisotropyEnable;
        float m_MaxAnisotropy;

        ESamplerBorderColor m_BorderColor;
    };
}