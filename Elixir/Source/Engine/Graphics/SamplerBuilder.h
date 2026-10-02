#pragma once

#include <Engine/Graphics/Sampler.h>

namespace Elixir
{
    class ELIXIR_API SamplerBuilder final
    {
      public:
        SamplerBuilder();

        Ref<Sampler> Build(const GraphicsContext* context);
        SamplerBuilder& Clear();

        SamplerBuilder& SetMagFilter(ESamplerFilter filter);
        SamplerBuilder& SetMinFilter(ESamplerFilter filter);

        SamplerBuilder& SetMipmapMode(ESamplerMipmapMode mode);

        SamplerBuilder& SetAddressModeU(ESamplerAddressMode mode);
        SamplerBuilder& SetAddressModeV(ESamplerAddressMode mode);
        SamplerBuilder& SetAddressModeW(ESamplerAddressMode mode);

        SamplerBuilder& SetMipLodBias(float bias);
        SamplerBuilder& SetMinLod(float lod);
        SamplerBuilder& SetMaxLod(float lod);

        SamplerBuilder& SetAnisotropyEnable(bool enable);
        SamplerBuilder& SetMaxAnisotropy(float maxAnisotropy);

        SamplerBuilder& SetBorderColor(ESamplerBorderColor color);

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
