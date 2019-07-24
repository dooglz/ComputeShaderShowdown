#include "../utils.h"
#include "bk_vulkan.h"
#include "vulkan_utils.h"
#include <vulkan/vulkan.hpp>
#include <shadercN/shaderc.h>
#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <chrono>
#define VALIDATION_LAYERS 0
#define DEBUG_EXT 0
//#include <util_init.hpp>
/*
 * A layer can expose extensions, keep track of those
 * extensions here.
 */
typedef struct {
	VkLayerProperties properties;
	std::vector<VkExtensionProperties> instance_extensions;
	std::vector<VkExtensionProperties> device_extensions;
} layer_properties;

/*
 * Keep each of our swap chain buffers' image, command buffer and view in one
 * spot
 */
typedef struct _swap_chain_buffers {
	VkImage image;
	VkImageView view;
} swap_chain_buffer;


struct sample_info {

	std::vector<const char*> instance_layer_names;
	std::vector<const char*> instance_extension_names;
	std::vector<layer_properties> instance_layer_properties;
	std::vector<VkExtensionProperties> instance_extension_properties;
	VkInstance inst;

	uint32_t computeQueueFamilyIndex;

	std::vector<const char*> device_extension_names;
	std::vector<VkExtensionProperties> device_extension_properties;
	VkPhysicalDevice PhysicalDevice;
	VkDevice device;

	VkPhysicalDeviceProperties gpu_props;
	std::vector<VkQueueFamilyProperties> queue_props;
	VkPhysicalDeviceMemoryProperties memory_properties;

	VkCommandPool cmd_pool;
	VkDeviceMemory stagingMemory;
	VkImage stagingImage;

	struct {
		VkBuffer buf;
		VkDeviceMemory mem;
		VkDescriptorBufferInfo buffer_info;
	} vertex_buffer;
	VkVertexInputBindingDescription vi_binding;
	VkVertexInputAttributeDescription vi_attribs[2];

	VkCommandBuffer cmd; // Buffer for initialization commands
	VkPipelineLayout pipeline_layout;
	std::vector<VkDescriptorSetLayout> desc_layout;
	VkPipelineCache pipelineCache;
	VkRenderPass render_pass;
	VkPipeline pipeline;

	VkPipelineShaderStageCreateInfo shaderStages[2];

	VkDescriptorPool desc_pool;
	std::vector<VkDescriptorSet> desc_set;

	PFN_vkCreateDebugReportCallbackEXT dbgCreateDebugReportCallback;
	PFN_vkDestroyDebugReportCallbackEXT dbgDestroyDebugReportCallback;
	PFN_vkDebugReportMessageEXT dbgBreakCallback;
	std::vector<VkDebugReportCallbackEXT> debug_report_callbacks;

	uint32_t current_buffer;
	uint32_t queue_family_count;
};



void go(sample_info& info);

VkDebugReportCallbackEXT cb1;
struct sample_info info = {};

int VK_deInit() {
	if (VALIDATION_LAYERS)
	{
		if (DEBUG_EXT) {
			PFN_vkDestroyDebugReportCallbackEXT DestroyDebugReportCallback = VK_NULL_HANDLE;
			DestroyDebugReportCallback = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(vkGetInstanceProcAddr(info.inst, "vkDestroyDebugReportCallbackEXT"));
			BAIL_IF(DestroyDebugReportCallback, VK_NULL_HANDLE);
			DestroyDebugReportCallback(info.inst, cb1, NULL);
		}
	}

	vkDestroyDevice(info.device, NULL);
	vkDestroyInstance(info.inst, NULL);
	return 0;
}

