#include <gtest/gtest.h>

#include <Engine/Aether/Effect/Effect.h>
#include <Engine/Aether/Effect/ModuleRegistry.h>
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


using namespace Elixir;
using namespace Elixir::Aether;
using namespace Elixir::Aether::Modules;

namespace
{
    /** @brief Supplies a factory registered by a consumer of the engine DLL. */
    class RegisteredTestModule final : public SpawnModule
    {
    public:
        /** @brief Creates the module with a literal test value. */
        explicit RegisteredTestModule(float value) : m_Value(value) {}

        /** @brief Emits the test value as a literal operation. */
        void Compile(ModuleCompileContext& context) const override
        {
            context.Emit({ .Data0 = glm::vec4(m_Value) });
        }

        /** @brief Reads the test value from its JSON object. */
        static Scope<Module> Create(Effect::ModuleParseContext& context, simdjson::ondemand::object& object)
        {
            return CreateScope<RegisteredTestModule>(context.RequireFloat(object, "value"));
        }

    private:
        float m_Value;
    };

    AETHER_REGISTER_MODULE(RegisteredTestModule, RegisteredTestModule::Create)

    // Parses an object while keeping its JSON storage alive during factory creation.
    Scope<Module> CreateRegistered(std::string_view type, std::string_view json, Effect::ModuleParseContext& context)
    {
        simdjson::ondemand::parser parser;
        const simdjson::padded_string storage{ json };
        auto document = parser.iterate(storage);
        simdjson::ondemand::object object;
        const auto error = document.get_object().get(object);
        EXPECT_EQ(error, simdjson::SUCCESS);
        if (error) return nullptr;
        return Effect::ModuleRegistry::Create(type, context, object);
    }
}

TEST(ModuleRegistryTest, CreatesSetLifetimeFromJson)
{
    Effect::ModuleParseContext context;
    const auto module = CreateRegistered("SetLifetime", R"({"min":"$Low","max":3})", context);
    ASSERT_FALSE(context.Failed()) << context.GetError();
    ASSERT_TRUE(module);
    EXPECT_NE(dynamic_cast<const SetLifetime*>(module.get()), nullptr);
}

TEST(ModuleRegistryTest, CreatesSetSizeFromJson)
{
    Effect::ModuleParseContext context;
    const auto module = CreateRegistered("SetSize", R"({"min":1,"max":2})", context);
    ASSERT_FALSE(context.Failed()) << context.GetError();
    ASSERT_TRUE(module);
    EXPECT_NE(dynamic_cast<const SetSize*>(module.get()), nullptr);
}

TEST(ModuleRegistryTest, CreatesSetScaleFromJson)
{
    Effect::ModuleParseContext context;
    const auto module = CreateRegistered("SetScale", R"({"min":1,"max":2})", context);
    ASSERT_FALSE(context.Failed()) << context.GetError();
    ASSERT_TRUE(module);
    EXPECT_NE(dynamic_cast<const SetScale*>(module.get()), nullptr);
}

TEST(ModuleRegistryTest, CreatesSetRotationFromJson)
{
    Effect::ModuleParseContext context;
    const auto module = CreateRegistered("SetRotation", R"({"min":0,"max":1})", context);
    ASSERT_FALSE(context.Failed()) << context.GetError();
    ASSERT_TRUE(module);
    EXPECT_NE(dynamic_cast<const SetRotation*>(module.get()), nullptr);
}

TEST(ModuleRegistryTest, CreatesSetColorFromJson)
{
    Effect::ModuleParseContext context;
    const auto module = CreateRegistered("SetColor", R"({"color":"$Tint"})", context);
    ASSERT_FALSE(context.Failed()) << context.GetError();
    ASSERT_TRUE(module);
    EXPECT_NE(dynamic_cast<const SetColor*>(module.get()), nullptr);
}

TEST(ModuleRegistryTest, CreatesSetPositionDiskFromJson)
{
    Effect::ModuleParseContext context;
    const auto module = CreateRegistered("SetPositionDisk", R"({"center":[1,2,3],"radius":4})", context);
    ASSERT_FALSE(context.Failed()) << context.GetError();
    ASSERT_TRUE(module);
    EXPECT_NE(dynamic_cast<const SetPositionDisk*>(module.get()), nullptr);
}

