#include <gtest/gtest.h>
#include <Engine/Graphics/Shader/Shader.h>
#include <Graphics/SpirV/SpirVShaderBackend.h>

using namespace Elixir;

namespace
{
    /**
     * @brief Check reflected integer member types and their byte sizes.
     * @param constants Reflected scalar and vector members to inspect.
     */
    void ExpectIntegerTypes(const std::span<const ShaderConstant> constants)
    {
        const std::array expected{
            std::tuple{"I1", EConstantType::Int, 4u},
            std::tuple{"I2", EConstantType::Int2, 8u},
            std::tuple{"I3", EConstantType::Int3, 12u},
            std::tuple{"I4", EConstantType::Int4, 16u},
            std::tuple{"U1", EConstantType::UInt, 4u},
            std::tuple{"U2", EConstantType::UInt2, 8u},
            std::tuple{"U3", EConstantType::UInt3, 12u},
            std::tuple{"U4", EConstantType::UInt4, 16u},
        };
        for (const auto& [name, type, size] : expected)
        {
            SCOPED_TRACE(name);
            const auto found = std::ranges::find_if(constants, [name](const auto& constant)
            {
                return constant.GetName() == name;
            });
            ASSERT_NE(found, constants.end());
            EXPECT_EQ(found->GetType(), type);
            EXPECT_EQ(found->GetSize(), size);
        }
    }
}

class SpirVShaderBackendTest : public testing::Test
{
protected:
    /** @brief Load the compiled fixture without creating a graphics device. */
    void SetUp() override
    {
        const auto path = std::filesystem::path(ELIXIR_TEST_SHADER_DIR)
            / "IntegerReflection.cs.spirv";
        if (!std::filesystem::is_regular_file(path))
            GTEST_SKIP() << "Test shader requires the Vulkan SDK shader compiler.";
        Module = SpirV::SpirVShaderBackend{}.LoadModule(path, EShaderStage::Compute);
        ASSERT_FALSE(Module.Bytecode.empty());
    }

    SShaderModuleCreateInfo Module;
};

TEST_F(SpirVShaderBackendTest, ReflectsIntegerScalarsAndVectorsInConstantBuffersAndStructs)
{
    ASSERT_EQ(Module.ConstantBuffers.size(), 1);
    const auto& constants = Module.ConstantBuffers.front().GetConstants();
    ExpectIntegerTypes(constants);
    const auto nested = std::ranges::find_if(constants, [](const auto& constant)
    {
        return constant.GetName() == "Nested";
    });
    ASSERT_NE(nested, constants.end());
    EXPECT_EQ(nested->GetType(), EConstantType::Struct);
    EXPECT_EQ(nested->GetSize(), 80);
}

TEST_F(SpirVShaderBackendTest, ReflectsIntegerScalarsAndVectorsInPushConstants)
{
    ASSERT_EQ(Module.PushConstants.size(), 1);
    ExpectIntegerTypes(Module.PushConstants.front().GetConstants());
}

TEST_F(SpirVShaderBackendTest, ReflectsIntegerVectorElementsInStorageBuffers)
{
    const std::array expected{
        std::pair{"signedInput", EConstantType::Int2},
        std::pair{"unsignedInput", EConstantType::UInt4},
    };
    for (const auto& [name, type] : expected)
    {
        SCOPED_TRACE(name);
        const auto found = std::ranges::find_if(Module.StorageBuffers, [name](const auto& buffer)
        {
            return buffer.GetName() == name;
        });
        ASSERT_NE(found, Module.StorageBuffers.end());
        ASSERT_EQ(found->GetConstants().size(), 1);
        EXPECT_EQ(found->GetConstants().front().GetType(), type);
    }
}
