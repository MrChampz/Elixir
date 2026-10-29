#include "epch.h"
#include "ImageSet.h"

#include <Graphics/Vulkan/VulkanImageSet.h>

namespace Elixir
{
    SResourceHandle ImageSet::AddTexture(const Ref<Texture>& texture)
    {
        return AddImage(texture ? texture->GetImage() : nullptr);
    }

    Ref<ImageSet> ImageSet::Create(const GraphicsContext* context)
    {
        switch (context->GetAPI())
        {
            case EGraphicsAPI::Vulkan:
                return CreateRef<Vulkan::VulkanImageSet>(context);
            default:
                EE_CORE_ASSERT(false, "Unknown GraphicsAPI!")
                return nullptr;
        }
    }

    ImageSet::ImageSet(const GraphicsContext* context)
        : m_GraphicsContext(context)
    {
        EE_PROFILE_ZONE_SCOPED()
    }
}