TEST(ModuleRegistryTest, CreatesSetPositionBoxFromJson)
{
    Effect::ModuleParseContext context;
    const auto module = CreateRegistered("SetPositionBox", R"({"min":[0,0,0],"max":[1,1,1]})", context);
    ASSERT_FALSE(context.Failed()) << context.GetError();
    ASSERT_TRUE(module);
    EXPECT_NE(dynamic_cast<const SetPositionBox*>(module.get()), nullptr);
}

TEST(ModuleRegistryTest, CreatesSetVelocityConeFromJson)
{
    Effect::ModuleParseContext context;
    const auto module = CreateRegistered("SetVelocityCone", R"({"direction":[0,1,0],"angle":"$Angle","minSpeed":1,"maxSpeed":2})", context);
    ASSERT_FALSE(context.Failed()) << context.GetError();
    ASSERT_TRUE(module);
    EXPECT_NE(dynamic_cast<const SetVelocityCone*>(module.get()), nullptr);
}

TEST(ModuleRegistryTest, CreatesSetPositionOnCircleFromJson)
{
    Effect::ModuleParseContext context;
    const auto module = CreateRegistered("SetPositionOnCircle", R"({"center":[1,2,3],"radius":4,"angularSpeed":5})", context);
    ASSERT_FALSE(context.Failed()) << context.GetError();
    ASSERT_TRUE(module);
    EXPECT_NE(dynamic_cast<const SetPositionOnCircle*>(module.get()), nullptr);
}

TEST(ModuleRegistryTest, CreatesSetPositionCircularPathFromJson)
{
    Effect::ModuleParseContext context;
    const auto module = CreateRegistered("SetPositionCircularPath", R"({"baseOffset":[1,2,3],"primaryAmplitude":[4,5,6],"secondaryAmplitude":[7,8,9],"timeScale":1})", context);
    ASSERT_FALSE(context.Failed()) << context.GetError();
    ASSERT_TRUE(module);
    EXPECT_NE(dynamic_cast<const SetPositionCircularPath*>(module.get()), nullptr);
}

TEST(ModuleRegistryTest, CreatesSetPositionVortexRibbonPathFromJson)
{
    Effect::ModuleParseContext context;
    const auto module = CreateRegistered("SetPositionVortexRibbonPath", R"({"center":[1,2,3],"orbitSpeed":1,"baseRadius":2,"radiusAmplitude":3,"radiusSpeed":4,"pulseAmplitude":5,"pulseSpeed":6,"curlAmplitude":7,"depthAmplitude":8})", context);
    ASSERT_FALSE(context.Failed()) << context.GetError();
    ASSERT_TRUE(module);
    EXPECT_NE(dynamic_cast<const SetPositionVortexRibbonPath*>(module.get()), nullptr);
}

TEST(ModuleRegistryTest, CreatesSetRibbonIdFromJson)
{
    Effect::ModuleParseContext context;
    const auto module = CreateRegistered("SetRibbonId", R"({"ribbonId":3})", context);
    ASSERT_FALSE(context.Failed()) << context.GetError();
    ASSERT_TRUE(module);
    EXPECT_NE(dynamic_cast<const SetRibbonId*>(module.get()), nullptr);
}

TEST(ModuleRegistryTest, CreatesSetRibbonIdFromSpawnOrderFromJson)
{
    Effect::ModuleParseContext context;
    const auto module = CreateRegistered("SetRibbonIdFromSpawnOrder", R"({"ribbonCount":0,"firstRibbonId":4})", context);
    ASSERT_FALSE(context.Failed()) << context.GetError();
    ASSERT_TRUE(module);
    EXPECT_NE(dynamic_cast<const SetRibbonIdFromSpawnOrder*>(module.get()), nullptr);
}

TEST(ModuleRegistryTest, CreatesSizeOverLifeFromJson)
{
    Effect::ModuleParseContext context;
    const auto module = CreateRegistered("SizeOverLife", R"({"start":1,"end":2})", context);
    ASSERT_FALSE(context.Failed()) << context.GetError();
    ASSERT_TRUE(module);
    EXPECT_NE(dynamic_cast<const SizeOverLife*>(module.get()), nullptr);
}

