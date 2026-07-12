#pragma once

#ifdef EE_PLATFORM_WINDOWS

#include <Engine/Graphics/Texture.h>
#include <Graphics/D3D12/D3D12GraphicsContext.h>

namespace Elixir::D3D12
{
    ID3D12Resource* TryToGetD3D12ImageResource(const Image* image);

    template <typename Base>
    class D3D12BaseImage : public Base
    {
      public:
        template <typename... Args>
        D3D12BaseImage(const GraphicsContext* context, const SImageCreateInfo& info, Args&&... args);
        ~D3D12BaseImage() override = default;

        void Destroy() override;
        void Resize(const Ref<CommandBuffer>& cmd, Extent3D extent) override;
        void Transition(const CommandBuffer* cmd, EImageLayout layout) override;
        void Copy(
            const CommandBuffer* cmd,
            Image* dst,
            const Extent3D& srcExtent,
            const Extent3D& dstExtent
        ) override;
        void CopyFrom(
            const CommandBuffer* cmd,
            const Buffer* src,
            std::span<SBufferImageCopy> regions = {}
        ) override;

        bool IsValid() const override { return m_Resource != nullptr; }

        ID3D12Resource* GetD3D12Resource() const { return m_Resource.Get(); }
        D3D12_RESOURCE_STATES GetD3D12State() const { return m_State; }

        D3D12_CPU_DESCRIPTOR_HANDLE GetRenderTargetView() const { return m_RTV.CPU; }
        D3D12_CPU_DESCRIPTOR_HANDLE GetDepthStencilView() const { return m_DSV.CPU; }

        void CreateShaderResourceView(D3D12_CPU_DESCRIPTOR_HANDLE destination) const;

      protected:
        void CreateImage(const SImageCreateInfo& info);
        void CreateViews();
        void UploadInitialData(const SImageCreateInfo& info);
        void UpdateSampler() override {}

        ComPtr<ID3D12Resource> m_Resource;
        D3D12_RESOURCE_STATES m_State = D3D12_RESOURCE_STATE_COMMON;
        SD3D12DescriptorAllocation m_RTV;
        SD3D12DescriptorAllocation m_DSV;
        const D3D12GraphicsContext* m_GraphicsContext = nullptr;
    };

    class ELIXIR_API D3D12Image final : public D3D12BaseImage<Image>
    {
      public:
        D3D12Image(
            const GraphicsContext* context,
            EImageFormat format,
            uint32_t width,
            const void* data = nullptr
        );
        D3D12Image(const GraphicsContext* context, const SImageCreateInfo& info);
        ~D3D12Image() override;
    };

    class ELIXIR_API D3D12DepthStencilImage final : public D3D12BaseImage<DepthStencilImage>
    {
      public:
        D3D12DepthStencilImage(
            const GraphicsContext* context,
            EDepthStencilImageFormat format,
            uint32_t width,
            uint32_t height
        );
        D3D12DepthStencilImage(const GraphicsContext* context, const SImageCreateInfo& info);
        ~D3D12DepthStencilImage() override;
    };

    class ELIXIR_API D3D12Texture final : public D3D12BaseImage<Texture>
    {
      public:
        D3D12Texture(
            const GraphicsContext* context,
            EImageFormat format,
            uint32_t width,
            const void* data = nullptr,
            const std::string& path = ""
        );
        D3D12Texture(
            const GraphicsContext* context,
            const SImageCreateInfo& info,
            const std::string& path = ""
        );
        ~D3D12Texture() override;
    };

    class ELIXIR_API D3D12Texture2D final : public D3D12BaseImage<Texture2D>
    {
      public:
        D3D12Texture2D(
            const GraphicsContext* context,
            EImageFormat format,
            uint32_t width,
            uint32_t height,
            const void* data = nullptr,
            const std::string& path = ""
        );
        D3D12Texture2D(
            const GraphicsContext* context,
            const SImageCreateInfo& info,
            const std::string& path = ""
        );
        ~D3D12Texture2D() override;
    };

    class ELIXIR_API D3D12Texture3D final : public D3D12BaseImage<Texture3D>
    {
      public:
        D3D12Texture3D(
            const GraphicsContext* context,
            EImageFormat format,
            uint32_t width,
            uint32_t height,
            uint32_t depth,
            const void* data = nullptr,
            const std::string& path = ""
        );
        D3D12Texture3D(
            const GraphicsContext* context,
            const SImageCreateInfo& info,
            const std::string& path = ""
        );
        ~D3D12Texture3D() override;
    };
}

#endif
