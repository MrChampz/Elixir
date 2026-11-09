#pragma once

#include <Engine/Materials/Material.h>
#include <Engine/Materials/Compilation/MaterialEmitContext.h>

namespace Elixir::Materials::Nodes
{
    /** @brief Reads the background behind the current geometry, with an optional UV offset. */
    class SceneColor final : public MaterialNode
    {
      public:
        /** @brief Creates a background sample with an offset measured in normalized UVs. */
        SceneColor()
            : MaterialNode(
                  {{"Offset",
                    EMaterialValueType::Float2,
                    "float2(0, 0)",
                    EMaterialValueType::Float2}}
              )
        {
        }
        /** @copydoc MaterialNode::GetTypeName */
        std::string_view GetTypeName() const override { return "Material.SceneColor"; }
        /** @copydoc MaterialNode::GetSupportedStages */
        EShaderStage GetSupportedStages(const SMaterialGraphBindings* bindings) const override
        {
            return EShaderStage::Pixel;
        }
        /** @copydoc MaterialNode::Emit */
        SMaterialExpression Emit(const MaterialEmitContext& context) const override
        {
            return {context.SceneColor(context.Input(0)), EMaterialValueType::Float4};
        }
    };
}
