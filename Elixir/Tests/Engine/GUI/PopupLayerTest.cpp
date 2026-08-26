#include <gtest/gtest.h>
using namespace testing;

#include "ManagerTestUtils.h"

#include <Engine/GUI/VerticalBox.h>
#include <Engine/GUI/Renderer/RenderBatch.h>
using namespace Elixir;
using namespace Elixir::GUI;

namespace
{
    // Leaf that emits a single rect tagged with `color` at its own geometry - lets a test
    // locate which layer's command a given draw came from. Same idiom as
    // DrawCacheTest.cpp's LayeredWidget, kept local since PopupLayerTest only needs one rect
    // per widget (no z-band spanning).
    class ProbeWidget final : public Widget
    {
      public:
        explicit ProbeWidget(const SColor& color) : m_Color(color) {}

        glm::vec2 ComputeDesiredSize(const glm::vec2&) override { return { 20.0f, 20.0f }; }

      protected:
        void BuildDrawCommands(RenderBatch& batch, const int zOrder) override
        {
            batch.AddRect(m_Geometry, m_Color, glm::vec4(0.0f), glm::vec4(0.0f), glm::vec4(0.0f), SOutline{}, zOrder);
        }

      private:
        SColor m_Color;
    };

    // Minimal container that clips its children to its own geometry - used to check that a
    // clip established on one layer does not leak into another (each layer starts
    // CollectDrawCommands with the invalid sentinel clip; see Manager::AssembleFrame).
    class ClippingBox final : public TPanel<LayoutSlot>
    {
      public:
        glm::vec2 ComputeDesiredSize(const glm::vec2&) override { return {}; }

      protected:
        bool ClipsChildren() const override { return true; }
    };

    const SDrawCommand* FindByColor(const std::vector<SDrawCommand>& commands, const SColor& color)
    {
        for (const auto& cmd : commands)
        {
            if (cmd.Color == color)
                return &cmd;
        }
        return nullptr;
    }
}

TEST(PopupLayerTest, PopupCountStartsAtZeroAfterSetRoot)
{
    const auto root = CreateRef<VerticalBox>();

    Manager manager; // no promotion needed: SetRoot/GetPopupCount/PushPopup/PopPopup/ClearPopups are public
    manager.SetRoot(root);

    EXPECT_EQ(manager.GetPopupCount(), 0u);
}

TEST(PopupLayerTest, PushAndPopPopupTracksCount)
{
    const auto root = CreateRef<VerticalBox>();
    const auto popup = CreateRef<VerticalBox>();

    Manager manager;
    manager.SetRoot(root);

    manager.PushPopup(popup, { { 0, 0 }, { 10, 10 } });
    EXPECT_EQ(manager.GetPopupCount(), 1u);

    manager.PopPopup();
    EXPECT_EQ(manager.GetPopupCount(), 0u);
}

TEST(PopupLayerTest, PopPopupOnEmptyStackIsNoOp)
{
    const auto root = CreateRef<VerticalBox>();

    Manager manager;
    manager.SetRoot(root);
    ASSERT_EQ(manager.GetPopupCount(), 0u);

    // Only the root layer exists; PopPopup must never touch it (see SLayer's doc comment).
    manager.PopPopup();
    EXPECT_EQ(manager.GetPopupCount(), 0u);
}

TEST(PopupLayerTest, ClearPopupsZeroesCountWithMultiplePopupsStacked)
{
    const auto root = CreateRef<VerticalBox>();

    Manager manager;
    manager.SetRoot(root);

    manager.PushPopup(CreateRef<VerticalBox>(), { { 0, 0 }, { 10, 10 } });
    manager.PushPopup(CreateRef<VerticalBox>(), { { 0, 0 }, { 10, 10 } });
    manager.PushPopup(CreateRef<VerticalBox>(), { { 0, 0 }, { 10, 10 } });
    ASSERT_EQ(manager.GetPopupCount(), 3u);

    manager.ClearPopups();
    EXPECT_EQ(manager.GetPopupCount(), 0u);
}

TEST(PopupLayerTest, PushPopupArrangesImmediatelyAgainstLastExtent)
{
    const auto root = CreateRef<VerticalBox>();
    const auto popup = CreateRef<ProbeWidget>(SColor(1.0f, 0.0f, 0.0f, 1.0f));

    Manager manager;
    manager.SetRoot(root);
    manager.ArrangeLayout({ 800, 600 }); // establishes the extent PushPopup arranges against

    manager.PushPopup(popup, { { 100, 100 }, { 0, 0 } });

    // Documented behavior: "Arranged immediately against the last extent ArrangeLayout ran
    // with", so the popup already has real geometry before any further ArrangeLayout call.
    EXPECT_NE(popup->GetGeometry().Size, glm::vec2(0.0f, 0.0f));
}

// The central guarantee behind the whole popup-layer feature: a popup must always draw above
// the root, regardless of what either one contains. Same technique as DrawCacheTest.cpp's
// SiblingSubtreesGetDisjointOrderedZBands - tag each layer's content with a distinct color and
// compare the ZOrder the assembled batch actually gave it.
TEST(PopupLayerTest, PopupCommandsHaveStrictlyHigherZOrderThanRootCommands)
{
    const auto root = CreateRef<VerticalBox>();
    const SColor rootColor(1.0f, 0.0f, 0.0f, 1.0f);
    root->AddChild(CreateRef<ProbeWidget>(rootColor));

    const auto popup = CreateRef<ProbeWidget>(SColor(0.0f, 1.0f, 0.0f, 1.0f));

    TestGUIManager manager;
    manager.SetRoot(root);
    manager.ArrangeLayout({ 800, 600 });
    manager.PushPopup(popup, { { 0, 0 }, { 0, 0 } });

    manager.AssembleFrame();

    const auto& commands = manager.GetRenderBatch().GetCommands();
    const auto* rootCmd = FindByColor(commands, rootColor);
    const auto* popupCmd = FindByColor(commands, SColor(0.0f, 1.0f, 0.0f, 1.0f));

    ASSERT_NE(rootCmd, nullptr);
    ASSERT_NE(popupCmd, nullptr);
    EXPECT_GT(popupCmd->ZOrder, rootCmd->ZOrder);
}

// Each layer's CollectDrawCommands walk starts from the invalid sentinel clip (see
// Manager::AssembleFrame), so a clipping container in one layer must never narrow another
// layer's commands.
TEST(PopupLayerTest, ClipOnOneLayerDoesNotLeakIntoAnother)
{
    const auto root = CreateRef<ClippingBox>();
    const SColor rootColor(1.0f, 0.0f, 0.0f, 1.0f);
    root->AddChild(CreateRef<ProbeWidget>(rootColor));

    const SColor popupColor(0.0f, 1.0f, 0.0f, 1.0f);
    const auto popup = CreateRef<ProbeWidget>(popupColor);

    TestGUIManager manager;
    manager.SetRoot(root);
    manager.ArrangeLayout({ 20, 20 }); // small root -> a leaked clip would be obviously tiny
    manager.PushPopup(popup, { { 0, 0 }, { 0, 0 } });

    manager.AssembleFrame();

    const auto& commands = manager.GetRenderBatch().GetCommands();
    const auto* popupCmd = FindByColor(commands, popupColor);

    ASSERT_NE(popupCmd, nullptr);
    EXPECT_FALSE(popupCmd->ScissorRect.IsValid())
        << "the root layer's clip must not apply to the popup layer";
}
