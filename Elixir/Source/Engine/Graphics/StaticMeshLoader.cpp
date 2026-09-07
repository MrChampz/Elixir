#include "epch.h"
#include "StaticMeshLoader.h"

namespace Elixir
{
    bool SStaticMeshLoadResult::HasErrors() const
    {
        return std::ranges::any_of(Diagnostics, [](const auto& diagnostic)
        {
            return diagnostic.Severity == EStaticMeshLoadDiagnosticSeverity::Error;
        });
    }
}