TEST(ModuleRegistryTest, CreatesColorOverLifeFromJson)
{
    Effect::ModuleParseContext context;
    const auto module = CreateRegistered("ColorOverLife", R"({"start":"$Start","end":"$End","curve":"Color","input":"NormalizedAge"})", context);
    ASSERT_FALSE(context.Failed()) << context.GetError();
    ASSERT_TRUE(module);
    EXPECT_NE(dynamic_cast<const ColorOverLife*>(module.get()), nullptr);
}

TEST(ModuleRegistryTest, CreatesApplyGravityFromJson)
{
    Effect::ModuleParseContext context;
    const auto module = CreateRegistered("ApplyGravity", R"({"gravity":[0,-9.8,0,0]})", context);
    ASSERT_FALSE(context.Failed()) << context.GetError();
    ASSERT_TRUE(module);
    EXPECT_NE(dynamic_cast<const ApplyGravity*>(module.get()), nullptr);
}

TEST(ModuleRegistryTest, CreatesApplyLinearDragFromJson)
{
    Effect::ModuleParseContext context;
    const auto module = CreateRegistered("ApplyLinearDrag", R"({"drag":"$Drag"})", context);
    ASSERT_FALSE(context.Failed()) << context.GetError();
    ASSERT_TRUE(module);
    EXPECT_NE(dynamic_cast<const ApplyLinearDrag*>(module.get()), nullptr);
}

TEST(ModuleRegistryTest, CreatesApplyAngularVelocityFromJson)
{
    Effect::ModuleParseContext context;
    const auto module = CreateRegistered("ApplyAngularVelocity", R"({"value":"$Speed","input":"EmitterTime"})", context);
    ASSERT_FALSE(context.Failed()) << context.GetError();
    ASSERT_TRUE(module);
    EXPECT_NE(dynamic_cast<const ApplyAngularVelocity*>(module.get()), nullptr);
}

TEST(ModuleRegistryTest, CreatesApplyVortexFromJson)
{
    Effect::ModuleParseContext context;
    const auto module = CreateRegistered("ApplyVortex", R"({"center":[0,0,0,0],"tangential":"$Strength","radial":2})", context);
    ASSERT_FALSE(context.Failed()) << context.GetError();
    ASSERT_TRUE(module);
    EXPECT_NE(dynamic_cast<const ApplyVortex*>(module.get()), nullptr);
}

TEST(ModuleRegistryTest, CreatesScaleOverLifeFromJson)
{
    Effect::ModuleParseContext context;
    const auto module = CreateRegistered("ScaleOverLife", R"({"start":"$Start","end":2,"curve":"Scale","input":"Random"})", context);
    ASSERT_FALSE(context.Failed()) << context.GetError();
    ASSERT_TRUE(module);
    EXPECT_NE(dynamic_cast<const ScaleOverLife*>(module.get()), nullptr);
}

TEST(ModuleRegistryTest, CreatesKillOutsideBoundsFromJson)
{
    Effect::ModuleParseContext context;
    const auto module = CreateRegistered("KillOutsideBounds", R"({"min":[0,0,0],"max":[1,1,1]})", context);
    ASSERT_FALSE(context.Failed()) << context.GetError();
    ASSERT_TRUE(module);
    EXPECT_NE(dynamic_cast<const KillOutsideBounds*>(module.get()), nullptr);
}

TEST(ModuleRegistryTest, PreservesParameterBindingsAndOptionalDefaults)
{
    Effect::ModuleParseContext lifetimeContext;
    const auto lifetime = CreateRegistered("SetLifetime", R"({"min":"$Low","max":3})", lifetimeContext);
    const auto* range = dynamic_cast<const SetLifetime*>(lifetime.get());
    ASSERT_NE(range, nullptr);
    EXPECT_EQ(range->GetMinSecondsParamName(), "Low");
    EXPECT_FLOAT_EQ(range->GetMaxSeconds(), 3.0f);

    Effect::ModuleParseContext diskContext;
    const auto disk = CreateRegistered("SetPositionDisk", R"({"center":[1,2,3],"radius":4})", diskContext);
    const auto* position = dynamic_cast<const SetPositionDisk*>(disk.get());
    ASSERT_NE(position, nullptr);
    EXPECT_EQ(position->GetNormal(), glm::vec3(0.0f, 1.0f, 0.0f));

    Effect::ModuleParseContext curveContext;
    const auto curve = CreateRegistered("ScaleOverLife", R"({"start":"$Start","end":2,"curve":"Scale","input":"Random"})", curveContext);
    const auto* scale = dynamic_cast<const ScaleOverLife*>(curve.get());
    ASSERT_NE(scale, nullptr);
    EXPECT_EQ(scale->GetStartScaleParamName(), "Start");
    EXPECT_EQ(scale->GetCurveName(), "Scale");
    EXPECT_EQ(scale->GetCurveInput(), EDynamicInput::Random);
}

