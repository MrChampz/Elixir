#include "epch.h"
#include "MaterialProxyResolver.h"

namespace Elixir::Materials
{
    MaterialProxyResolver::MaterialProxyResolver(const ShaderLoader* shaderLoader)
      : m_CompilationCache(shaderLoader) {}

    Ref<const MaterialRenderProxy> MaterialProxyResolver::Resolve(
        const Ref<MaterialInstance>& instance
    )
    {
        if (!instance || !instance->GetParent())
            return nullptr;

        const auto compiled = m_CompilationCache.GetOrCompile(instance->GetParent());
        return compiled
            ? MaterialRenderProxy::Create(compiled, *instance)
            : nullptr;
    }
}