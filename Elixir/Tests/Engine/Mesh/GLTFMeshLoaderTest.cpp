#include <gtest/gtest.h>

#include <Platform/GLTF/GLTFMeshLoader.h>
#include <Engine/Graphics/GraphicsContext.h>
#include <Engine/Mesh/SkeletalMeshInstance.h>
#include <Engine/Materials/DefaultMaterials.h>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <cstring>
#include <fstream>

using namespace Elixir;

namespace
{
    class ImportTestContext final : public GraphicsContext
    {
    public:
        ImportTestContext() : GraphicsContext(EGraphicsAPI::Vulkan, nullptr) {}

        void Init() override {}
        void Shutdown() override {}
        void ProcessEvent(Event&) override {}
        void RenderFrame(std::function<void()>) override {}
        void DrainRenderQueue() override {}
        bool EnqueueRenderTask(std::function<void()> task) const override { task(); return true; }
        bool RunRenderTaskAndWait(std::function<void()> task) const override { task(); return true; }
        bool IsRenderThread() const override { return true; }
        bool IsFrameRecording() const override { return false; }
        void WaitForSubmittedFrames() const override {}
        void SetClearColor(const glm::vec4&) override {}
        void Resize(Extent2D) override {}
        Ref<CommandBuffer> GetSecondaryCommandBuffer() const override { return nullptr; }
        Ref<CommandBuffer> GetUploadCommandBuffer() const override { return nullptr; }
        void EnqueueSecondaryCommandBuffer(const Ref<CommandBuffer>&) const override {}
        Extent3D GetSwapchainExtent() const override { return {}; }
        uint64_t GetMaxStorageBufferRange() const override { return 0; }
        glm::uvec3 GetMaxComputeWorkGroupCount() const override { return {0, 0, 0}; }

    private:
        void ClearImage(const Ref<Image>&) override {}
        void CreateRenderTargets() override {}
    };