TEST(ModuleRegistryTest, RejectsInvalidFieldsAndPreservesTheFirstError)
{
    const std::array cases{
        std::pair{ "SetRibbonId", R"({"ribbonId":4294967296})" },
        std::pair{ "SetPositionDisk", R"({"center":[1,2],"radius":4})" },
        std::pair{ "SetPositionDisk", R"({"radius":4})" },
        std::pair{ "SetLifetime", R"({"min":true,"max":false})" },
        std::pair{ "ApplyAngularVelocity", R"({"value":1,"input":"Unknown"})" },
    };
    for (const auto& [type, json] : cases)
    {
        SCOPED_TRACE(type);
        Effect::ModuleParseContext context;
        EXPECT_FALSE(CreateRegistered(type, json, context));
        ASSERT_TRUE(context.Failed());
        const auto error = context.GetError();
        EXPECT_FALSE(error.empty());
        context.Fail("Subsequent error");
        EXPECT_EQ(context.GetError(), error);
    }
}

TEST(ModuleRegistryTest, UnknownTypesReturnNullWithoutConsumingFields)
{
    Effect::ModuleParseContext context;
    EXPECT_FALSE(CreateRegistered("UnknownModule", "{}", context));
    EXPECT_FALSE(context.Failed());
}

TEST(ModuleRegistryTest, LoadsExistingEffectAssets)
{
    const auto root = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path().parent_path().parent_path();
    for (const auto* asset : { "FireAndFireworks.json", "RainStorm.json", "RibbonGarden.json", "RibbonVortex.json" })
    {
        SCOPED_TRACE(asset);
        const auto system = Elixir::Aether::Effect::LoadEffectFile(root / "Assets" / "VFX" / asset);
        ASSERT_TRUE(system);
        EXPECT_FALSE(system->GetEmitters().empty());
    }
}

TEST(ModuleRegistryTest, EffectLoaderRejectsUnknownTypesAndWrongPhases)
{
    const auto filepath = std::filesystem::temp_directory_path() / ("Elixir-registry-" + UUID().ToString() + ".json");
    for (const auto* modules : {
        R"("spawnModules":[{"type":"UnknownModule"}])",
        R"("spawnModules":[{"type":"ApplyLinearDrag","drag":1}])",
        R"("updateModules":[{"type":"SetLifetime","min":1,"max":2}])",
        R"("spawnModules":[{"type":"SetPositionDisk","center":[1,2],"radius":4}])",
    })
    {
        SCOPED_TRACE(modules);
        {
            std::ofstream file(filepath);
            file << R"({"name":"Invalid","emitters":[{"name":"Smoke","maxParticles":8,"spawnRate":1,)"
                 << modules << "}]}";
        }
        EXPECT_FALSE(Elixir::Aether::Effect::LoadEffectFile(filepath));
    }
    std::filesystem::remove(filepath);
}

TEST(ModuleRegistryTest, CreatesAndCompilesModulesRegisteredOutsideTheEngine)
{
    Effect::ModuleParseContext context;
    const auto module = CreateRegistered("RegisteredTestModule", R"({"value":7})", context);
    ASSERT_FALSE(context.Failed()) << context.GetError();
    ASSERT_TRUE(module);
    EXPECT_EQ(module->GetPhase(), EModulePhase::Spawn);
    std::vector<SGPUParticleOp> operations;
    const std::vector<Elixir::Aether::Core::SGPUParameter> parameters;
    ModuleCompileContext compileContext(operations, parameters, "Test", 1.0f);
    module->Compile(compileContext);
    ASSERT_EQ(operations.size(), 1u);
    EXPECT_EQ(operations[0].Data0, glm::vec4(7.0f));
}
