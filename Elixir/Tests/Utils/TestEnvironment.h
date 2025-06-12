#include <gtest/gtest.h>

#include <Engine/Logging/Log.h>
using namespace Elixir;

class TestEnvironment : public ::testing::Environment
{
  public:
    void SetUp() override
    {
        std::cout << "Setting up global environment...\n";
        Log::Init();
    }
};