#include <gtest/gtest.h>

#include <Engine/Mesh/Sorter.h>
#include <glm/gtc/matrix_transform.hpp>

using namespace Elixir;

TEST(SectionSorterTest, UsesTheCenterOfTheSectionBounds)
{
    const SMeshSection section{
        .FirstIndex = 18,
        .IndexCount = 12,
        .VertexOffset = 50,
        .LocalBounds = {.Min = {-2, -4, -8}, .Max = {2, 4, -4}},
    };
    SectionSorter implementation;
    Sorter& sorter = implementation;
    sorter.SetContext({});

    EXPECT_FLOAT_EQ(sorter.CalculateDepth(section), 6.0f);
}

TEST(SectionSorterTest, CombinesModelScaleAndCameraOrientation)
{
    const SMeshSection section{
        .LocalBounds = {.Min = {1, -1, -1}, .Max = {3, 1, 1}},
    };
    const auto model = glm::translate(glm::mat4(1), glm::vec3(4, 2, 3)) *
        glm::scale(glm::mat4(1), glm::vec3(2, 1, 1));
    const auto view = glm::lookAt(glm::vec3(10, 2, 3), glm::vec3(9, 2, 3), glm::vec3(0, 1, 0));
    SectionSorter sorter;
    sorter.SetContext({.Model = model, .View = view});
    EXPECT_FLOAT_EQ(sorter.CalculateDepth(section), 2.0f);
}

TEST(SectionSorterTest, KeepsSignedDepthForSectionsBehindTheCamera)
{
    const SMeshSection section{
        .LocalBounds = {.Min = {-1, -1, 2}, .Max = {1, 1, 4}},
    };
    SectionSorter sorter;
    sorter.SetContext({});

    EXPECT_FLOAT_EQ(sorter.CalculateDepth(section), -3.0f);
}

TEST(SectionSorterTest, MeasuresViewDepthRatherThanDistanceFromTheCamera)
{
    const SMeshSection section{
        .LocalBounds = {.Min = {-1, -1, -12}, .Max = {1, 1, -8}},
    };
    SectionSorter sorter;
    const auto shifted = glm::translate(glm::mat4(1), glm::vec3(1000, -500, 0));

    sorter.SetContext({.Model = shifted});
    EXPECT_FLOAT_EQ(sorter.CalculateDepth(section), 10.0f);
    sorter.SetContext({});
    EXPECT_FLOAT_EQ(sorter.CalculateDepth(section), 10.0f);
}

TEST(SectionSorterTest, HandlesReflectedModelTransforms)
{
    const SMeshSection section{
        .LocalBounds = {.Min = {-1, -1, -8}, .Max = {1, 1, -4}},
    };
    const auto model = glm::translate(glm::mat4(1), glm::vec3(0, 0, -5)) *
        glm::scale(glm::mat4(1), glm::vec3(1, 1, -2));
    SectionSorter sorter;
    sorter.SetContext({.Model = model});
    EXPECT_FLOAT_EQ(sorter.CalculateDepth(section), -7.0f);
}

TEST(SectionSorterTest, AcceptsPointBounds)
{
    const SMeshSection section{
        .LocalBounds = {.Min = {0, 0, -12}, .Max = {0, 0, -12}},
    };
    SectionSorter sorter;
    sorter.SetContext({});

    EXPECT_FLOAT_EQ(sorter.CalculateDepth(section), 12.0f);
}

TEST(SectionSorterTest, ReusesContextForSectionsAndReplacesItForTheNextMesh)
{
    const SMeshSection first{
        .LocalBounds = {.Min = {-1, -1, -1}, .Max = {1, 1, 1}},
    };
    const SMeshSection second{
        .LocalBounds = {.Min = {-1, -1, -2}, .Max = {1, 1, 0}},
    };
    SectionSorter implementation;
    Sorter& sorter = implementation;

    sorter.SetContext({
        .Model = glm::translate(glm::mat4(1), glm::vec3(0, 0, -4)),
        .View = glm::translate(glm::mat4(1), glm::vec3(0, 0, -3)),
    });
    EXPECT_FLOAT_EQ(sorter.CalculateDepth(first), 7.0f);
    EXPECT_FLOAT_EQ(sorter.CalculateDepth(second), 8.0f);

    sorter.SetContext({
        .Model = glm::translate(glm::mat4(1), glm::vec3(0, 0, 5)),
        .View = glm::translate(glm::mat4(1), glm::vec3(0, 0, 2)),
    });
    EXPECT_FLOAT_EQ(sorter.CalculateDepth(first), -7.0f);
    EXPECT_FLOAT_EQ(sorter.CalculateDepth(second), -6.0f);

    sorter.SetContext({});
    EXPECT_FLOAT_EQ(sorter.CalculateDepth(first), 0.0f);
}