    class GLTFMeshLoaderTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            Directory = std::filesystem::temp_directory_path() /
                ("ElixirSkeletal-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            ASSERT_TRUE(std::filesystem::create_directory(Directory));
            Bytes.resize(276);
            const float positions[] = { 2, 1, 0, 3, 1, 0, 2, 2, 0 };
            const uint16_t joints[12]{};
            const float weights[] = { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0 };
            const uint16_t indices[] = { 0, 1, 2 };
            const glm::mat4 bind[] = {
                glm::translate(glm::mat4(1), glm::vec3(-2, -1, 0)),
                glm::translate(glm::mat4(1), glm::vec3(-2, 0, 0)),
            };
            const float times[] = { 0, 2 };
            const float translations[] = { 0, 1, 0, 0, 3, 0 };
            std::memcpy(Bytes.data(), positions, sizeof(positions));
            std::memcpy(Bytes.data() + 36, joints, sizeof(joints));
            std::memcpy(Bytes.data() + 60, weights, sizeof(weights));
            std::memcpy(Bytes.data() + 108, indices, sizeof(indices));
            std::memcpy(Bytes.data() + 116, bind, sizeof(bind));
            std::memcpy(Bytes.data() + 244, times, sizeof(times));
            std::memcpy(Bytes.data() + 252, translations, sizeof(translations));
            Json = R"({
                "asset":{"version":"2.0"},
                "buffers":[{"uri":"skin.bin","byteLength":276}],
                "bufferViews":[
                    {"buffer":0,"byteOffset":0,"byteLength":36},
                    {"buffer":0,"byteOffset":36,"byteLength":24},
                    {"buffer":0,"byteOffset":60,"byteLength":48},
                    {"buffer":0,"byteOffset":108,"byteLength":6},
                    {"buffer":0,"byteOffset":116,"byteLength":128},
                    {"buffer":0,"byteOffset":244,"byteLength":8},
                    {"buffer":0,"byteOffset":252,"byteLength":24}
                ],
                "accessors":[
                    {"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[2,1,0],"max":[3,2,0]},
                    {"bufferView":1,"componentType":5123,"count":3,"type":"VEC4"},
                    {"bufferView":2,"componentType":5126,"count":3,"type":"VEC4"},
                    {"bufferView":3,"componentType":5123,"count":3,"type":"SCALAR"},
                    {"bufferView":4,"componentType":5126,"count":2,"type":"MAT4"},
                    {"bufferView":5,"componentType":5126,"count":2,"type":"SCALAR","min":[0],"max":[2]},
                    {"bufferView":6,"componentType":5126,"count":2,"type":"VEC3"}
                ],
                "meshes":[{"primitives":[{"attributes":{"POSITION":0,"JOINTS_0":1,"WEIGHTS_0":2},"indices":3}]}],
                "nodes":[
                    {"name":"Root","translation":[2,0,0],"children":[1]},
                    {"name":"Hand","translation":[0,1,0]},
                    {"mesh":0,"skin":0}
                ],
                "skins":[{"joints":[1,0],"inverseBindMatrices":4}],
                "animations":[{"name":"RaiseHand","samplers":[{"input":5,"output":6,"interpolation":"LINEAR"}],
                    "channels":[{"sampler":0,"target":{"node":1,"path":"translation"}}]}],
                "scenes":[{"nodes":[0,2]}],"scene":0
            })";
        }

        void TearDown() override
        {
            std::error_code error;
            std::filesystem::remove_all(Directory, error);
        }

        void Replace(const std::string& from, const std::string& to)
        {
            const auto position = Json.find(from);
            ASSERT_NE(position, std::string::npos);
            Json.replace(position, from.size(), to);
        }

        std::filesystem::path Write(const bool glb = false)
        {
            const auto path = Directory / (glb ? "skin.glb" : "skin.gltf");
            if (!glb)
            {
                std::ofstream data(Directory / "skin.bin", std::ios::binary);
                data.write(Bytes.data(), std::streamsize(Bytes.size()));
                std::ofstream source(path);
                source << Json;
                return path;
            }
            Replace("\"uri\":\"skin.bin\",", "");
            while (Json.size() % 4) Json += ' ';
            std::ofstream file(path, std::ios::binary);
            const auto writeWord = [&](const uint32_t value) { file.write(reinterpret_cast<const char*>(&value), sizeof(value)); };
            writeWord(0x46546C67); writeWord(2); writeWord(uint32_t(12 + 8 + Json.size() + 8 + Bytes.size()));
            writeWord(uint32_t(Json.size())); writeWord(0x4E4F534A); file.write(Json.data(), std::streamsize(Json.size()));
            writeWord(uint32_t(Bytes.size())); writeWord(0x004E4942); file.write(Bytes.data(), std::streamsize(Bytes.size()));
            return path;
        }

        Ref<SkeletalMesh> CreateMesh(SSkeletalMeshData data)
        {
            data.Materials = { Materials::CreateDefaultMaterials()[size_t(Materials::EMaterialUsage::Surface)] };
            return SkeletalMesh::Create(std::move(data));
        }

        ImportTestContext Context;
        GLTFMeshLoader Loader;
        std::filesystem::path Directory;
        std::vector<char> Bytes;
        std::string Json;
    };
}

TEST_F(GLTFMeshLoaderTest, ImportsSeparateAssetsAndDeformsFromAnExternalBuffer)
{
    auto source = Loader.LoadSkeletalMesh(Context, Write());
    ASSERT_TRUE(source);
    ASSERT_EQ(source->Animations.size(), 1);
    ASSERT_EQ(source->Skeleton->GetBones().size(), 2);
    EXPECT_EQ(source->BoneMap, (std::vector<uint32_t>{ 1, 0 }));
    EXPECT_FALSE(source->Materials.empty());
    const auto animation = source->Animations[0];
    auto mesh = CreateMesh(std::move(*source));
    ASSERT_TRUE(mesh);
    EXPECT_EQ(mesh->GetSkeleton(), animation->GetSkeleton());
    ASSERT_EQ(mesh->GetAnimations().size(), 1);
    SkeletalMeshInstance meshInstance(mesh);
    std::vector<SStaticMeshVertex> vertices;
    ASSERT_TRUE(mesh->SkinVertices(meshInstance.GetModelPose(), vertices));
    EXPECT_NEAR(vertices[0].Position.y, 1, 1e-5f);
    ASSERT_TRUE(meshInstance.Play(animation));
    ASSERT_TRUE(meshInstance.Update(1));
    ASSERT_TRUE(mesh->SkinVertices(meshInstance.GetModelPose(), vertices));
    EXPECT_NEAR(vertices[0].Position.x, 2, 1e-5f);
    EXPECT_NEAR(vertices[0].Position.y, 2, 1e-5f);
    EXPECT_NEAR(glm::length(vertices[0].Normal), 1, 1e-5f);
    EXPECT_NEAR(glm::length(glm::vec3(vertices[0].Tangent)), 1, 1e-5f);
}

