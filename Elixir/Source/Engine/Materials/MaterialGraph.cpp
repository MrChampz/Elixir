#include "epch.h"
#include "MaterialGraph.h"

#include <Engine/Materials/Material.h>

namespace Elixir::Materials
{
    namespace
    {
        const char* ChannelName(const EMaterialChannel channel)
        {
            switch (channel)
            {
                case EMaterialChannel::BaseColor:               return "BaseColor";
                case EMaterialChannel::Normal:                  return "Normal";
                case EMaterialChannel::Metallic:                return "Metallic";
                case EMaterialChannel::Roughness:               return "Roughness";
                case EMaterialChannel::Opacity:                 return "Opacity";
                case EMaterialChannel::Emissive:                return "Emissive";
                case EMaterialChannel::SubsurfaceColor:         return "SubsurfaceColor";
                case EMaterialChannel::AmbientOcclusion:        return "AmbientOcclusion";
                case EMaterialChannel::Specular:                return "Specular";
                case EMaterialChannel::SpecularColor:           return "SpecularColor";
                case EMaterialChannel::ClearCoat:               return "ClearCoat";
                case EMaterialChannel::ClearCoatRoughness:      return "ClearCoatRoughness";
                case EMaterialChannel::ClearCoatBottomNormal:   return "ClearCoatBottomNormal";
            }

            return "BaseColor";
        }

        std::string CoerceForChannel(
            const SMaterialExpression& expression,
            const EMaterialChannel channel
        )
        {
            const bool isScalar = channel == EMaterialChannel::Metallic ||
                channel == EMaterialChannel::Roughness ||
                channel == EMaterialChannel::Opacity ||
                channel == EMaterialChannel::AmbientOcclusion ||
                channel == EMaterialChannel::Specular ||
                channel == EMaterialChannel::ClearCoat ||
                channel == EMaterialChannel::ClearCoatRoughness;

            if (isScalar)
                return expression.ValueType == EMaterialValueType::Float
                    ? expression.Code
                    : "(" + expression.Code + ").x";

            switch (expression.ValueType)
            {
                case EMaterialValueType::Float:  return expression.Code + ".xxx";
                case EMaterialValueType::Float2: return "float3(" + expression.Code + ", 0.0)";
                case EMaterialValueType::Float3: return expression.Code;
                case EMaterialValueType::Float4: return "(" + expression.Code + ").rgb";
            }
            return expression.Code;
        }
    }

    uint32_t MaterialGraph::AddNode(Scope<MaterialNode> node)
    {
        if (!node) return 0;

        const uint32_t id = m_NextId++;
        m_Nodes.emplace(id, SGraphNode{ .Node = std::move(node) });

        return id;
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

    bool MaterialGraph::Validate(
        const MaterialNodeValidationContext& parameters,
        std::string* error
    ) const
    {
        for (const auto& [id, graphNode] : m_Nodes)
        {
            std::string nodeError;

            if (graphNode.Node->Validate(parameters, nodeError))
                continue;

            if (error)
                *error = std::string(graphNode.Node->GetTypeName()) + " node " +
                    std::to_string(id) + ": " + nodeError;

            return false;
        }

        std::unordered_set<uint32_t> visiting;
        std::unordered_set<uint32_t> visited;
        std::function<bool(uint32_t)> visit = [&](const uint32_t id)
        {
            if (visiting.contains(id))
            {
                if (error)
                    *error = "Material graph contains a cycle involving node " + std::to_string(id) + ".";

                return false;
            }

            if (visited.contains(id))
                return true;

            visiting.insert(id);

            const auto nodeIt = m_Nodes.find(id);
            if (nodeIt != m_Nodes.end())
            {
                for (const int32_t input : nodeIt->second.Inputs)
                {
                    if (input >= 0 && !visit(static_cast<uint32_t>(input)))
                        return false;
                }
            }

            visiting.erase(id);
            visited.insert(id);
            return true;
        };

        for (const auto& [id, _] : m_Nodes)
        {
            if (!visit(id))
                return false;
        }

        return true;
    }

    std::string MaterialGraph::GenerateHLSL() const
    {
        return GenerateHLSL({});
    }

    std::string MaterialGraph::GenerateHLSL(const SMaterialGraphBindings& bindings) const
    {
        std::string body;
        std::unordered_map<uint32_t, SMaterialExpression> emitted;
        std::unordered_set<uint32_t> visiting;

        for (const auto& [channel, nodeId] : m_Channels)
        {
            const auto expression = EmitNode(nodeId, emitted, visiting, body, &bindings);
            body += "   surface." + std::string(ChannelName(channel)) + " = " +
                CoerceForChannel(expression, channel) + ";\n";
        }

        return body;
    }

    const MaterialNode* MaterialGraph::FindNode(const uint32_t id) const
    {
        const auto it = m_Nodes.find(id);
        return it == m_Nodes.end() ? nullptr : it->second.Node.get();
    }

    SMaterialExpression MaterialGraph::EmitNode(
        const uint32_t id,
        std::unordered_map<uint32_t, SMaterialExpression>& emitted,
        std::unordered_set<uint32_t>& visiting,
        std::string& body,
        const SMaterialGraphBindings* bindings
    ) const
    {
        if (const auto it = emitted.find(id); it != emitted.end())
            return it->second;

        const auto it = m_Nodes.find(id);
        if (it == m_Nodes.end() || visiting.contains(id))
            return { .Code = "0.0", .ValueType = EMaterialValueType::Float };

        visiting.insert(id);
        const auto& node = it->second;
        const auto& definitions = node.Node->GetInputs();
        std::vector<SMaterialExpression> inputs;
        inputs.reserve(definitions.size());

        for (size_t slot = 0; slot < definitions.size(); ++slot)
        {
            const int32_t source = slot < node.Inputs.size() ? node.Inputs[slot] : -1;

            if (source >= 0)
                inputs.push_back(EmitNode(
                    (uint32_t)source,
                    emitted,
                    visiting,
                    body,
                    bindings
                ));
            else
                inputs.push_back({
                    .Code = definitions[slot].DefaultExpression,
                    .ValueType = definitions[slot].DefaultValueType
                });
        }

        const MaterialEmitContext context(inputs, bindings);
        SMaterialExpression expression = node.Node->Emit(context);

        const std::string variable = "n" + std::to_string(id);
        body += "   " + std::string(MaterialEmitContext::TypeName(expression.ValueType)) +
            " " + variable + " = " + expression.Code + ";\n";
        expression.Code = variable;

        visiting.erase(id);
        emitted.emplace(id, expression);

        return expression;
    }
}
