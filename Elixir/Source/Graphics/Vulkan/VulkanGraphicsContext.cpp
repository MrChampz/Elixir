#include "epch.h"
#include "VulkanGraphicsContext.h"

#include "Converters.h"
#include "Utils.h"
#include "VkBootstrap.h"

#include "GLFW/glfw3.h"

namespace Elixir
{
    using namespace Vulkan;
    using namespace Vulkan::Converters;

    void SDescriptorAllocator::InitPool(
        const VkDevice device,
        const uint32_t maxSets,
        std::span<PoolSizeRatio> ratios
    )
    {
        EE_PROFILE_ZONE_SCOPED()

        std::vector<VkDescriptorPoolSize> poolSizes(ratios.size());
        for (auto& [Type, Ratio] : ratios)
        {
            VkDescriptorPoolSize size;
            size.type = Type;
            size.descriptorCount = static_cast<uint32_t>(Ratio * maxSets);

            poolSizes.push_back(size);
        }

        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets = maxSets;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();

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

    VulkanGraphicsContext::VulkanGraphicsContext(const Window* window)
        : GraphicsContext(window)
    {
        EE_PROFILE_ZONE_SCOPED()
        EE_CORE_ASSERT(window, "Invalid window!")

        glfwGetFramebufferSize(
            static_cast<GLFWwindow*>(m_Window->GetNativeWindow()),
            reinterpret_cast<int*>(&m_SwapchainExtent.width),
            reinterpret_cast<int*>(&m_SwapchainExtent.height)
        );
    }

    VulkanGraphicsContext::~VulkanGraphicsContext()
    {
        EE_PROFILE_ZONE_SCOPED()
        VulkanGraphicsContext::Shutdown();
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

            for (int i = 0; i < FRAMES; i++)
            {
                vkDestroyFence(m_Device, m_Frames[i].RenderFence, nullptr);
                vkDestroySemaphore(m_Device, m_Frames[i].RenderSemaphore, nullptr);
                vkDestroySemaphore(m_Device, m_Frames[i].SwapchainSemaphore, nullptr);
            }

            DestroySwapchain();

            vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
            vkDestroyDevice(m_Device, nullptr);

            vkb::destroy_debug_utils_messenger(m_Instance, m_DebugMessenger);

            vkDestroyInstance(m_Instance, nullptr);

            m_IsInitialized = false;
        }
    }

    void VulkanGraphicsContext::Prepare()
    {
        EE_PROFILE_ZONE_SCOPED()
    }

    void VulkanGraphicsContext::Submit()
    {
        EE_PROFILE_ZONE_SCOPED()
    }

    void VulkanGraphicsContext::Present()
    {
        EE_PROFILE_ZONE_SCOPED()
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
    }

    void VulkanGraphicsContext::Resize(const Extent2D extent)
    {
        EE_PROFILE_ZONE_SCOPED()
        m_WindowExtent = GetExtent2D(extent);
        RecreateSwapchain();
    }

    void VulkanGraphicsContext::FlushCommandBuffer(CommandBuffer& cmd) const
    {
        EE_PROFILE_ZONE_SCOPED()
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
            .set_debug_callback(DeviceUtils::DebugCallback)
            .require_api_version(1, 3, 0)
            .build();

        if (!instanceResult)
        {
            std::string error = "Vulkan instance creation failed: ";
            error += instanceResult.error().message();
            EE_CORE_FATAL(error)
        }

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
		vkb::PhysicalDevice gpu = selector
			.set_minimum_version(1, 3)
			.set_required_features_13(features13)
			.set_required_features_12(features12)
			.set_surface(m_Surface)
			.select()
			.value();

		vkb::DeviceBuilder deviceBuilder{ gpu };
        auto deviceResult = deviceBuilder.build();

        if (!deviceResult)
        {
            std::string error = "Vulkan device creation failed: ";
            error += deviceResult.error().message();
            EE_CORE_FATAL(error)
        }

		vkb::Device vkbDevice = deviceResult.value();

		m_Device = vkbDevice.device;
		m_GPU = gpu.physical_device;
		m_GPUProperties = gpu.properties;

		m_GraphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
		m_GraphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
    }

    void VulkanGraphicsContext::InitAllocator()
    {
        EE_PROFILE_ZONE_SCOPED()
    }

    void VulkanGraphicsContext::InitSwapchain()
    {
        EE_PROFILE_ZONE_SCOPED()
    }

    void VulkanGraphicsContext::InitCommands()
    {
        EE_PROFILE_ZONE_SCOPED()
    }

    void VulkanGraphicsContext::InitSyncStructures()
    {
        EE_PROFILE_ZONE_SCOPED()
    }

    void VulkanGraphicsContext::InitDescriptors()
    {
        EE_PROFILE_ZONE_SCOPED()
    }

    void VulkanGraphicsContext::CreateSwapchain(uint32_t width, uint32_t height)
    {
        EE_PROFILE_ZONE_SCOPED()
    }

    void VulkanGraphicsContext::DestroySwapchain()
    {
        EE_PROFILE_ZONE_SCOPED()
    }

    void VulkanGraphicsContext::RecreateSwapchain()
    {
        EE_PROFILE_ZONE_SCOPED()
    }
}