TEST_F(GLTFMeshLoaderTest, ImportsBinaryGLB)
{
    auto source = Loader.LoadSkeletalMesh(Context, Write(true));
    ASSERT_TRUE(source);
    EXPECT_EQ(source->Vertices.size(), 3);
    ASSERT_EQ(source->Animations.size(), 1);
    EXPECT_FLOAT_EQ(source->Animations[0]->GetDuration(), 2);
}

TEST_F(GLTFMeshLoaderTest, ReusesAnExplicitCompatibleSkeleton)
{
    auto path = Write();
    auto first = Loader.LoadSkeletalMesh(Context, path);
    ASSERT_TRUE(first);
    auto second = Loader.LoadSkeletalMesh(Context, path, first->Skeleton);
    ASSERT_TRUE(second);
    EXPECT_EQ(second->Skeleton, first->Skeleton);
    auto incompatible = Skeleton::Create("Other", {{ .Name = "Other" }});
    EXPECT_FALSE(Loader.LoadSkeletalMesh(Context, path, incompatible));
}

TEST_F(GLTFMeshLoaderTest, RejectsScenesWithoutSkinsAndMissingFiles)
{
    Replace(",\"skin\":0", "");
    auto path = Write();
    EXPECT_FALSE(Loader.LoadSkeletalMesh(Context, path));
    EXPECT_FALSE(Loader.LoadSkeletalMesh(Context, Directory / "missing.glb"));
}

TEST_F(GLTFMeshLoaderTest, RejectsAdditionalSkinInfluencesExplicitly)
{
    Replace("\"JOINTS_0\":1", "\"JOINTS_0\":1,\"JOINTS_1\":1");
    EXPECT_FALSE(Loader.LoadSkeletalMesh(Context, Write()));
}

TEST_F(GLTFMeshLoaderTest, RejectsMismatchedSkinAttributeCounts)
{
    Replace("\"bufferView\":1,\"componentType\":5123,\"count\":3", "\"bufferView\":1,\"componentType\":5123,\"count\":2");
    EXPECT_FALSE(Loader.LoadSkeletalMesh(Context, Write()));
}

TEST_F(GLTFMeshLoaderTest, RejectsCyclicHierarchy)
{
    Replace("\"name\":\"Hand\",\"translation\":[0,1,0]", "\"name\":\"Hand\",\"translation\":[0,1,0],\"children\":[0]");
    EXPECT_FALSE(Loader.LoadSkeletalMesh(Context, Write()));
}

TEST_F(GLTFMeshLoaderTest, RejectsCubicAnimationExplicitly)
{
    Replace("\"interpolation\":\"LINEAR\"", "\"interpolation\":\"CUBICSPLINE\"");
    EXPECT_FALSE(Loader.LoadSkeletalMesh(Context, Write()));
}

TEST_F(GLTFMeshLoaderTest, MissingInverseBindMatricesUseIdentity)
{
    Replace(",\"inverseBindMatrices\":4", "");
    auto source = Loader.LoadSkeletalMesh(Context, Write());
    ASSERT_TRUE(source);
    EXPECT_EQ(source->InverseBindMatrices[0], glm::mat4(1));
}

TEST_F(GLTFMeshLoaderTest, SupportsStepAnimation)
{
    Replace("\"interpolation\":\"LINEAR\"", "\"interpolation\":\"STEP\"");
    auto source = Loader.LoadSkeletalMesh(Context, Write());
    ASSERT_TRUE(source);
    std::vector<SBoneTransform> pose;
    ASSERT_TRUE(source->Animations[0]->Sample(1, pose));
    EXPECT_FLOAT_EQ(pose[1].Translation.y, 1);
    ASSERT_TRUE(source->Animations[0]->Sample(2, pose));
    EXPECT_FLOAT_EQ(pose[1].Translation.y, 3);
}

TEST_F(GLTFMeshLoaderTest, RejectsTruncatedAccessorStorage)
{
    Replace("\"byteOffset\":60,\"byteLength\":48", "\"byteOffset\":60,\"byteLength\":4");
    EXPECT_FALSE(Loader.LoadSkeletalMesh(Context, Write()));
}

TEST_F(GLTFMeshLoaderTest, RejectsInvalidAnimationSamplerReferences)
{
    Replace("\"sampler\":0", "\"sampler\":99");
    EXPECT_FALSE(Loader.LoadSkeletalMesh(Context, Write()));
}

