#include "AnimationTestNodes.h"
#include "AnimationTestCharacter.h"
#include "AnimationGraphTestDriver.h"

using namespace Elixir;

Ref<AnimationGraph> CreateTestGraph(SAnimationGraphTestInfo info)
{
    auto output = BlendNode::Create(info.Name, std::move(info.Layers));
    if (!output) return nullptr;
    return AnimationGraph::Create({.Name = std::move(info.Name),
        .Parameters = std::move(info.Parameters), .Output = MakeNode(std::move(output))});
}

const std::vector<SBlendNodeInput>& GetTestGraphInputs(const AnimationGraph& graph)
{
    return static_cast<const BlendNode&>(*graph.GetOutput()).GetInputs();
}

AnimationGraphTestDriver::AnimationGraphTestDriver(Ref<const AnimationGraph> controller,
    AnimationTestCharacter& mesh) : m_Mesh(mesh), m_Controller(std::move(controller))
{
    Reset();
}

bool AnimationGraphTestDriver::SetFloat(const std::string_view name, const float value)
{
    return m_Animation && m_Animation->GetParameters().SetFloat(name, value);
}

bool AnimationGraphTestDriver::SetBool(const std::string_view name, const bool value)
{
    return m_Animation && m_Animation->GetParameters().SetBool(name, value);
}

std::optional<float> AnimationGraphTestDriver::GetFloat(const std::string_view name) const
{
    return m_Animation ? m_Animation->GetParameters().GetFloat(name) : std::nullopt;
}

std::optional<bool> AnimationGraphTestDriver::GetBool(const std::string_view name) const
{
    return m_Animation ? m_Animation->GetParameters().GetBool(name) : std::nullopt;
}

bool AnimationGraphTestDriver::Update(const float dt)
{
    return m_Animation && m_Animation->Update(dt);
}

void AnimationGraphTestDriver::Reset()
{
    if (!m_Controller) return;
    auto animation = m_Controller->CreateAnimation(m_Mesh.GetMesh()->GetData().ReferencePose);
    if (!m_Mesh.ReplaceAnimation(animation)) return;
    m_Animation = std::move(animation);
}

std::string_view AnimationGraphTestDriver::GetCurrentState(const std::string_view name) const
{
    return m_Animation ? m_Animation->GetCurrentState(name) : std::string_view{};
}
