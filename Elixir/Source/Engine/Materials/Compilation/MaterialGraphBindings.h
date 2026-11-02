#pragma once

#include <string>
#include <array>
#include <unordered_map>

#include <Engine/Materials/MaterialNode.h>

namespace Elixir::Materials::Compilation
{
    /** @brief Maps material parameter names to generated HLSL expressions. */
    struct SMaterialGraphBindings
    {
        std::unordered_map<std::string, std::string> Values;

        std::unordered_map<std::string, std::string> Textures;

        /** Compile-time choices shared by nested function calls. */
        std::unordered_map<std::string, bool> StaticValues;

        /** Arguments of the current function call. */
        std::unordered_map<std::string, SMaterialExpression> FunctionInputs;
    };
}