TEST_F(GLTFMeshLoaderTest, RejectsBufferViewsOutsideLoadedBytes)
{
    Replace("\"byteOffset\":252,\"byteLength\":24", "\"byteOffset\":252,\"byteLength\":100");
    EXPECT_FALSE(Loader.LoadSkeletalMesh(Context, Write()));
}

TEST_F(GLTFMeshLoaderTest, CombinesAllMeshNodesIntoOneAsset)
{
    Replace("{\"mesh\":0,\"skin\":0}", "{\"mesh\":0,\"skin\":0},{\"mesh\":0,\"skin\":0}");
    Replace("\"nodes\":[0,2]", "\"nodes\":[0,2,3]");
    auto source = Loader.LoadSkeletalMesh(Context, Write());
    ASSERT_TRUE(source);
    ASSERT_EQ(source->Sections.size(), 2);
    ASSERT_EQ(source->Vertices.size(), 6);
    EXPECT_EQ(source->BoneMap, (std::vector<uint32_t>{ 1, 0, 1, 0 }));
    EXPECT_EQ(source->Vertices[3].Joints.x, 2);
    EXPECT_EQ(source->Indices, (std::vector<uint32_t>{ 0, 1, 2, 0, 1, 2 }));
    EXPECT_EQ(source->Vertices[2].Position, glm::vec3(2, 2, 0));
    EXPECT_EQ(source->Sections[1].VertexOffset, 3);
    EXPECT_EQ(source->Sections[1].FirstIndex, 3);
    auto mesh = CreateMesh(std::move(*source));
    ASSERT_TRUE(mesh);
    EXPECT_EQ(mesh->GetSections().size(), 2);
    EXPECT_EQ(mesh->GetAnimations().size(), 1);
    SkeletalMeshInstance meshInstance(mesh);
    ASSERT_TRUE(meshInstance.Play(mesh->GetAnimations()[0]));
    ASSERT_TRUE(meshInstance.Update(1));
    std::vector<SStaticMeshVertex> vertices;
    ASSERT_TRUE(mesh->SkinVertices(meshInstance.GetModelPose(), vertices));
    EXPECT_NEAR(vertices[0].Position.y, 2, 1e-5f);
    EXPECT_EQ(vertices[0].Position, vertices[3].Position);
    EXPECT_EQ(mesh->GetLocalBounds().Min, glm::vec3(2, 1, 0));
    EXPECT_EQ(mesh->GetLocalBounds().Max, glm::vec3(3, 2, 0));
}

TEST_F(GLTFMeshLoaderTest, CombinesDifferentSkinsWithTheirOwnPaletteOrder)
{
    Replace("{\"mesh\":0,\"skin\":0}", "{\"mesh\":0,\"skin\":0},{\"mesh\":0,\"skin\":1}");
    Replace("\"nodes\":[0,2]", "\"nodes\":[0,2,3]");
    Replace("\"skins\":[{\"joints\":[1,0],\"inverseBindMatrices\":4}]",
        "\"skins\":[{\"joints\":[1,0],\"inverseBindMatrices\":4},{\"joints\":[0,1]}]");
    auto source = Loader.LoadSkeletalMesh(Context, Write());
    ASSERT_TRUE(source);
    ASSERT_EQ(source->Sections.size(), 2);
    EXPECT_EQ(source->BoneMap, (std::vector<uint32_t>{ 1, 0, 0, 1 }));
    EXPECT_EQ(source->InverseBindMatrices[2], glm::mat4(1));
    auto mesh = CreateMesh(std::move(*source));
    ASSERT_TRUE(mesh);
    SkeletalMeshInstance meshInstance(mesh);
    std::vector<SStaticMeshVertex> vertices;
    ASSERT_TRUE(mesh->SkinVertices(meshInstance.GetModelPose(), vertices));
    EXPECT_EQ(vertices[0].Position, glm::vec3(2, 1, 0));
    EXPECT_EQ(vertices[3].Position, glm::vec3(4, 1, 0));
    EXPECT_EQ(mesh->GetLocalBounds().Max.x, 5);
}

TEST_F(GLTFMeshLoaderTest, IncludesRigidMeshAttachmentsInTheModel)
{
    Replace("{\"mesh\":0,\"skin\":0}", "{\"mesh\":0,\"skin\":0},{\"name\":\"Attachment\",\"mesh\":0,\"translation\":[5,0,0]}");
    Replace("\"nodes\":[0,2]", "\"nodes\":[0,2,3]");
    auto source = Loader.LoadSkeletalMesh(Context, Write());
    ASSERT_TRUE(source);
    ASSERT_EQ(source->Sections.size(), 2);
    EXPECT_GE(source->Skeleton->FindBone("Attachment"), 0);
    auto mesh = CreateMesh(std::move(*source));
    ASSERT_TRUE(mesh);
    SkeletalMeshInstance meshInstance(mesh);
    std::vector<SStaticMeshVertex> vertices;
    ASSERT_TRUE(mesh->SkinVertices(meshInstance.GetModelPose(), vertices));
    EXPECT_EQ(vertices[3].Position, glm::vec3(7, 1, 0));
}

