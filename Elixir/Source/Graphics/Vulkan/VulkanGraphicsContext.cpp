#include "epch.h"
#include "VulkanGraphicsContext.h"

#include <Graphics/SpirV/SpirVShaderBackend.h>

#include "Converters.h"
#include "Utils.h"

#include <VkBootstrap.h>
#include <vulkan/vulkan.h>

#define VMA_IMPLEMENTATION
#include "VulkanCommandBuffer.h"

#include <vk_mem_alloc.h>

#include "GLFW/glfw3.h"

namespace Elixir
{
    using namespace Vulkan;
    using namespace Vulkan::Converters;
    using namespace SpirV;

    template <typename T>
    void LogError(vkb::Result<T> result, std::string preMessage = "")
    {
        if (!result)
        {
            const auto message = preMessage + result.error().message();
            EE_CORE_FATAL(message)
        }
    }

    /* SDescriptorAllocator */

    void SDescriptorAllocator::InitPool(
        const VkDevice device,
        const uint32_t maxSets,
        std::span<SPoolSizeRatio> ratios
    )
    {
        EE_PROFILE_ZONE_SCOPED()

        std::vector<VkDescriptorPoolSize> poolSizes;
        poolSizes.reserve(ratios.size());

        for (auto& [Type, Ratio] : ratios)
        {
            VkDescriptorPoolSize size;
            size.type = Type;
            size.descriptorCount = (uint32_t)(Ratio * maxSets);

            poolSizes.emplace_back(size);
        }

        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets = maxSets;
        poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

        vkCreateDescriptorPool(device, &poolInfo, nullptr, &Pool);
    }

    void SDescriptorAllocator::Reset(const VkDevice device) const
    {
        EE_PROFILE_ZONE_SCOPED()
        vkResetDescriptorPool(device, Pool, 0);
    }

    void SDescriptorAllocator::DestroyPool(const VkDevice device) const
    {
        EE_PROFILE_ZONE_SCOPED()
        vkDestroyDescriptorPool(device, Pool, nullptr);
    }

