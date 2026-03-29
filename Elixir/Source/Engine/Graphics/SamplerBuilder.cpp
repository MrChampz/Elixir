#include "epch.h"
#include "SamplerBuilder.h"

namespace Elixir
{
    SamplerBuilder::SamplerBuilder()
	{
        SamplerBuilder::Clear();
	}

	Ref<Sampler> SamplerBuilder::Build(const GraphicsContext* context)
	{
        SSamplerCreateInfo info = {};

        info.MagFilter = m_MagFilter;
        info.MinFilter = m_MinFilter;

		info.MipmapMode = m_MipmapMode;

		info.AddressModeU = m_AddressModeU;
		info.AddressModeV = m_AddressModeV;
		info.AddressModeW = m_AddressModeW;

        info.MipLodBias = m_MipLodBias;
        info.MinLod = m_MinLod;
        info.MaxLod = m_MaxLod;

        info.AnisotropyEnable = m_AnisotropyEnable;
        info.MaxAnisotropy = m_MaxAnisotropy;

        info.BorderColor = m_BorderColor;

		return std::move(Sampler::Create(context, info));
	}

	SamplerBuilder& SamplerBuilder::Clear()
	{
		m_MagFilter = ESamplerFilter::Linear;
		m_MinFilter = ESamplerFilter::Linear;

		m_MipmapMode = ESamplerMipmapMode::Linear;

		m_AddressModeU = ESamplerAddressMode::Repeat;
		m_AddressModeV = ESamplerAddressMode::Repeat;
		m_AddressModeW = ESamplerAddressMode::Repeat;

		m_MipLodBias = 0;
		m_MinLod = 0;
		m_MaxLod = 0;

		m_AnisotropyEnable = false;
		m_MaxAnisotropy = 0;

		m_BorderColor = ESamplerBorderColor::FloatTransparentBlack;

		return *this;
	}

	SamplerBuilder& SamplerBuilder::SetMagFilter(const ESamplerFilter filter)
	{
		m_MagFilter = filter;
		return *this;
	}

	SamplerBuilder& SamplerBuilder::SetMinFilter(const ESamplerFilter filter)
	{
		m_MinFilter = filter;
		return *this;
	}

	SamplerBuilder& SamplerBuilder::SetMipmapMode(const ESamplerMipmapMode mode)
	{
		m_MipmapMode = mode;
		return *this;
	}

	SamplerBuilder& SamplerBuilder::SetAddressModeU(const ESamplerAddressMode mode)
	{
		m_AddressModeU = mode;
		return *this;
	}

	SamplerBuilder& SamplerBuilder::SetAddressModeV(const ESamplerAddressMode mode)
	{
		m_AddressModeV = mode;
		return *this;
	}

	SamplerBuilder& SamplerBuilder::SetAddressModeW(const ESamplerAddressMode mode)
	{
		m_AddressModeW = mode;
		return *this;
	}

	SamplerBuilder& SamplerBuilder::SetMipLodBias(const float bias)
	{
		m_MipLodBias = bias;
		return *this;
	}

	SamplerBuilder& SamplerBuilder::SetMinLod(const float lod)
	{
		m_MinLod = lod;
		return *this;
	}

	SamplerBuilder& SamplerBuilder::SetMaxLod(const float lod)
	{
		m_MaxLod = lod;
		return *this;
	}

	SamplerBuilder& SamplerBuilder::SetAnisotropyEnable(const bool enable)
	{
		m_AnisotropyEnable = enable;
		return *this;
	}

	SamplerBuilder& SamplerBuilder::SetMaxAnisotropy(const float maxAnisotropy)
	{
		m_MaxAnisotropy = maxAnisotropy;
		return *this;
	}

	SamplerBuilder& SamplerBuilder::SetBorderColor(const ESamplerBorderColor color)
	{
		m_BorderColor = color;
		return *this;
	}
}