#ifndef __vk_h__
#define __vk_h__

#include "vulkan/vulkan.hpp"

#define VK_RESULT(a) getVulkanResultString(a)
static const char *getVulkanResultString(vk::Result result)
{
	switch (result)
	{
	case vk::Result::eSuccess:
		return "VK_SUCCESS";
	case vk::Result::eNotReady:
		return "VK_NOT_READY";
	case vk::Result::eTimeout:
		return "VK_TIMEOUT";
	case vk::Result::eEventSet:
		return "VK_EVENT_SET";
	case vk::Result::eEventReset:
		return "VK_EVENT_RESET";
	case vk::Result::eIncomplete:
		return "VK_INCOMPLETE";
	case vk::Result::eErrorOutOfHostMemory:
		return "VK_ERROR_OUT_OF_HOST_MEMORY";
	case vk::Result::eErrorOutOfDeviceMemory:
		return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
	case vk::Result::eErrorInitializationFailed:
		return "VK_ERROR_INITIALIZATION_FAILED";
	case vk::Result::eErrorDeviceLost:
		return "VK_ERROR_DEVICE_LOST";
	case vk::Result::eErrorMemoryMapFailed:
		return "VK_ERROR_MEMORY_MAP_FAILED";
	case vk::Result::eErrorLayerNotPresent:
		return "VK_ERROR_LAYER_NOT_PRESENT";
	case vk::Result::eErrorExtensionNotPresent:
		return "VK_ERROR_EXTENSION_NOT_PRESENT";
	case vk::Result::eErrorFeatureNotPresent:
		return "VK_ERROR_FEATURE_NOT_PRESENT";
	case vk::Result::eErrorIncompatibleDriver:
		return "VK_ERROR_INCOMPATIBLE_DRIVER";
	case vk::Result::eErrorTooManyObjects:
		return "VK_ERROR_TOO_MANY_OBJECTS";
	case vk::Result::eErrorFormatNotSupported:
		return "VK_ERROR_FORMAT_NOT_SUPPORTED";
	case vk::Result::eErrorFragmentedPool:
		return "VK_ERROR_FRAGMENTED_POOL";
	case vk::Result::eErrorOutOfPoolMemory:
		return "VK_ERROR_OUT_OF_POOL_MEMORY";
	case vk::Result::eErrorInvalidExternalHandle:
		return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
	case vk::Result::eErrorSurfaceLostKHR:
		return "VK_ERROR_SURFACE_LOST_KHR";
	case vk::Result::eErrorNativeWindowInUseKHR:
		return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
	case vk::Result::eSuboptimalKHR:
		return "VK_SUBOPTIMAL_KHR";
	case vk::Result::eErrorOutOfDateKHR:
		return "VK_ERROR_OUT_OF_DATE_KHR";
	case vk::Result::eErrorIncompatibleDisplayKHR:
		return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
	case vk::Result::eErrorValidationFailedEXT:
		return "VK_ERROR_VALIDATION_FAILED_EXT";
	case vk::Result::eErrorInvalidShaderNV:
		return "VK_ERROR_INVALID_SHADER_NV";
	case vk::Result::eErrorNotPermittedEXT:
		return "VK_ERROR_NOT_PERMITTED_EXT";
	default:
		return "VK_ERROR_<Unknown>";
	}
}

#endif //__vk_h__
