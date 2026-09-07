#pragma once

#include <Engine/Aether/Runtime/InstanceRegistry.h>
#include <Engine/Materials/MaterialRegistry.h>

using namespace Elixir;
using namespace Elixir::Aether;
using namespace Elixir::Materials;

class TestInstanceRegistry final
{
private:
    MaterialRegistry m_MaterialRegistry;

public:
    TestInstanceRegistry()
      : Registry(m_MaterialRegistry) {}

    Ref<SystemInstance> CreateRegisteredInstance(const Ref<System>& system)
    {
        const auto instance = system->CreateInstance();
        return Registry.Register(instance) ? instance : nullptr;
    }

    Ref<SystemInstance> CreateRegisteredInstance(std::string name = "Test system")
    {
        return CreateRegisteredInstance(CreateRef<System>(std::move(name)));
    }

    Runtime::InstanceRegistry Registry;
};
