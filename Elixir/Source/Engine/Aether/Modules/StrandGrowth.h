#pragma once
#include <Engine/Aether/Modules/Module.h>
#include <Engine/Aether/Core/StrandGrowthSettings.h>
#include <Engine/Aether/Effect/ModuleParseContext.h>

namespace Elixir::Aether::Modules
{
    /** Configures the once-per-chain development stage before strand physics. */
    class ELIXIR_API StrandGrowth final : public UpdateModule
    {
    public:
        /** Creates a growth module; settings are validated during system compilation. */
        explicit StrandGrowth(SStrandGrowthSettings settings = {}) : m_Settings(settings)
        { SetTarget(EModuleTarget::StrandPoints); }
        /** Growth belongs to strand chains, not particle motion. */
        bool SupportsTarget(EModuleTarget target) const override { return target == EModuleTarget::StrandPoints; }
        /** Returns authoring data consumed by the separate development stage. */
        const SStrandGrowthSettings* GetStrandGrowthSettings() const override { return &m_Settings; }
        /** The emitter compiles growth configuration once; no per-point force instruction is emitted. */
        void Compile(ModuleCompileContext&) const override {}
        /** Parses growth, branch, and state controls from an update module. */
        static Scope<Module> Create(Effect::ModuleParseContext& context, simdjson::ondemand::object& object)
        {
            SStrandGrowthSettings settings;
            if (context.HasField(object,"speed")) settings.Speed = context.RequireFloat(object,"speed");
            if (context.HasField(object,"maxLength")) settings.MaxLength = context.RequireFloat(object,"maxLength");
            if (context.HasField(object,"branchCount")) settings.BranchCount = context.RequireUInt(object,"branchCount");
            if (context.HasField(object,"branchInterval")) settings.BranchInterval = context.RequireUInt(object,"branchInterval");
            if (context.HasField(object,"branchLengthScale")) settings.BranchLengthScale = context.RequireFloat(object,"branchLengthScale");
            if (context.HasField(object,"branchVariation")) settings.BranchVariation = context.RequireFloat(object,"branchVariation");
            if (context.HasField(object,"branchDelay")) settings.BranchDelay = context.RequireFloat(object,"branchDelay");
            if (context.HasField(object,"seed")) settings.Seed = context.RequireUInt(object,"seed");
            if (context.HasField(object,"state"))
            {
                const auto state = context.RequireString(object,"state");
                if (state == "Growing") settings.State = EStrandGrowthState::Growing;
                else if (state == "Mature") settings.State = EStrandGrowthState::Mature;
                else if (state == "Paused") settings.State = EStrandGrowthState::Paused;
                else context.Fail("Unknown strand growth state '{}'.",state);
            }
            if (!settings.IsValid()) context.Fail("Invalid strand growth settings.");
            return context.Failed() ? nullptr : CreateScope<StrandGrowth>(settings);
        }
    private:
        SStrandGrowthSettings m_Settings;
    };
}