TEST_F(GLTFMeshLoaderTest, ExcludesMeshNodesOutsideTheSelectedScene)
{
    Replace("{\"mesh\":0,\"skin\":0}", "{\"mesh\":0,\"skin\":0},{\"mesh\":0,\"skin\":0}");
    auto source = Loader.LoadSkeletalMesh(Context, Write());
    ASSERT_TRUE(source);
    EXPECT_EQ(source->Sections.size(), 1);
}

TEST_F(GLTFMeshLoaderTest, RejectsJointIndicesOutsideEachIndividualSkin)
{
    Replace("{\"mesh\":0,\"skin\":0}", "{\"mesh\":0,\"skin\":0},{\"mesh\":0,\"skin\":0}");
    Replace("\"nodes\":[0,2]", "\"nodes\":[0,2,3]");
    const uint16_t invalidJoint = 2;
    std::memcpy(Bytes.data() + 36, &invalidJoint, sizeof(invalidJoint));
    EXPECT_FALSE(Loader.LoadSkeletalMesh(Context, Write()));
}

TEST_F(GLTFMeshLoaderTest, OneLoaderImportsBothTypedResultsFromTheSameSource)
{
    const auto path = Write();
    const MeshLoader& loader = Loader;
    auto staticMesh = loader.LoadStaticMesh(Context, path);
    auto skeletalMesh = loader.LoadSkeletalMesh(Context, path);
    ASSERT_TRUE(staticMesh);
    ASSERT_TRUE(skeletalMesh);
    EXPECT_EQ(staticMesh->Sections.size(), skeletalMesh->Sections.size());
    EXPECT_EQ(staticMesh->Indices, skeletalMesh->Indices);
    EXPECT_EQ(staticMesh->Vertices.size(), skeletalMesh->Vertices.size());
    EXPECT_EQ(staticMesh->Materials.size(), skeletalMesh->Materials.size());
    EXPECT_EQ(staticMesh->Vertices[0].Position, skeletalMesh->Vertices[0].Position);
    EXPECT_NEAR(glm::length(staticMesh->Vertices[0].Normal), 1, 1e-5f);
    EXPECT_NEAR(glm::length(glm::vec3(staticMesh->Vertices[0].Tangent)), 1, 1e-5f);
}

TEST_F(GLTFMeshLoaderTest, StaticImportWorksWithoutASkinnedNode)
{
    Replace(",\"skin\":0", "");
    const auto path = Write();
    auto mesh = Loader.LoadStaticMesh(Context, path);
    ASSERT_TRUE(mesh);
    EXPECT_EQ(mesh->Sections.size(), 1);
    EXPECT_EQ(mesh->Indices, (std::vector<uint32_t>{ 0, 1, 2 }));
    EXPECT_EQ(mesh->LocalBounds.Min, glm::vec3(2, 1, 0));
    EXPECT_EQ(mesh->LocalBounds.Max, glm::vec3(3, 2, 0));
    EXPECT_FALSE(Loader.LoadSkeletalMesh(Context, path));
}

TEST_F(GLTFMeshLoaderTest, StaticImportBakesNodeTransformsAndBounds)
{
    Replace("{\"mesh\":0,\"skin\":0}", "{\"mesh\":0,\"skin\":0,\"translation\":[5,2,1]}");
    auto mesh = Loader.LoadStaticMesh(Context, Write());
    ASSERT_TRUE(mesh);
    EXPECT_EQ(mesh->Vertices[0].Position, glm::vec3(7, 3, 1));
    EXPECT_EQ(mesh->LocalBounds.Min, glm::vec3(7, 3, 1));
    EXPECT_EQ(mesh->LocalBounds.Max, glm::vec3(8, 4, 1));
}

