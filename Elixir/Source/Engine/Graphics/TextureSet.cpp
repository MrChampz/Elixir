#include "epch.h"
#include "TextureSet.h"

#include <Graphics/Vulkan/VulkanTextureSet.h>

namespace Elixir
{
    Ref<TextureSet> TextureSet::Create(const GraphicsContext* context)
    {
        switch (context->GetAPI())
        {
            case EGraphicsAPI::Vulkan:
                return CreateRef<Vulkan::VulkanTextureSet>(context);
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