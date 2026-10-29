#include <gtest/gtest.h>
#include <Engine/Camera/ArcBallCameraController.h>

#include <limits>

using namespace Elixir;

TEST(ArcBallCameraControllerTest, ElevatedOrbitKeepsItsTargetAndDistance)
{
    ArcBallCameraController controller(60, 16.0f / 9.0f);
    const glm::vec3 target(2, 1, 3);
    controller.SetTarget(target);
    controller.SetDistance(10);
    controller.SetOrbitAngles(-90, 45);
    const auto position = controller.GetCamera().GetPosition();
    EXPECT_EQ(controller.GetTarget(), target);
    EXPECT_FLOAT_EQ(controller.GetDistance(), 10);
    EXPECT_NEAR(glm::length(position - target), 10, 1e-5f);
    EXPECT_NEAR(position.x, target.x, 1e-5f);
    EXPECT_GT(position.y, target.y);
    EXPECT_LT(position.z, target.z);
}

TEST(ArcBallCameraControllerTest, InvalidAnglesPreserveTheCurrentView)
{
    ArcBallCameraController controller(60, 16.0f / 9.0f);
    controller.SetOrbitAngles(-65, 40);
    const auto position = controller.GetCamera().GetPosition();
    controller.SetOrbitAngles(std::numeric_limits<float>::quiet_NaN(), 30);
    EXPECT_EQ(controller.GetCamera().GetPosition(), position);
    controller.SetOrbitAngles(0, std::numeric_limits<float>::infinity());
    EXPECT_EQ(controller.GetCamera().GetPosition(), position);
}