    VkDescriptorSet SDescriptorAllocator::Allocate(
        const VkDevice device,
        const VkDescriptorSetLayout layout
    ) const
    {
        EE_PROFILE_ZONE_SCOPED()

        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.pNext = nullptr;
        allocInfo.descriptorPool = Pool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &layout;

        VkDescriptorSet set;
        VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &set));

        return set;
    }

    /* VulkanGraphicsContext */

    VulkanGraphicsContext::VulkanGraphicsContext(const EGraphicsAPI api, const Window* window)
        : GraphicsContext(api, window)
    {
        EE_PROFILE_ZONE_SCOPED()
        EE_CORE_ASSERT(window, "Invalid window!")

        glfwGetFramebufferSize(
            (GLFWwindow*)m_Window->GetNativeWindow(),
            (int*)&m_WindowExtent.Width,
            (int*)&m_WindowExtent.Height
        );

        m_ShaderBackend = CreateScope<SpirVShaderBackend>();
    }

    VulkanGraphicsContext::~VulkanGraphicsContext()
    {
        EE_PROFILE_ZONE_SCOPED()
        Shutdown();
    }

    void VulkanGraphicsContext::Init()
    {
        EE_PROFILE_ZONE_SCOPED()

        InitVulkan();
        InitAllocator();
        InitSwapchain();
        InitCommands();
        InitSyncStructures();
        InitDescriptors();
        CreateRenderTarget();

        SetClearColor({ 0.0f, 0.0f, 0.0f, 1.0f });

        EE_CORE_INFO("Vulkan Renderer:")
		EE_CORE_INFO("  Vendor: {0}", DeviceUtils::GetVendorName(m_GPUProperties.vendorID));
		EE_CORE_INFO("  Renderer: {0}", m_GPUProperties.deviceName)
		EE_CORE_INFO("  Version: {0}", DeviceUtils::GetApiVersion(m_GPUProperties.apiVersion));

        m_IsInitialized = true;
    }

    void VulkanGraphicsContext::Shutdown()
    {
        EE_PROFILE_ZONE_SCOPED()

        if (m_IsInitialized)
        {
            vkDeviceWaitIdle(m_Device);

            m_DeletionQueue.Flush();
            m_CommandBuffers.clear();

            DestroySwapchain();

            m_GlobalDescriptorAllocator.DestroyPool(m_Device);

            for (int i = 0; i < FRAMES; i++)
            {
                vkDestroyFence(m_Device, m_Frames[i].RenderFence, nullptr);
                vkDestroySemaphore(m_Device, m_Frames[i].RenderSemaphore, nullptr);
                vkDestroySemaphore(m_Device, m_Frames[i].SwapchainSemaphore, nullptr);
            }

            vkDestroyDevice(m_Device, nullptr);

            vkb::destroy_debug_utils_messenger(m_Instance, m_DebugMessenger);
            vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
            vkDestroyInstance(m_Instance, nullptr);

            m_IsInitialized = false;
        }
    }

    void VulkanGraphicsContext::Prepare()
    {
        EE_PROFILE_ZONE_SCOPED()

        auto& frame = GetCurrentFrame();
        const auto cmd = std::static_pointer_cast<VulkanCommandBuffer>(GetCommandBuffer());

        if (m_SwapchainRecreateRequested)
            RecreateSwapchain();

        // Wait until the gpu has finished rendering the last frame. Timeout of 1 second.
        auto result = vkWaitForFences(
            m_Device,
            1,
            &frame.RenderFence,
            true,
            1000000000
        );
        VK_CHECK_RESULT(result);

        frame.DeletionQueue.Flush();

        VK_CHECK_RESULT(vkResetFences(m_Device, 1, &frame.RenderFence));

        result = vkAcquireNextImageKHR(
            m_Device,
            m_Swapchain,
            1000000000,
            frame.SwapchainSemaphore,
            nullptr,
            &m_CurrentSwapchainImageIndex
        );

        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            m_SwapchainRecreateRequested = true;
            return;
        }

        VK_CHECK_RESULT(result);

        VK_CHECK_RESULT(vkResetCommandBuffer(cmd->GetVulkanCommandBuffer(), 0));

        cmd->Begin();

        const auto& image = m_SwapchainImages[m_CurrentSwapchainImageIndex].Image;

        CommandUtils::TransitionImage(
            cmd->GetVulkanCommandBuffer(),
            image,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_ASPECT_COLOR_BIT
        );
    }

    void VulkanGraphicsContext::Submit()
    {
        EE_PROFILE_ZONE_SCOPED()

        // // set swapchain image layout to Present so we can show it on the screen
        // vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        //
        // //finalize the command buffer (we can no longer add commands, but it can now be executed)
        // VK_CHECK(vkEndCommandBuffer(cmd));
        //
        // VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);
        //
        // VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,get_current_frame()._swapchainSemaphore);
        // VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,get_current_frame(). _renderSemaphore);
        //
        // VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo,&signalInfo,&waitInfo);
        //
        // //submit command buffer to the queue and execute it.
        // // _renderFence will now block until the graphic commands finish execution
        // VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, get_current_frame()._renderFence));
    }

    void VulkanGraphicsContext::Present()
    {
        EE_PROFILE_ZONE_SCOPED()

        const auto& frame = GetCurrentFrame();
        const auto cmd = std::static_pointer_cast<VulkanCommandBuffer>(GetCommandBuffer());

        const auto& image = m_SwapchainImages[m_CurrentSwapchainImageIndex].Image;

        CommandUtils::TransitionImage(
            cmd->GetVulkanCommandBuffer(),
            image,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_IMAGE_ASPECT_COLOR_BIT
        );

        cmd->End();
        cmd->Submit(frame.SwapchainSemaphore, frame.RenderSemaphore, frame.RenderFence);

        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.pNext = nullptr;
        presentInfo.pSwapchains = &m_Swapchain;
        presentInfo.swapchainCount = 1;
        presentInfo.pWaitSemaphores = &frame.RenderSemaphore;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pImageIndices = &m_CurrentSwapchainImageIndex;

        const auto result = vkQueuePresentKHR(m_GraphicsQueue, &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            m_SwapchainRecreateRequested = true;
            return;
        }

        VK_CHECK_RESULT(result);

        m_FrameNumber++;
    }

    void VulkanGraphicsContext::WaitDeviceIdle()
    {
        EE_PROFILE_ZONE_SCOPED()
        VK_CHECK_RESULT(vkDeviceWaitIdle(m_Device));
    }

    void VulkanGraphicsContext::SetClearColor(const glm::vec4& color)
    {
        EE_PROFILE_ZONE_SCOPED()
        m_ClearColor = {{ color.r, color.g, color.b, color.a }};
    }

    void VulkanGraphicsContext::Clear()
    {
        EE_PROFILE_ZONE_SCOPED()

        const auto cmd = std::static_pointer_cast<VulkanCommandBuffer>(GetCommandBuffer());

        const auto range = Initializers::ImageSubresourceRange(EImageAspect::Color);
        vkCmdClearColorImage(
            cmd->GetVulkanCommandBuffer(),
            m_SwapchainImages[m_CurrentSwapchainImageIndex].Image,
            VK_IMAGE_LAYOUT_GENERAL,
            &m_ClearColor,
            1,
            &range
        );
    }

    void VulkanGraphicsContext::Resize(const Extent2D extent)
    {
        EE_PROFILE_ZONE_SCOPED()
        m_WindowExtent = extent;
        RecreateSwapchain();
    }

    void VulkanGraphicsContext::FlushCommandBuffer(CommandBuffer& cmd) const
    {
        EE_PROFILE_ZONE_SCOPED()
        cmd.Flush();
    }

    Extent2D VulkanGraphicsContext::GetSwapchainExtent() const
    {
        return { m_SwapchainExtent.width, m_SwapchainExtent.height };
    }

    void VulkanGraphicsContext::InitVulkan()
    {
        EE_PROFILE_ZONE_SCOPED()

        vkb::InstanceBuilder builder;

        auto instanceResult = builder
            .set_app_name("Elixir Engine")
            .request_validation_layers(m_UseValidationLayers)
            .add_debug_messenger_severity(VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
            .set_debug_callback(DeviceUtils::DebugCallback)
            .require_api_version(1, 3, 0)
            .build();

        LogError(instanceResult, "Vulkan instance creation failed: ");

        const vkb::Instance vkbInstance = instanceResult.value();
        m_Instance = vkbInstance.instance;
        m_DebugMessenger = vkbInstance.debug_messenger;

        VK_CHECK_RESULT(
            glfwCreateWindowSurface(
                m_Instance,
                static_cast<GLFWwindow*>(m_Window->GetNativeWindow()),
                nullptr,
                &m_Surface
            )
        );

        // Vulkan 1.3 features.
		VkPhysicalDeviceVulkan13Features features13{};
		features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
		features13.dynamicRendering = true;
		features13.synchronization2 = true;

		// Vulkan 1.2 features.
		VkPhysicalDeviceVulkan12Features features12{};
		features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		features12.bufferDeviceAddress = true;
		features12.descriptorIndexing = true;

        vkb::PhysicalDeviceSelector selector{ vkbInstance };
		auto physicalDeviceResult = selector
			.set_minimum_version(1, 3)
			.set_required_features_13(features13)
			.set_required_features_12(features12)
			.set_surface(m_Surface)
			.select();

        LogError(physicalDeviceResult, "Vulkan physical device selection failed: ");

        vkb::PhysicalDevice gpu = physicalDeviceResult.value();

		vkb::DeviceBuilder deviceBuilder{ gpu };
        auto deviceResult = deviceBuilder.build();

        LogError(deviceResult, "Vulkan device creation failed: ");

		vkb::Device vkbDevice = deviceResult.value();

		m_Device = vkbDevice.device;
		m_GPU = gpu.physical_device;
		m_GPUProperties = gpu.properties;

		auto graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics);
        LogError(graphicsQueue);
        m_GraphicsQueue = graphicsQueue.value();

        auto graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics);
        LogError(graphicsQueueFamily);
		m_GraphicsQueueFamily = graphicsQueueFamily.value();
    }

    void VulkanGraphicsContext::InitAllocator()
    {
        EE_PROFILE_ZONE_SCOPED()

        VmaAllocatorCreateInfo info = {};
        info.physicalDevice = m_GPU;
        info.device = m_Device;
        info.instance = m_Instance;
        info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

        vmaCreateAllocator(&info, &m_Allocator);

        m_DeletionQueue.Push([&]()
        {
            vmaDestroyAllocator(m_Allocator);
        });
    }

    void VulkanGraphicsContext::InitSwapchain()
    {
        EE_PROFILE_ZONE_SCOPED()
        CreateSwapchain(m_WindowExtent);
    }

    void VulkanGraphicsContext::InitCommands()
    {
        EE_PROFILE_ZONE_SCOPED()

        for (auto it = m_CommandBuffers.begin(); it != m_CommandBuffers.end(); ++it)
        {
            *it = CreateCommandBuffer();
        }
    }

    void VulkanGraphicsContext::InitSyncStructures()
    {
        EE_PROFILE_ZONE_SCOPED()

        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.pNext = nullptr;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VkSemaphoreCreateInfo semaphoreInfo = {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphoreInfo.pNext = nullptr;

        for (int i = 0; i < FRAMES; i++)
        {
            VK_CHECK_RESULT(
                vkCreateFence(
                    m_Device,
                    &fenceInfo,
                    nullptr,
                    &m_Frames[i].RenderFence
                )
            );

            VK_CHECK_RESULT(
                vkCreateSemaphore(
                    m_Device,
                    &semaphoreInfo,
                    nullptr,
                    &m_Frames[i].SwapchainSemaphore
                )
            );

            VK_CHECK_RESULT(
                vkCreateSemaphore(
                    m_Device,
                    &semaphoreInfo,
                    nullptr,
                    &m_Frames[i].RenderSemaphore
                )
            );
        }
    }

    void VulkanGraphicsContext::InitDescriptors()
    {
        EE_PROFILE_ZONE_SCOPED()

        std::vector<SDescriptorAllocator::SPoolSizeRatio> sizes =
        {
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0.5 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 0.25 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0.25 }
        };

        m_GlobalDescriptorAllocator.InitPool(m_Device, 10, sizes);
    }

    void VulkanGraphicsContext::CreateSwapchain(const Extent2D& extent)
    {
        EE_PROFILE_ZONE_SCOPED()

        m_SwapchainImageFormat = VK_FORMAT_R8G8B8A8_UNORM;
        constexpr auto colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

        vkb::SwapchainBuilder builder{ m_GPU, m_Device, m_Surface };

        auto result = builder
            .set_desired_format({ .format = m_SwapchainImageFormat, .colorSpace = colorSpace })
            .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
            .set_desired_extent(extent.Width, extent.Height)
            .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .build();

        LogError(result, "Swapchain creation failed: ");

        m_Swapchain = result.value().swapchain;
        m_SwapchainExtent = result.value().extent;

        const auto images = result.value().get_images().value();
        const auto views = result.value().get_image_views().value();

        uint32_t index = 0;
        for (const auto image : images)
        {
            m_SwapchainImages[index] = { .Image = image, .View = views[index] };
            index++;
        }
    }

    void VulkanGraphicsContext::DestroySwapchain()
    {
        EE_PROFILE_ZONE_SCOPED()

        for (const auto& [_, view] : m_SwapchainImages)
        {
            vkDestroyImageView(m_Device, view, nullptr);
        }

        vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
    }

    void VulkanGraphicsContext::RecreateSwapchain()
    {
        EE_PROFILE_ZONE_SCOPED()

        WaitDeviceIdle();
        DestroySwapchain();
        CreateSwapchain(m_WindowExtent);
        m_SwapchainRecreateRequested = false;
    }

    void VulkanGraphicsContext::CreateRenderTarget()
    {
        int width, height;
        glfwGetFramebufferSize((GLFWwindow*)m_Window->GetNativeWindow(), &width, &height);
        //m_RenderTarget = Texture2D::Create(this, EImageFormat::R8G8B8A8_UNORM, width, height);
    }
}