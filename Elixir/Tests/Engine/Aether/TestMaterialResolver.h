#pragma once

#include <Engine/Materials/Compilation/Compiler.h>
#include <Engine/Materials/Rendering/MaterialRenderProxy.h>
#include <Engine/Materials/Rendering/MaterialResolver.h>

using namespace Elixir;
using namespace Elixir::Materials;
using namespace Elixir::Materials::Compilation;
using namespace Elixir::Materials::Rendering;

class TestMaterialResolver : public MaterialResolver
{
public:
    Ref<const MaterialRenderProxy> Resolve(const Ref<MaterialInstance>& instance) override
    {
        if (!instance || !instance->GetParent()) return nullptr;
        const auto result = Compiler::Build(*instance->GetParent());
        return result ? MaterialRenderProxy::Create(result.Material, *instance) : nullptr;
    }
};
