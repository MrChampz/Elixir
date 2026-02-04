#include "epch.h"
#include "TextureSet.h"

#include <Graphics/Vulkan/VulkanTextureSet.h>

namespace Elixir
{
    TextureSet::TextureSet(TextureSet&& other) noexcept
        : m_MaxTextures(other.m_MaxTextures),
          m_TextureCount(other.m_TextureCount),
          m_Textures(std::move(other.m_Textures)),
          m_FreeSlots(std::move(other.m_FreeSlots)),
          m_NeedsUpdate(other.m_NeedsUpdate),
          m_GraphicsContext(other.m_GraphicsContext)
    {
        other.m_TextureCount = 0;
    }

    TextureSet& TextureSet::operator=(TextureSet&& other) noexcept
    {
        if (this != &other)
        {
            m_MaxTextures = other.m_MaxTextures;
            m_TextureCount = other.m_TextureCount;
            m_Textures = std::move(other.m_Textures);
            m_FreeSlots = std::move(other.m_FreeSlots);
            m_NeedsUpdate = other.m_NeedsUpdate;
            m_GraphicsContext = other.m_GraphicsContext;

            other.m_TextureCount = 0;
        }

        return *this;
    }

    void TextureSet::Clear()
    {
        for (uint32_t i = 0; i < m_MaxTextures; ++i)
        {
            if (m_Textures[i])
                RemoveTexture(i);
        }
    }

    uint32_t TextureSet::AddTexture(const Ref<Texture>& texture)
    {
        EE_CORE_ASSERT(texture, "TextureSet: Texture is null!")
        uint32_t slot = GetTextureIndex(texture);
        if (slot == -1)
        {
            slot = FindFreeSlot();
            SetTexture(slot, texture);
        }
        return slot;
    }

    void TextureSet::SetTexture(const uint32_t index, const Ref<Texture>& texture)
    {
        EE_CORE_ASSERT(index < m_MaxTextures, "TextureSet: Index out of bounds!")
        EE_CORE_ASSERT(texture, "TextureSet: Texture is null!")

        if (!m_Textures[index])
        {
            m_TextureCount++;
            const auto it = std::ranges::find(m_FreeSlots, index);
            if (it != m_FreeSlots.end())
            {
                m_FreeSlots.erase(it);
            }
        }

        m_Textures[index] = texture;
        m_NeedsUpdate = true;
    }

    void TextureSet::RemoveTexture(const uint32_t index)
    {
        if (index >= m_MaxTextures)
            return;

        if (m_Textures[index])
        {
            m_Textures[index].reset();
            m_TextureCount--;
            m_FreeSlots.push_back(index);
            m_NeedsUpdate = true;
        }
    }

    bool TextureSet::HasTexture(const uint32_t index) const
    {
        return index < m_MaxTextures && m_Textures[index];
    }

    Ref<Texture> TextureSet::GetTexture(const uint32_t index) const
    {
        if (index >= m_MaxTextures)
            return nullptr;
        return m_Textures[index];
    }

    uint32_t TextureSet::GetTextureIndex(const Ref<Texture>& texture) const
    {
        for (uint32_t i = 0; i < m_MaxTextures; ++i)
        {
            if (m_Textures[i] == texture)
                return i;
        }
        return -1;
    }

    Ref<TextureSet> TextureSet::Create(const GraphicsContext* context, uint32_t maxTextures)
    {
        switch (context->GetAPI())
        {
            case EGraphicsAPI::Vulkan:
                return CreateRef<Vulkan::VulkanTextureSet>(context, maxTextures);
            default:
                EE_CORE_ASSERT(false, "Unknown GraphicsAPI!")
                return nullptr;
        }
    }

    TextureSet::TextureSet(const GraphicsContext* context, const uint32_t maxTextures)
        : m_MaxTextures(maxTextures), m_GraphicsContext(context)
    {
        EE_CORE_ASSERT(maxTextures > 0, "TextureSet: maxTextures must be greater than 0!")
        EE_CORE_ASSERT(
            maxTextures <= MAX_TEXTURES,
            "TextureSet: maxTextures exceeds {0} limit!", MAX_TEXTURES
        )

        m_Textures.resize(maxTextures);

        m_FreeSlots.reserve(maxTextures);
        for (auto i = 0; i < maxTextures; ++i)
        {
            m_FreeSlots.push_back(i);
        }
    }

    uint32_t TextureSet::FindFreeSlot() const
    {
        EE_CORE_ASSERT(!m_FreeSlots.empty(), "TextureSet: No free slots!")
        return m_FreeSlots.front();
    }
}