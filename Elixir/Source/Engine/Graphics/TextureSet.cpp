#include "epch.h"
#include "TextureSet.h"

#ifdef EE_PLATFORM_WINDOWS
#include <Graphics/D3D12/D3D12TextureSet.h>
#endif
#include <Graphics/Vulkan/VulkanTextureSet.h>

namespace Elixir
{
    Ref<TextureSet> TextureSet::Create(const GraphicsContext* context)
    {
        switch (context->GetAPI())
        {
            case EGraphicsAPI::Vulkan:
                return CreateRef<Vulkan::VulkanTextureSet>(context);
#ifdef EE_PLATFORM_WINDOWS
            case EGraphicsAPI::D3D12:
                return CreateRef<D3D12::D3D12TextureSet>(context);
#endif
            default:
                EE_CORE_ASSERT(false, "Unknown GraphicsAPI!")
                return nullptr;
        }
    }

    TextureSet::TextureSet(const GraphicsContext* context)
        : m_GraphicsContext(context)
    {
        EE_PROFILE_ZONE_SCOPED()
    }
}
