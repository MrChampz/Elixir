#pragma once

#include <Engine/Material/MaterialResolver.h>

using namespace Elixir;

class TestMaterialResolver : public MaterialResolver
{
public:
    Ref<const MaterialRenderProxy> Resolve(const Ref<MaterialInstance>& instance) override
    {
        if (!instance || !instance->GetParent()) return nullptr;
        const auto result = MaterialCompiler::Build(*instance->GetParent());
        return result ? MaterialRenderProxy::Create(result.Material, *instance) : nullptr;
    }
};