#pragma once

#include "glm/ext/scalar_uint_sized.hpp"

#include <Engine/Materials/Material.h>
#include <Engine/Materials/MaterialNode.h>

namespace Elixir::Materials::Nodes
{
    /**
     * @brief Outputs one texture-coordinate channel of a static mesh.
     */
    class TexCoord final : public MaterialNode
    {
    public:
        /**
         * @brief Creates a texture-coordinate node.
         * @param coordinateIndex Zero-based texture-coordinate channel.
         */
        explicit TexCoord(const uint32_t coordinateIndex = 0)
          : m_CoordinateIndex(std::min(coordinateIndex, 1u)) {}

        std::string_view GetTypeName() const override { return "Material.TexCoord"; }

        SMaterialExpression Emit(const MaterialEmitContext& context) const override
        {
            return {
                .Code = m_CoordinateIndex == 0
                    ? "input.TexCoord"
                    : "input.TexCoord1",
                .ValueType = EMaterialValueType::Float2
            };
        }

    private:
        uint32_t m_CoordinateIndex;
    };
}