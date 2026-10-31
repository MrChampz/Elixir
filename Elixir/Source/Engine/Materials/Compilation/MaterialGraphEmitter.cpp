#include "epch.h"
#include "MaterialGraphEmitter.h"

#include <Engine/Materials/Material.h>
#include <Engine/Materials/Compilation/MaterialShaderSourceBuilder.h>

namespace Elixir::Materials::Compilation
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
                case EMaterialChannel::FuzzColor:               return "FuzzColor";
                case EMaterialChannel::Cloth:                   return "Cloth";
                case EMaterialChannel::FuzzRoughness:           return "FuzzRoughness";
                case EMaterialChannel::AmbientOcclusion:        return "AmbientOcclusion";
                case EMaterialChannel::Specular:                return "Specular";
                case EMaterialChannel::SpecularColor:           return "SpecularColor";
                case EMaterialChannel::ClearCoat:               return "ClearCoat";
                case EMaterialChannel::ClearCoatRoughness:      return "ClearCoatRoughness";
                case EMaterialChannel::ClearCoatBottomNormal:   return "ClearCoatBottomNormal";
                case EMaterialChannel::WorldPositionOffset:     return "WorldPositionOffset";
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
                                  channel == EMaterialChannel::Cloth ||
                                  channel == EMaterialChannel::FuzzRoughness ||
                                  channel == EMaterialChannel::ClearCoat ||
                                  channel == EMaterialChannel::ClearCoatRoughness;

            if (isScalar)
            {
                return expression.ValueType == EMaterialValueType::Float
                    ? expression.Code
                    : "(" + expression.Code + ").x";
            }

            switch (expression.ValueType)
            {
                case EMaterialValueType::Float:
                    return expression.Code + ".xxx";
                case EMaterialValueType::Float2:
                    return "float3(" + expression.Code + ", 0.0)";
                case EMaterialValueType::Float3:
                    return expression.Code;
                case EMaterialValueType::Float4:
                    return "(" + expression.Code + ").rgb";
            }

            return expression.Code;
        }
    }

    bool MaterialGraphEmitter::Validate(
        const MaterialNodeValidationContext& parameters,
        std::string* error,
        const SMaterialGraphBindings* bindings
    ) const
    {
        if (!bindings)
            bindings = &m_Bindings;

        for (const auto& id : m_Graph.m_Channels | std::views::values)
        {
            if (m_Graph.m_Nodes.contains(id))
                continue;

            if (error)
                *error = "Material graph output references a missing node.";

            return false;
        }

        for (const auto& [id, graphNode] : m_Graph.m_Nodes)
        {
            for (const auto source : graphNode.Inputs)
            {
                if (source < 0 || m_Graph.m_Nodes.contains(uint32_t(source)))
                    continue;

                if (error)
                    *error = "Material graph input references a missing node.";

                return false;
            }

            std::string nodeError;

            if (graphNode.Node->Validate(parameters, nodeError))
                continue;

            if (error)
                *error = std::string(graphNode.Node->GetTypeName()) + " node " +
                         std::to_string(id) + ": " + nodeError;

            return false;
        }

        for (const auto& [channel, id] : m_Graph.m_Channels)
        {
            const bool vertexOutput = channel == EMaterialChannel::WorldPositionOffset;
            if (vertexOutput && parameters.GetUsage() != EMaterialUsage::Surface)
            {
                if (error)
                    *error = "World position offset requires surface usage.";
                return false;
            }

            const auto stage = vertexOutput ? EShaderStage::Vertex : EShaderStage::Pixel;
            const auto supported = GetSupportedStages(id, bindings);
            if (!(supported & stage))
            {
                if (error)
                    *error = "Material output uses a node unavailable in its shader stage.";
                return false;
            }
        }

        std::unordered_set<uint32_t> visiting;
        std::unordered_set<uint32_t> visited;

        std::function<bool(uint32_t)> visit = [&](const uint32_t id)
        {
            if (visiting.contains(id))
            {
                if (error)
                    *error = "Material graph contains a cycle involving node " +
                             std::to_string(id) + ".";

                return false;
            }

            if (visited.contains(id))
                return true;

            visiting.insert(id);

            const auto nodeIt = m_Graph.m_Nodes.find(id);
            if (nodeIt != m_Graph.m_Nodes.end())
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

        for (const auto& id : m_Graph.m_Nodes | std::views::keys)
        {
            if (!visit(id))
                return false;
        }

        return true;
    }

    MaterialGraphEmitter::MaterialGraphEmitter(
        const MaterialGraph& graph,
        const SMaterialEmissionEnvironment& environment,
        const SMaterialGraphBindings& bindings
    ) : m_Graph(graph),
        m_Environment(environment),
        m_Bindings(bindings) {}

    std::string MaterialGraphEmitter::GenerateHLSL()
    {
        m_Diagnostics.clear();

        const auto stage = m_Environment.Stage;
        const auto& bindings = m_Bindings;

        if (stage != EShaderStage::Vertex && stage != EShaderStage::Pixel)
        {
            m_Diagnostics =
                "Material surface outputs require a Vertex or Pixel emission stage.";
            EE_CORE_ERROR("{}", m_Diagnostics)
            return {};
        }

        std::string body;
        std::unordered_map<uint32_t, SMaterialExpression> emitted;
        std::unordered_set<uint32_t> visiting;

        for (const auto& [channel, nodeId] : m_Graph.m_Channels)
        {
            const bool vertexOutput = channel == EMaterialChannel::WorldPositionOffset;
            if (vertexOutput != (stage == EShaderStage::Vertex))
                continue;

            const auto supported = GetSupportedStages(nodeId, &bindings);
            if (!(supported & stage))
            {
                m_Diagnostics = "Material output does not support the requested shader stage.";
                EE_CORE_ERROR("{}", m_Diagnostics)
                return {};
            }

            const auto expression = EmitNode(nodeId, emitted, visiting, body, &bindings);
            const auto destination = vertexOutput
                 ? std::string("worldPositionOffset")
                 : "surface." + std::string(ChannelName(channel));

            const auto coerced = CoerceForChannel(expression, channel);
            body += "   " + destination + " = " + coerced + ";\n";
        }

        if (!m_Diagnostics.empty())
        {
            EE_CORE_ERROR("{}", m_Diagnostics)
            return {};
        }

        return body;
    }

    SMaterialExpression MaterialGraphEmitter::GenerateExpression(const uint32_t nodeId)
    {
        m_Diagnostics.clear();

        const auto stage = m_Environment.Stage;
        const auto& bindings = m_Bindings;
        const auto supported = GetSupportedStages(nodeId, &bindings);

        if (!MaterialEmitContext::IsValidStage(stage) || !(supported & stage))
        {
            m_Diagnostics = "Material expression does not support the requested shader stage.";
            EE_CORE_ERROR("{}", m_Diagnostics)
            return { "0.0", EMaterialValueType::Float };
        }

        std::string body;
        std::unordered_map<uint32_t, SMaterialExpression> emitted;
        std::unordered_set<uint32_t> visiting;

        const auto expression = EmitNode(nodeId, emitted, visiting, body, &bindings, true);

        if (!m_Diagnostics.empty())
        {
            EE_CORE_ERROR("{}", m_Diagnostics)
            return { "0.0", EMaterialValueType::Float };
        }

        return expression;
    }

    EShaderStage MaterialGraphEmitter::GetSupportedStages(
        const uint32_t nodeId,
        const SMaterialGraphBindings* bindings
    ) const
    {
        if (!bindings)
            bindings = &m_Bindings;

        std::unordered_set<uint32_t> visited;
        auto supported = EShaderStage::All;

        std::function<void(uint32_t)> visit = [&](const uint32_t id)
        {
            const auto [_, inserted] = visited.insert(id);
            if (!inserted) return;

            const auto found = m_Graph.m_Nodes.find(id);
            if (found == m_Graph.m_Nodes.end())
            {
                supported = EShaderStage::None;
                return;
            }

            const auto nodeStages = found->second.Node->GetSupportedStages(bindings);
            supported = static_cast<EShaderStage>(uint8_t(supported) & uint8_t(nodeStages));

            const auto selected = found->second.Node->SelectInput(bindings);
            for (size_t slot = 0; slot < found->second.Inputs.size(); ++slot)
            {
                if (selected && slot != *selected)
                    continue;

                const auto source = found->second.Inputs[slot];
                if (source >= 0)
                    visit(uint32_t(source));
            }
        };

        visit(nodeId);

        return supported;
    }

    SMaterialExpression MaterialGraphEmitter::EmitNode(
        const uint32_t id,
        std::unordered_map<uint32_t, SMaterialExpression>& emitted,
        std::unordered_set<uint32_t>& visiting,
        std::string& body,
        const SMaterialGraphBindings* bindings,
        const bool inlineExpression
    )
    {
        if (const auto it = emitted.find(id); it != emitted.end())
            return it->second;

        const auto it = m_Graph.m_Nodes.find(id);
        if (it == m_Graph.m_Nodes.end() || visiting.contains(id))
        {
            if (m_Diagnostics.empty())
                m_Diagnostics = "Material emission encountered a missing node or a cycle.";
            return { .Code = "0.0", .ValueType = EMaterialValueType::Float };
        }

        visiting.insert(id);

        const auto& node = it->second;
        const auto& definitions = node.Node->GetInputs();

        std::vector<SMaterialExpression> inputs;
        inputs.reserve(definitions.size());

        const auto selected = node.Node->SelectInput(bindings);

        const MaterialEmitContext context(inputs, bindings, m_Environment, m_Diagnostics);
        for (size_t slot = 0; slot < definitions.size(); ++slot)
        {
            const int32_t source = slot < node.Inputs.size() ? node.Inputs[slot] : -1;

            if (selected && slot != *selected)
            {
                inputs.push_back({ "0.0", EMaterialValueType::Float });
                continue;
            }

            if (source >= 0)
                inputs.push_back(EmitNode(
                    (uint32_t)source,
                    emitted,
                    visiting,
                    body,
                    bindings,
                    inlineExpression
                ));
            else
                inputs.push_back(context.DefaultInput(definitions[slot]));
        }

        SMaterialExpression expression = node.Node->Emit(context);

        if (inlineExpression)
        {
            expression.Code = "(" + expression.Code + ")";
            visiting.erase(id);
            emitted.emplace(id, expression);
            return expression;
        }

        const std::string variable = "n" + std::to_string(id);
        body += "   " + std::string(MaterialEmitContext::TypeName(expression.ValueType)) +
                " " + variable + " = " + expression.Code + ";\n";
        expression.Code = variable;

        visiting.erase(id);
        emitted.emplace(id, expression);

        return expression;
    }
}
