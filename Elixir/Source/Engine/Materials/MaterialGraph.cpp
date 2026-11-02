#include "epch.h"
#include "MaterialGraph.h"

namespace Elixir::Materials
{
    uint32_t MaterialGraph::AddNode(Scope<MaterialNode> node)
    {
        if (!node) return 0;

        const uint32_t id = m_NextId++;
        m_Nodes.emplace(id, SGraphNode{ .Node = std::move(node) });

        return id;
    }

    const MaterialNode* MaterialGraph::FindNode(const uint32_t id) const
    {
        const auto it = m_Nodes.find(id);
        return it == m_Nodes.end() ? nullptr : it->second.Node.get();
    }

    void MaterialGraph::Connect(
        const uint32_t fromNode,
        const uint32_t toNode,
        const uint32_t toSlot
    )
    {
        const auto it = m_Nodes.find(toNode);

        if (it == m_Nodes.end() || toSlot >= it->second.Node->GetInputs().size())
            return;

        if (it->second.Inputs.size() <= toSlot)
            it->second.Inputs.resize(toSlot + 1, -1);

        it->second.Inputs[toSlot] = (int32_t)fromNode;
    }

    void MaterialGraph::SetChannel(const EMaterialChannel channel, const uint32_t nodeId)
    {
        m_Channels[channel] = nodeId;
    }
}
