#pragma once

#include <string>

#include <vulkan/vulkan.h>

#define VK_CHECK_RESULT(result)                                                             \
    EE_CORE_ASSERT(                                                                         \
        result == VK_SUCCESS,                                                               \
        "VkResult is \"{0}\" in file {1} at line {2}",                                      \
        Elixir::Vulkan::Utils::GetErrorString(result),                                      \
        __FILE__,                                                                           \
        __LINE__                                                                            \
    )

namespace Elixir::Vulkan
{
    namespace Utils
    {
        static std::string GetErrorString(const VkResult result)
        {
#define STR(r) case VK_##r: return #r
            switch (result)
            {
                STR(NOT_READY);
                STR(TIMEOUT);
                STR(EVENT_SET);
                STR(EVENT_RESET);
                STR(INCOMPLETE);
                STR(ERROR_OUT_OF_HOST_MEMORY);
                STR(ERROR_OUT_OF_DEVICE_MEMORY);
                STR(ERROR_INITIALIZATION_FAILED);
                STR(ERROR_DEVICE_LOST);
                STR(ERROR_MEMORY_MAP_FAILED);
                STR(ERROR_LAYER_NOT_PRESENT);
                STR(ERROR_EXTENSION_NOT_PRESENT);
                STR(ERROR_FEATURE_NOT_PRESENT);
                STR(ERROR_INCOMPATIBLE_DRIVER);
                STR(ERROR_TOO_MANY_OBJECTS);
                STR(ERROR_FORMAT_NOT_SUPPORTED);
                STR(ERROR_SURFACE_LOST_KHR);
                STR(ERROR_NATIVE_WINDOW_IN_USE_KHR);
                STR(SUBOPTIMAL_KHR);
                STR(ERROR_OUT_OF_DATE_KHR);
                STR(ERROR_INCOMPATIBLE_DISPLAY_KHR);
                STR(ERROR_VALIDATION_FAILED_EXT);
                STR(ERROR_INVALID_SHADER_NV);
                default:
                    return "UNKNOWN_ERROR";
            }
#undef STR
        }
    }

    namespace DeviceUtils
    {
        static std::string GetVendorName(const uint32_t vendorId)
		{
			char str[128];

			switch (vendorId)
			{
			    case 0x8086:
			    case 0x8087:
				    snprintf(str, sizeof(str), "Intel [0x%04x]", vendorId);
				    break;
			    case 0x1002:
			    case 0x1022:
				    snprintf(str, sizeof(str), "AMD [0x%04x]", vendorId);
				    break;
			    case 0x10DE:
				    snprintf(str, sizeof(str), "Nvidia [0x%04x]", vendorId);
				    break;
			    case 0x1EB5:
				    snprintf(str, sizeof(str), "ARM [0x%04x]", vendorId);
				    break;
			    case 0x5143:
				    snprintf(str, sizeof(str), "Qualcomm [0x%04x]", vendorId);
				    break;
			    case 0x1099:
			    case 0x10C3:
			    case 0x1249:
			    case 0x4E8:
				    snprintf(str, sizeof(str), "Samsung [0x%04x]", vendorId);
				    break;
                case 0x106b:
                case 0x05AC:
                    snprintf(str, sizeof(str), "Apple Inc. [0x%04x]", vendorId);
                    break;
			    default:
				    snprintf(str, sizeof(str), "Unknown [0x%04x]", vendorId);
				    break;
			}

			return std::string(str);
		}

		static std::string GetApiVersion(const uint32_t apiVersion)
		{
			std::stringstream ss;
			ss << VK_API_VERSION_MAJOR(apiVersion);
			ss << ".";
			ss << VK_API_VERSION_MINOR(apiVersion);
			ss << ".";
			ss << VK_API_VERSION_PATCH(apiVersion);

			return ss.str();
		}

		static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
			const VkDebugUtilsMessageSeverityFlagBitsEXT severity,
			VkDebugUtilsMessageTypeFlagsEXT type,
			const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
			void* pUserData
		)
		{
			switch (severity)
			{
			    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
				    EE_CORE_INFO("[{0}]: {1}.", type, pCallbackData->pMessage)
				    break;
			    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
				    EE_CORE_WARN("[{0}]: {1}.", type, pCallbackData->pMessage)
				    break;
			    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
				    EE_CORE_ERROR("[{0}]: {1}.", type, pCallbackData->pMessage)
				    break;
			    default:
				    EE_CORE_TRACE("[{0}]: {1}.", type, pCallbackData->pMessage)
				    break;
			}

			return VK_FALSE;
		}
    }
}