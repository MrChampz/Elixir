#pragma once

#include <Engine/Aether/Runtime/InstanceRegistry.h>
#include <Engine/Material/MaterialRegistry.h>

#include "TestMaterialResolver.h"

using namespace Elixir;
using namespace Elixir::Aether;

class TestInstanceRegistry final
{
private:
    MaterialRegistry m_MaterialRegistry;
    TestMaterialResolver m_MaterialResolver;

public:
    TestInstanceRegistry()
      : Registry(m_MaterialRegistry, m_MaterialResolver) {}

    Ref<SystemInstance> CreateInstance(std::string name = "Test system")
    {
        return Registry.CreateInstance(CreateRef<System>(std::move(name)));
    }

    Runtime::InstanceRegistry Registry;
};
