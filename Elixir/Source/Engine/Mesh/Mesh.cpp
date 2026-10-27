#include "epch.h"
#include "Mesh.h"

#include <Engine/Graphics/BufferLayout.h>

namespace Elixir
{
    const BufferLayout& GetMeshVertexLayout()
    {
        static const BufferLayout layout = {{
            { EDataType::Float3, "Position"  },
            { EDataType::Float3, "Normal"    },
            { EDataType::Float4, "Tangent"   },
            { EDataType::Float2, "TexCoord"  },
            { EDataType::Float2, "TexCoord1" },
        }};
        return layout;
    }
}