int VK_init(unsigned char dev) {
	traceEvent("VK Init");
	// initialize the VkApplicationInfo structure
	VkApplicationInfo app_info = {};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pNext = NULL;
	app_info.pApplicationName = "ShaderShowdown";
	app_info.applicationVersion = 1;
	app_info.pEngineName = "ShaderShowdown";
	app_info.engineVersion = 1;
	app_info.apiVersion = VK_API_VERSION_1_1;

	// initialize the VkInstanceCreateInfo structure
	VkInstanceCreateInfo inst_create_info = {};
	inst_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	inst_create_info.pNext = NULL;
	inst_create_info.flags = 0;
	inst_create_info.pApplicationInfo = &app_info;
	inst_create_info.enabledExtensionCount = 0;
	inst_create_info.ppEnabledExtensionNames = NULL;
	inst_create_info.enabledLayerCount = 0;
	inst_create_info.ppEnabledLayerNames = NULL;

	if (VALIDATION_LAYERS)
	{
		inst_create_info.enabledLayerCount = 1;
		constexpr char* vlayer = "VK_LAYER_LUNARG_standard_validation";
		inst_create_info.ppEnabledLayerNames = &vlayer;
		if (DEBUG_EXT) {
			constexpr char* vExt = "VK_EXT_debug_report";
			inst_create_info.ppEnabledExtensionNames = &vExt;
			inst_create_info.enabledExtensionCount = 1;
		}
	}

	VkResult U_ASSERT_ONLY res;

	res = vkCreateInstance(&inst_create_info, NULL, &info.inst);
	if (res == VK_ERROR_INCOMPATIBLE_DRIVER) {
		std::cout << "cannot find a compatible Vulkan ICD\n";
		exit(-1);
	}
	else if (res == VK_ERROR_LAYER_NOT_PRESENT) {
		std::cout << "VK_ERROR_LAYER_NOT_PRESENT\n";
		exit(-1);
	}
	else if (res) {
		std::cout << "unknown error\n";
		exit(-1);
	}
	std::cout << "Created VK Instance\n";

	if (VALIDATION_LAYERS)
	{
		if (DEBUG_EXT) {
			PFN_vkCreateDebugReportCallbackEXT CreateDebugReportCallback = VK_NULL_HANDLE;
			CreateDebugReportCallback = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(vkGetInstanceProcAddr(info.inst, "vkCreateDebugReportCallbackEXT"));
			BAIL_IF(CreateDebugReportCallback, VK_NULL_HANDLE);

			VkDebugReportCallbackCreateInfoEXT callback1 = {
					VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,    // sType
					NULL,                                                       // pNext
					VK_DEBUG_REPORT_ERROR_BIT_EXT |                             // flags
					VK_DEBUG_REPORT_WARNING_BIT_EXT,
					messageCallback,                                        // pfnCallback
					NULL                                                        // pUserData
			};
			BAIL_ON_BAD_RESULT(CreateDebugReportCallback(info.inst, &callback1, nullptr, &cb1));
		}
	}

	uint32_t gpu_count = 1;
	res = vkEnumeratePhysicalDevices(info.inst, &gpu_count, NULL);
	assert(gpu_count);
	assert(dev < gpu_count);
	std::vector<VkPhysicalDevice> physdevs;
	physdevs.resize(gpu_count);
	res = vkEnumeratePhysicalDevices(info.inst, &gpu_count, physdevs.data());
	assert(!res && gpu_count >= 1);
	for (const auto g : physdevs) {
		VkPhysicalDeviceProperties dp;
		vkGetPhysicalDeviceProperties(g, &dp);
		std::cout << "Device " << dp.deviceID << " - " << dp.deviceName << " - " << dp.driverVersion << "\n";
	}
	//std::cout << "usign Device " << physdevs[dev].
	info.PhysicalDevice = physdevs[dev];
	//for (const auto& g : info.gpus) {

	info.computeQueueFamilyIndex = 0;
	BAIL_ON_BAD_RESULT(vkGetBestComputeQueueNPH(info.PhysicalDevice, &info.computeQueueFamilyIndex));
	std::cout << "Using Queue " << info.computeQueueFamilyIndex << " For Compute work\n";
	const float queuePrioritory = 1.0f;

	VkDeviceQueueCreateInfo deviceQueueCreateInfo;
	deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	deviceQueueCreateInfo.pNext = nullptr;
	deviceQueueCreateInfo.flags = 0;
	deviceQueueCreateInfo.queueFamilyIndex = info.computeQueueFamilyIndex;
	deviceQueueCreateInfo.queueCount = 1;
	deviceQueueCreateInfo.pQueuePriorities = &queuePrioritory;

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pNext = nullptr;
	deviceCreateInfo.flags = 0;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.pQueueCreateInfos = &deviceQueueCreateInfo;
	deviceCreateInfo.enabledLayerCount = 0;
	deviceCreateInfo.ppEnabledLayerNames = 0;
	deviceCreateInfo.enabledExtensionCount = 0;
	deviceCreateInfo.ppEnabledExtensionNames = 0;
	deviceCreateInfo.pEnabledFeatures = nullptr;

	VkDevice device;
	auto ret = vkCreateDevice(info.PhysicalDevice, &deviceCreateInfo, nullptr, &device);
	BAIL_ON_BAD_RESULT(ret)
		info.device = device;
	std::cout << "Created Device " << device << "\n";

	return 0;
}