TEST_F(GLTFMeshLoaderTest, StaticImportPreservesWindingUnderReflection)
{
    Replace("{\"mesh\":0,\"skin\":0}", "{\"mesh\":0,\"skin\":0,\"scale\":[-1,1,1]}");
    auto mesh = Loader.LoadStaticMesh(Context, Write());
    ASSERT_TRUE(mesh);
    EXPECT_EQ(mesh->Indices, (std::vector<uint32_t>{ 0, 2, 1 }));
    EXPECT_EQ(mesh->LocalBounds.Min, glm::vec3(-3, 1, 0));
    EXPECT_EQ(mesh->LocalBounds.Max, glm::vec3(-2, 2, 0));
    EXPECT_FLOAT_EQ(mesh->Vertices[0].Tangent.w, -1);
}

TEST_F(GLTFMeshLoaderTest, StaticImportCombinesSectionsAndUsesTheSelectedScene)
{
    Replace("{\"mesh\":0,\"skin\":0}", "{\"mesh\":0,\"skin\":0},{\"mesh\":0,\"skin\":0,\"translation\":[5,0,0]}");
    Replace("\"nodes\":[0,2]", "\"nodes\":[0,2,3]");
    auto mesh = Loader.LoadStaticMesh(Context, Write());
    ASSERT_TRUE(mesh);
    ASSERT_EQ(mesh->Sections.size(), 2);
    EXPECT_EQ(mesh->Sections[1].FirstIndex, 3);
    EXPECT_EQ(mesh->Sections[1].VertexOffset, 3);
    EXPECT_EQ(mesh->LocalBounds.Min, glm::vec3(2, 1, 0));
    EXPECT_EQ(mesh->LocalBounds.Max, glm::vec3(8, 2, 0));
}

TEST_F(GLTFMeshLoaderTest, StaticImportRejectsMalformedStorageAndMissingSources)
{
    Replace("\"byteOffset\":60,\"byteLength\":48", "\"byteOffset\":60,\"byteLength\":4");
    EXPECT_FALSE(Loader.LoadStaticMesh(Context, Write()));
    EXPECT_FALSE(Loader.LoadStaticMesh(Context, Directory / "missing.gltf"));
}

TEST_F(GLTFMeshLoaderTest, CopiesBothUvChannelsIntoIndependentVertexTypes)
{
    Bytes.resize(324);
    const float uv0[] = { 0.25f, 0.5f, 0.75f, 0.5f, 0.25f, 1.0f };
    const float uv1[] = { 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f };
    std::memcpy(Bytes.data() + 276, uv0, sizeof(uv0));
    std::memcpy(Bytes.data() + 300, uv1, sizeof(uv1));
    Replace("\"byteLength\":276", "\"byteLength\":324");
    Replace("{\"buffer\":0,\"byteOffset\":252,\"byteLength\":24}",
        "{\"buffer\":0,\"byteOffset\":252,\"byteLength\":24},"
        "{\"buffer\":0,\"byteOffset\":276,\"byteLength\":24},"
        "{\"buffer\":0,\"byteOffset\":300,\"byteLength\":24}");
    Replace("{\"bufferView\":6,\"componentType\":5126,\"count\":2,\"type\":\"VEC3\"}",
        "{\"bufferView\":6,\"componentType\":5126,\"count\":2,\"type\":\"VEC3\"},"
        "{\"bufferView\":7,\"componentType\":5126,\"count\":3,\"type\":\"VEC2\"},"
        "{\"bufferView\":8,\"componentType\":5126,\"count\":3,\"type\":\"VEC2\"}");
    Replace("\"WEIGHTS_0\":2", "\"WEIGHTS_0\":2,\"TEXCOORD_0\":7,\"TEXCOORD_1\":8");
    const auto path = Write();
    auto staticMesh = Loader.LoadStaticMesh(Context, path);
    auto skeletalMesh = Loader.LoadSkeletalMesh(Context, path);
    ASSERT_TRUE(staticMesh);
    ASSERT_TRUE(skeletalMesh);
    ASSERT_EQ(skeletalMesh->Vertices.size(), 3);
    for (size_t i = 0; i < skeletalMesh->Vertices.size(); ++i)
    {
        EXPECT_EQ(skeletalMesh->Vertices[i].TexCoord, glm::vec2(uv0[i * 2], uv0[i * 2 + 1]));
        EXPECT_EQ(skeletalMesh->Vertices[i].TexCoord1, glm::vec2(uv1[i * 2], uv1[i * 2 + 1]));
        EXPECT_EQ(skeletalMesh->Vertices[i].TexCoord, staticMesh->Vertices[i].TexCoord);
        EXPECT_EQ(skeletalMesh->Vertices[i].TexCoord1, staticMesh->Vertices[i].TexCoord1);
    }
}
