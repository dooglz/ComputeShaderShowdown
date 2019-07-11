#pragma once

#define BAIL_ON_BAD_RESULT(result) \
  if (VK_SUCCESS != (result)) { BAIL }


#include <vulkan/vulkan.hpp>

VkResult vkGetBestTransferQueueNPH(const VkPhysicalDevice& physicalDevice, uint32_t* queueFamilyIndex);
VkResult vkGetBestComputeQueueNPH(const VkPhysicalDevice& physicalDevice, uint32_t* queueFamilyIndex);
VKAPI_ATTR VkBool32 VKAPI_CALL messageCallback(
	VkDebugReportFlagsEXT flags,
	VkDebugReportObjectTypeEXT objType,
	uint64_t srcObject,
	size_t location,
	int32_t msgCode,
	const char* pLayerPrefix,
	const char* pMsg,
	void* pUserData);