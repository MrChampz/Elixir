#include "epch.h"
#include "Icon.h"

namespace Elixir
{
    namespace
    {
        uint64_t MakeRasterKey(const glm::uvec2 size)
        {
            return (static_cast<uint64_t>(size.x) << 32) | size.y;
        }
    }

    Icon::Icon(
        const GraphicsContext* context,
        SIconSource source,
        Ref<IconContent> content
    ) : m_GraphicsContext(context),
        m_Source(std::move(source)),
        m_Content(std::move(content)) {}

    Ref<Texture2D> Icon::GetTexture(const glm::vec2& logicalSize) const
    {
        EE_CORE_ASSERT(m_GraphicsContext, "Icon requires an initialized IconManager")
        if (!m_GraphicsContext) return nullptr;

        const float dpiScale = m_GraphicsContext->GetDPIScale();
        const glm::uvec2 pixelSize = glm::max(
            glm::uvec2(glm::round(glm::max(logicalSize, glm::vec2(1.0f)) * dpiScale)),
            glm::uvec2(1)
        );
        const uint64_t key = MakeRasterKey(pixelSize);
        if (const auto it = m_Textures.find(key); it != m_Textures.end())
            return it->second;

        const SIconBitmap bitmap = m_Content->Rasterize({ .PixelSize = pixelSize });
        if (!bitmap.IsValid())
        {
            EE_CORE_ERROR("Cannot rasterize icon! [Path={0}]", m_Source.Path.string())
            return nullptr;
        }

        const auto texture = Texture2D::Create(
            m_GraphicsContext,
            EImageFormat::R8G8B8A8_UNORM,
            bitmap.Size.x,
            bitmap.Size.y,
            bitmap.Pixels.data(),
            m_Source.Path.string()
        );
        m_Textures.emplace(key, texture);
        return texture;
    }
}