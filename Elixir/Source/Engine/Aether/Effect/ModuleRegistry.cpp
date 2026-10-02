#include "epch.h"
#include "ModuleRegistry.h"

#include <Engine/Aether/Modules/ApplyAngularVelocity.h>
#include <Engine/Aether/Modules/ApplyGravity.h>
#include <Engine/Aether/Modules/ApplyLinearDrag.h>
#include <Engine/Aether/Modules/ApplyVortex.h>
#include <Engine/Aether/Modules/ColorOverLife.h>
#include <Engine/Aether/Modules/KillOutsideBounds.h>
#include <Engine/Aether/Modules/ScaleOverLife.h>
#include <Engine/Aether/Modules/SetColor.h>
#include <Engine/Aether/Modules/SetLifetime.h>
#include <Engine/Aether/Modules/SetPositionBox.h>
#include <Engine/Aether/Modules/SetPositionCircularPath.h>
#include <Engine/Aether/Modules/SetPositionDisk.h>
#include <Engine/Aether/Modules/SetPositionOnCircle.h>
#include <Engine/Aether/Modules/SetPositionVortexRibbonPath.h>
#include <Engine/Aether/Modules/SetRibbonId.h>
#include <Engine/Aether/Modules/SetRibbonIdFromSpawnOrder.h>
#include <Engine/Aether/Modules/SetRotation.h>
#include <Engine/Aether/Modules/SetScale.h>
#include <Engine/Aether/Modules/SetSize.h>
#include <Engine/Aether/Modules/SetVelocityCone.h>
#include <Engine/Aether/Modules/SizeOverLife.h>

namespace Elixir::Aether::Effect
{
    namespace
    {
        using Registry = std::unordered_map<std::string, ModuleFactory>;

        Registry& GetRegistry()
        {
            // The registry intentionally survives static destruction. Module
            // registration can run from another translation unit during shutdown.
            static auto* registry = new Registry();
            return *registry;
        }
    }

    bool ModuleRegistry::Register(const std::string_view typeName, const ModuleFactory factory)
    {
        EE_CORE_ASSERT(factory, "Aether module factory must be valid.")
        const auto [_, inserted] = GetRegistry().emplace(std::string(typeName), factory);
        EE_CORE_ASSERT(inserted, "Aether module type '{}' was registered twice.", typeName)
        return inserted;
    }

    Scope<Modules::Module> ModuleRegistry::Create(
        const std::string_view typeName,
        ModuleParseContext& context,
        simdjson::ondemand::object& object
    )
    {
        const auto it = GetRegistry().find(std::string(typeName));
        if (context.Failed() || it == GetRegistry().end())
            return nullptr;
        auto module = it->second(context, object);
        return context.Failed() ? nullptr : std::move(module);
    }

    // Register built-in factories only in the engine shared library.
    AETHER_REGISTER_MODULE(ApplyAngularVelocity, Modules::ApplyAngularVelocity::Create)
    AETHER_REGISTER_MODULE(ApplyGravity, Modules::ApplyGravity::Create)
    AETHER_REGISTER_MODULE(ApplyLinearDrag, Modules::ApplyLinearDrag::Create)
    AETHER_REGISTER_MODULE(ApplyVortex, Modules::ApplyVortex::Create)
    AETHER_REGISTER_MODULE(ColorOverLife, Modules::ColorOverLife::Create)
    AETHER_REGISTER_MODULE(KillOutsideBounds, Modules::KillOutsideBounds::Create)
    AETHER_REGISTER_MODULE(ScaleOverLife, Modules::ScaleOverLife::Create)
    AETHER_REGISTER_MODULE(SetColor, Modules::SetColor::Create)
    AETHER_REGISTER_MODULE(SetLifetime, Modules::SetLifetime::Create)
    AETHER_REGISTER_MODULE(SetPositionBox, Modules::SetPositionBox::Create)
    AETHER_REGISTER_MODULE(SetPositionCircularPath, Modules::SetPositionCircularPath::Create)
    AETHER_REGISTER_MODULE(SetPositionDisk, Modules::SetPositionDisk::Create)
    AETHER_REGISTER_MODULE(SetPositionOnCircle, Modules::SetPositionOnCircle::Create)
    AETHER_REGISTER_MODULE(SetPositionVortexRibbonPath, Modules::SetPositionVortexRibbonPath::Create)
    AETHER_REGISTER_MODULE(SetRibbonId, Modules::SetRibbonId::Create)
    AETHER_REGISTER_MODULE(SetRibbonIdFromSpawnOrder, Modules::SetRibbonIdFromSpawnOrder::Create)
    AETHER_REGISTER_MODULE(SetRotation, Modules::SetRotation::Create)
    AETHER_REGISTER_MODULE(SetScale, Modules::SetScale::Create)
    AETHER_REGISTER_MODULE(SetSize, Modules::SetSize::Create)
    AETHER_REGISTER_MODULE(SetVelocityCone, Modules::SetVelocityCone::Create)
    AETHER_REGISTER_MODULE(SizeOverLife, Modules::SizeOverLife::Create)
}
