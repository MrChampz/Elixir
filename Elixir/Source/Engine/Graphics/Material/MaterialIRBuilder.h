#pragma once

#include <Engine/Graphics/Material/MaterialIR.h>

namespace Elixir
{
    class ELIXIR_API MaterialIRBuilder
    {
      public:
        // Builds one stage in deterministic topological order. Static switches alias
        // only their selected branch, so inactive permutations never enter the IR.
        [[nodiscard]] static SMaterialIR Build(
            const MaterialGraph& graph, EMaterialIRStage stage);
    };
}