void  VK_go(size_t runs) {
	const VkPhysicalDevice& g = info.PhysicalDevice;
	VkPhysicalDeviceMemoryProperties properties;
	vkGetPhysicalDeviceMemoryProperties(g, &properties);

	const int32_t bufferLength = 16384;
	const uint32_t bufferSize = sizeof(int32_t) * bufferLength;

	// we are going to need two buffers from this one memory
	const VkDeviceSize memorySize = bufferSize * 2;
	// set memoryTypeIndex to an invalid entry in the properties.memoryTypes array
	uint32_t memoryTypeIndex = VK_MAX_MEMORY_TYPES;

	//find a coherent host mappable memory element
	for (uint32_t k = 0; k < properties.memoryTypeCount; k++) {
		if ((VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT & properties.memoryTypes[k].propertyFlags) &&
			(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT & properties.memoryTypes[k].propertyFlags) &&
			(memorySize < properties.memoryHeaps[properties.memoryTypes[k].heapIndex].size)) {
			memoryTypeIndex = k;
			break;
		}
	}

	BAIL_ON_BAD_RESULT(memoryTypeIndex == VK_MAX_MEMORY_TYPES ? VK_ERROR_OUT_OF_HOST_MEMORY : VK_SUCCESS);
	std::cout << "Using Memory Area " << memoryTypeIndex << " Capacity:" << properties.memoryHeaps[properties.memoryTypes[memoryTypeIndex].heapIndex].size << "\n";
	std::cout << "Allocating " << bufferSize << " bytes\n";

	const VkMemoryAllocateInfo memoryAllocateInfo = {
	  VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
	  0,
	  memorySize,
	  memoryTypeIndex
	};
	auto device = info.device;
	VkDeviceMemory memory;
	BAIL_ON_BAD_RESULT(vkAllocateMemory(device, &memoryAllocateInfo, 0, &memory));

	int32_t* payload;
	BAIL_ON_BAD_RESULT(
		vkMapMemory(device, memory, 0, memorySize, 0, (void**)(&payload))
	);
	for (uint32_t k = 1; k < memorySize / sizeof(int32_t); k++) {
		payload[k] = (k % 10);//rand();
	}
	vkUnmapMemory(device, memory);


	const VkBufferCreateInfo bufferCreateInfo = {
	  VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
	  0, 0,
	  bufferSize,
	  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
	  VK_SHARING_MODE_EXCLUSIVE,
	  1,
	  &info.computeQueueFamilyIndex
	};

	VkBuffer in_buffer;
	VkMemoryRequirements memreqs;
	BAIL_ON_BAD_RESULT(vkCreateBuffer(device, &bufferCreateInfo, 0, &in_buffer));
	vkGetBufferMemoryRequirements(device, in_buffer, &memreqs);
	BAIL_ON_BAD_RESULT(vkBindBufferMemory(device, in_buffer, memory, 0));

	VkBuffer out_buffer;
	BAIL_ON_BAD_RESULT(vkCreateBuffer(device, &bufferCreateInfo, 0, &out_buffer));
	vkGetBufferMemoryRequirements(device, out_buffer, &memreqs);
	BAIL_ON_BAD_RESULT(vkBindBufferMemory(device, out_buffer, memory, bufferSize));

	std::cout << "Compiling Shader\n";
	VkShaderModule shader_module;

	{
		shaderc_compiler_t compiler = shaderc_compiler_initialize();
		const std::string shaderSource = "#version 450\nvoid main() {}";
		const std::string kMinimalShaderWithoutVersion = "void main(){}";
		const std::string shaderSource2 =
			"#version 450\n"
			"void main() {\n"
			"    uvec3 temp = gl_WorkGroupID;\n"
			"}";
		const std::string shaderSource3 =
			"#version 450\n"
			"layout(binding = 0) buffer Pos1 {uint vIn[];};"
			"layout(binding = 0) buffer Pos2 {uint vOut[];};"
			"void main() {\n"
			"	uint index = gl_GlobalInvocationID.x;"
			"	if (index >= 16384){return;}"
			"	vOut[index] = vIn[index]*2;"
			"}";



		shaderc_compile_options_t so = shaderc_compile_options_initialize();
		shaderc_compile_options_set_target_env(so, shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_1);

		shaderc_compilation_result_t shaderSPV = shaderc_compile_into_spv(
			compiler,
			shaderSource3.data(),
			shaderSource3.size(),
			shaderc_glsl_compute_shader,
			"main.comp",
			"main",
			so);

		std::cout << "Compiled, Warnings:" << shaderc_result_get_num_warnings(shaderSPV) << " Errors:" << shaderc_result_get_num_errors(shaderSPV) << "\n";
		const  shaderc_compilation_status  ss = shaderc_result_get_compilation_status(shaderSPV);

		if (ss != shaderc_compilation_status_success) {
			std::cout << "Shader Error:\n" << shaderc_result_get_error_message(shaderSPV) << std::endl;
			exit(-1);
		}

		uint32_t* da = (uint32_t*)shaderc_result_get_bytes(shaderSPV);
		const auto sl = shaderc_result_get_length(shaderSPV);

		std::cout << "Creating Shader Module\n";
		VkShaderModuleCreateInfo shaderModuleCreateInfo = {
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		0,
		0,
		sl,
		da
		};
		BAIL_ON_BAD_RESULT(vkCreateShaderModule(device, &shaderModuleCreateInfo, 0, &shader_module));

		shaderc_result_release(shaderSPV);
		shaderc_compiler_release(compiler);
	}
	std::cout << "Driver Accepted Shader\n";

	VkDescriptorSetLayoutBinding descriptorSetLayoutBindings[2] = {
	  {
		0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, 0
	  },
	  {
		1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, 0
	  }
	};

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
	  VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
	  0,0,2,
	  descriptorSetLayoutBindings
	};

	VkDescriptorSetLayout descriptorSetLayout;
	BAIL_ON_BAD_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, 0, &descriptorSetLayout));


	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
	  VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
	  0, 0, 1,
	  &descriptorSetLayout,
	  0, 0
	};
	VkPipelineLayout pipelineLayout;
	BAIL_ON_BAD_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, 0, &pipelineLayout));

	VkComputePipelineCreateInfo computePipelineCreateInfo = {
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		0,
		0,
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			0,
			0,
			VK_SHADER_STAGE_COMPUTE_BIT,
			shader_module,
			"main",
			0
		},
		pipelineLayout,
		0,
		0
	};

	VkPipeline pipeline;
	BAIL_ON_BAD_RESULT(vkCreateComputePipelines(device, 0, 1, &computePipelineCreateInfo, 0, &pipeline));
	std::cout << "Pipeline Created\n";

	VkCommandPoolCreateInfo commandPoolCreateInfo = {
	  VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
	  0,0,
	  info.computeQueueFamilyIndex
	};

	VkDescriptorPoolSize descriptorPoolSize = {
	  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2
	};

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
	  VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
	  0,0,1,1,
	  &descriptorPoolSize
	};

	VkDescriptorPool descriptorPool;
	BAIL_ON_BAD_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, 0, &descriptorPool));

	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
	  VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
	  0,
	  descriptorPool,
	  1,
	  &descriptorSetLayout
	};

	VkDescriptorSet descriptorSet;
	BAIL_ON_BAD_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet));

	VkDescriptorBufferInfo in_descriptorBufferInfo = {
	  in_buffer,
	  0,
	  VK_WHOLE_SIZE
	};

	VkDescriptorBufferInfo out_descriptorBufferInfo = {
	  out_buffer,
	  0,
	  VK_WHOLE_SIZE
	};

	VkWriteDescriptorSet writeDescriptorSet[2] = {
	  {
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		0,
		descriptorSet,
		0,
		0,
		1,
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		0,
		&in_descriptorBufferInfo,
		0
	  },
	  {
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		0,
		descriptorSet,
		1,
		0,
		1,
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		0,
		&out_descriptorBufferInfo,
		0
	  }
	};

	vkUpdateDescriptorSets(device, 2, writeDescriptorSet, 0, 0);


	VkCommandPool commandPool;
	BAIL_ON_BAD_RESULT(vkCreateCommandPool(device, &commandPoolCreateInfo, 0, &commandPool));

	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
	  VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
	  0,
	  commandPool,
	  VK_COMMAND_BUFFER_LEVEL_PRIMARY,
	  1
	};

	VkCommandBuffer commandBuffer;
	BAIL_ON_BAD_RESULT(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer));

	VkCommandBufferBeginInfo commandBufferBeginInfo = {
	  VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
	  0,
	  0,
	  0
	};

	BAIL_ON_BAD_RESULT(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, 0);

	vkCmdDispatch(commandBuffer, bufferSize / sizeof(int32_t), 1, 1);

	BAIL_ON_BAD_RESULT(vkEndCommandBuffer(commandBuffer));

	std::cout << "Ready To Dispatch\n";

	VkQueue queue;
	vkGetDeviceQueue(device, info.computeQueueFamilyIndex, 0, &queue);

	VkSubmitInfo submitInfo = {
	  VK_STRUCTURE_TYPE_SUBMIT_INFO,
	  0,
	  0,
	  0,
	  0,
	  1,
	  &commandBuffer,
	  0,
	  0
	};
	traceEvent("VK Start");
	for (size_t i = 0; i < runs; i++)
	{
		traceEvent("Submit");
		auto t1 = std::chrono::high_resolution_clock::now();
		BAIL_ON_BAD_RESULT(vkQueueSubmit(queue, 1, &submitInfo, 0));
		BAIL_ON_BAD_RESULT(vkQueueWaitIdle(queue));
		auto t2 = std::chrono::high_resolution_clock::now();
		std::cout << "Done " << std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count() << "ns \n";
	}
	traceEvent("VK End");

	BAIL_ON_BAD_RESULT(vkMapMemory(device, memory, 0, memorySize, 0, (void**)& payload));
	//std::cout << payload[0] << "," << payload[1] << "," << payload[2] << "," << payload[3] << std::endl;
	std::cout << printSome(payload, bufferLength);
	vkUnmapMemory(device, memory);


	vkDestroyCommandPool(device, commandPool, NULL);
	vkDestroyDescriptorPool(device, descriptorPool, NULL);
	vkDestroyPipeline(device, pipeline, NULL);
	vkDestroyPipelineLayout(device, pipelineLayout, NULL);
	vkDestroyDescriptorSetLayout(device, descriptorSetLayout, NULL);
	vkDestroyShaderModule(device, shader_module, NULL);
	vkDestroyBuffer(device, in_buffer, NULL);
	vkDestroyBuffer(device, out_buffer, NULL);
	vkFreeMemory(device, memory, NULL);
}
