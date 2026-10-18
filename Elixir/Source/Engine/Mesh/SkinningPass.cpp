#include "epch.h"
#include "SkinningPass.h"

#include <Engine/Mesh/StaticMesh.h>
#include <Engine/Graphics/Pipeline/Pipeline.h>
#include <Engine/Graphics/Shader/ShaderLoader.h>

namespace Elixir
{
    SkinningPass::SkinningPass(
        const GraphicsContext& context,
        const ShaderLoader& shaderLoader
    ) : m_FrameBuffers(context),
        m_Context(context)
    {
        m_Shader = shaderLoader.LoadShader(
            "./Shaders/Mesh/", "Skinning",
            EShaderStage::Compute
        );

        if (!m_Shader)
        {
            EE_CORE_ERROR("GPU skinning shader could not be loaded.")
            return;
        }

        m_Pipeline = ComputePipeline::Create(&context, { .Shader = m_Shader });
    }

    bool SkinningPass::IsValid() const
    {
        return m_Shader && m_Pipeline;
    }

    bool SkinningPass::EnsureBuffer(Ref<DynamicStorageBuffer>& buffer, const size_t bytes)
    {
        if (buffer && buffer->IsValid() && buffer->GetSize() >= bytes)
            return true;

        const auto limit = std::min<uint64_t>(
            m_Context.GetMaxStorageBufferRange(),
            UINT32_MAX / 2
        );

        if (bytes == 0 || bytes > limit)
        {
            EE_CORE_ERROR("GPU skinning upload exceeds the supported buffer size.")
            return false;
        }

        size_t capacity = 256;
        while (capacity < bytes) capacity *= 2;
        capacity = std::min<size_t>(capacity, limit);

        buffer = DynamicStorageBuffer::Create(&m_Context, capacity);

        return buffer && buffer->IsValid();
    }

    bool SkinningPass::Record(
        const Ref<CommandBuffer>& cmd,
        const SkinningBatch& batch,
        const Ref<StorageBuffer>& source,
        const Ref<StorageBuffer>& output,
        const EPipelineStage dstStage,
        const EPipelineAccess dstAccess
    )
    {
        if (!IsValid() || !cmd || !source || !output || !source->IsValid() || !output->IsValid())
        {
            EE_CORE_ERROR("GPU skinning requires a pipeline, commands, and valid geometry buffers.")
            return false;
        }

        if (batch.GetBlocks().empty()) return true;

        const auto maxWorkGroupCount = m_Context.GetMaxComputeWorkGroupCount();
        if (maxWorkGroupCount.x == 0 || maxWorkGroupCount.y == 0 || maxWorkGroupCount.z == 0)
        {
            EE_CORE_ERROR("GPU skinning requires nonzero compute workgroup limits.")
            return false;
        }

        const auto rangeLimit = m_Context.GetMaxStorageBufferRange();
        if (source->GetSize() > rangeLimit || output->GetSize() > rangeLimit)
        {
            EE_CORE_ERROR("GPU skinning geometry exceeds the device storage-buffer range; use smaller geometry pool capacities.")
            return false;
        }

        for (const auto& job : batch.GetJobs())
        {
            const auto sourceEndByteOffset = (uint64_t(job.SourceVertexOffset) + job.VertexCount)
                * sizeof(SSkeletalMeshVertex);
            const auto outputEndByteOffset = (uint64_t(job.OutputVertexOffset) + job.VertexCount)
                * sizeof(SStaticMeshVertex);

            if (sourceEndByteOffset > source->GetSize() ||
                outputEndByteOffset > output->GetSize())
            {
                EE_CORE_ERROR("GPU skinning job exceeds its geometry buffers.")
                return false;
            }
        }

        auto& frame = m_FrameBuffers.GetCurrent();

        const auto matrixBytes = batch.GetMatrices().size() * sizeof(glm::mat4);
        const auto jobBytes = batch.GetJobs().size() * sizeof(SSkinningJob);
        const auto blockBytes = batch.GetBlocks().size() * sizeof(SSkinningBlock);

        if (!EnsureBuffer(frame.Matrices, matrixBytes) ||
            !EnsureBuffer(frame.Jobs, jobBytes) ||
            !EnsureBuffer(frame.Blocks, blockBytes))
            return false;

        frame.Matrices->UpdateData(batch.GetMatrices().data(), matrixBytes);
        frame.Jobs->UpdateData(batch.GetJobs().data(), jobBytes);
        frame.Blocks->UpdateData(batch.GetBlocks().data(), blockBytes);

        m_Shader->BindStorageBuffer("sourceVertices", source);
        m_Shader->BindStorageBuffer("boneMatrices", frame.Matrices);
        m_Shader->BindStorageBuffer("jobs", frame.Jobs);
        m_Shader->BindStorageBuffer("blocks", frame.Blocks);
        m_Shader->BindStorageBuffer("outputVertices", output);

        source->Barrier(cmd, EPipelineStage::ComputeShader, EPipelineAccess::ShaderRead);
        output->Barrier(cmd, EPipelineStage::ComputeShader, EPipelineAccess::ShaderWrite);

        m_Pipeline->Bind(cmd);

        for (uint32_t first = 0; first < batch.GetBlocks().size();)
        {
            m_Shader->SetPushConstant(cmd, "pc", &first, sizeof(first));

            const auto count = std::min(
                maxWorkGroupCount.x,
                (uint32_t)batch.GetBlocks().size() - first
            );

            cmd->Dispatch(count);
            first += count;
        }

        output->Barrier(cmd, dstStage, dstAccess);

        return true;
    }
}
