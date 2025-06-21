#include <gtest/gtest.h>
using namespace testing;

#include "Utils/TestEnvironment.h"

int main(int argc, char** argv)
{
    InitGoogleTest(&argc, argv);
    AddGlobalTestEnvironment(new TestEnvironment);
    return RUN_ALL_TESTS();
}