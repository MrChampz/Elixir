#pragma once

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
          : m_CoordinateIndex(coordinateIndex) {}

        std::string_view GetTypeName() const override { return "Material.TexCoord"; }

        SMaterialExpression Emit(const MaterialEmitContext& context) const override
        {
            return {
                context.TexCoord(m_CoordinateIndex),
                EMaterialValueType::Float2
            };
        }

    private:
        uint32_t m_CoordinateIndex;
    };
}