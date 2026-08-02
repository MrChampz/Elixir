#include "epch.h"
#include "MaterialRenderScene.h"

namespace Elixir
{
    void MaterialRenderScene::Add(SMaterialRenderItem item)
    {
        m_Items.push_back(std::move(item));
    }
}
