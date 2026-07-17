#pragma once

#include <Engine/Graphics/Material/MaterialResource.h>

#include <functional>
#include <string>
#include <unordered_map>

namespace Elixir
{
    // Immutable runtime snapshot of a MaterialInstance. It owns resolved parameter
    // values, so the render thread never reads the instance being edited by the UI.
    class ELIXIR_API MaterialRenderProxy
    {
      public:
        using TextureIndexResolver = std::function<uint32_t(const Ref<Texture>&)>;

        static Ref<const MaterialRenderProxy> Create(
            const MaterialInstance& instance,
            const Ref<const MaterialResource>& resource = nullptr);

        [[nodiscard]] const Ref<const MaterialResource>& GetResource() const { return m_Resource; }
        [[nodiscard]] const std::string& GetName() const { return m_Name; }
        [[nodiscard]] const SMaterialParam* GetParameter(const std::string& name) const;
        [[nodiscard]] float GetScalar(const std::string& name) const;
        [[nodiscard]] glm::vec4 GetVector(const std::string& name) const;
        [[nodiscard]] Ref<Texture> GetTexture(const std::string& name) const;

        uint32_t CollectGraphParams(glm::vec4* out, uint32_t maxCount,
            const TextureIndexResolver& textureResolver = {}) const;

      private:
        MaterialRenderProxy(std::string name,
            Ref<const MaterialResource> resource,
            std::unordered_map<std::string, SMaterialParam> parameters);

        std::string m_Name;
        Ref<const MaterialResource> m_Resource;
        std::unordered_map<std::string, SMaterialParam> m_Parameters;
    };
}
