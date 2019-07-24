cmake_minimum_required (VERSION 3.11)

project (shader_showdown)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)


option(ShdrShwdwn_CUDA "" ON)
if(${ShdrShwdwn_CUDA})
	#enable_language(CUDA)
	find_package(CUDA REQUIRED)
	file(GLOB CUDA_SOURCE_FILES cuda/*.cpp cuda/*.h)
	source_group(CUDA FILES ${CUDA_SOURCE_FILES})
	set(SOURCE_FILES ${SOURCE_FILES} ${CUDA_SOURCE_FILES})
endif(${ShdrShwdwn_CUDA})


option(ShdrShwdwn_OPENCL "" ON)
if(${ShdrShwdwn_OPENCL})
	#find_package(OpenCL REQUIRED)
	#set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/opencl/common-lib-amd-APPSDK-3.0/")
	#include(Global-OpenCL)
	##
	set(OPENCL_DIR "${CMAKE_SOURCE_DIR}/../OpenCL-ICD-Loader/build/Release")
	set(OpenCL_LIBRARIES "${OPENCL_DIR}/OpenCL.lib")
	set(OpenCL_INCLUDE_DIRS "${OPENCL_DIR}")
	##
	#set(OPENCL_DIR "${CMAKE_SOURCE_DIR}/opencl/common-lib-amd-APPSDK-3.0/3-0")
	#set(OpenCL_LIBRARIES "${OPENCL_DIR}/lib/x86_64/OpenCL.lib")
	#set(OpenCL_INCLUDE_DIRS "${OPENCL_DIR}/include")
	##
	file(GLOB OPENCL_SOURCE_FILES opencl/*.cpp opencl/*.h)
	source_group(OPENCL FILES ${OPENCL_SOURCE_FILES})
	set(SOURCE_FILES ${SOURCE_FILES} ${OPENCL_SOURCE_FILES})
endif(${ShdrShwdwn_OPENCL})

option(ShdrShwdwn_VULKAN "" ON)

if(${ShdrShwdwn_VULKAN})
	file(GLOB VK_SOURCE_FILES vulkan/*.cpp vulkan/*.h)
	source_group(Vulkan FILES ${VK_SOURCE_FILES})
	find_package(Vulkan REQUIRED)
	set(SOURCE_FILES ${SOURCE_FILES} ${VK_SOURCE_FILES})
endif(${ShdrShwdwn_VULKAN})

option(ShdrShwdwn_DX12 "" ON)

if(${ShdrShwdwn_DX12})
	file(GLOB DX12_SOURCE_FILES dx12/*.cpp dx12/*.h)
	source_group(DX12 FILES ${DX12_SOURCE_FILES})
	set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/dx12/")
	set(DXC_BUILD_ARCH "x64")
	find_package(D3D12 REQUIRED)
	set(SOURCE_FILES ${SOURCE_FILES} ${DX12_SOURCE_FILES})
endif(${ShdrShwdwn_DX12})





add_executable(MAIN main.cpp utils.h utils.cpp ss_RenderDoc.cpp ss_RenderDoc.h ${SOURCE_FILES})


if(${ShdrShwdwn_DX12})
	target_include_directories(MAIN PRIVATE ${D3D12_INCLUDE_DIR} ${DXGI_INCLUDE_DIR})
	target_link_libraries(MAIN ${D3D12_LIBRARY} ${DXGI_LIBRARY} ${D3DCOMPILER_LIBRARY})
endif(${ShdrShwdwn_DX12})

if(${ShdrShwdwn_VULKAN})
	target_compile_definitions(MAIN PRIVATE VK_USE_PLATFORM_WIN32_KHR)
	target_include_directories(MAIN PRIVATE Vulkan::Vulkan)
	target_link_libraries(MAIN Vulkan::Vulkan)
	#Also Link to compiled ShaderRC from VSDK (Could include() the Source Cmakelists but this is quicker)
	#However VSDK doesn't ship with debug libs so in this case we need to build ourselves

	get_filename_component(Vulkan_LIBRARY_DIR ${Vulkan_LIBRARY} DIRECTORY)  
	option(ShdrShwdwn_BUILD_SHADERC "BUILD ShaderRC from scratch?" ${ShdrShwdwn_BUILD_SHADERC})
	set(ShdrShwdwn_SHADERC_SRC_DIR "${Vulkan_LIBRARY_DIR}/../shaderc" CACHE FILEPATH "")
	set(ShdrShwdwn_SHADERC_DIR "${Vulkan_LIBRARY_DIR}/shaderc_combined.lib" CACHE FILEPATH "")

	if(${ShdrShwdwn_BUILD_SHADERC})
	  message(STATUS "Configuring ShdrShwdwn to build ShaderRC.")
	  set(SKIP_GLSLANG_INSTALL ON)
	  set(SKIP_SPIRV_TOOLS_INSTALL ON)
	  set(BUILD_GMOCK OFF)
	  set(BUILD_TESTING OFF)
	  set(SHADERC_SKIP_INSTALL ON)
	  set(SHADERC_SKIP_TESTS ON)
	  add_subdirectory(${ShdrShwdwn_SHADERC_SRC_DIR} "${CMAKE_BINARY_DIR}/bin" EXCLUDE_FROM_ALL)
	  target_link_libraries(MAIN shaderc)
	else()
	  message(STATUS "Configuring ShdrShwdwn to avoid building ShaderRC.")
	  target_link_libraries(MAIN 
		debug "C:/VulkanSDK/shaderc-2019.0/build/libshaderc/Debug/shaderc_shared.lib"
		optimized "C:/VulkanSDK/shaderc-2019.0/build/libshaderc/Release/shaderc_shared.lib"
	  )
		add_custom_command(TARGET MAIN POST_BUILD     
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"C:/VulkanSDK/shaderc-2019.0/build/libshaderc/$(Configuration)/shaderc_shared.dll"  
			$<TARGET_FILE_DIR:MAIN>
		)     
	endif()
endif(${ShdrShwdwn_VULKAN})

if(${ShdrShwdwn_OPENCL})
	target_include_directories(MAIN PRIVATE ${OpenCL_INCLUDE_DIRS})
	target_link_libraries(MAIN ${OpenCL_LIBRARIES})
endif(${ShdrShwdwn_OPENCL})

if(${ShdrShwdwn_CUDA})
	target_include_directories(MAIN PRIVATE ${CUDA_TOOLKIT_INCLUDE})
	get_filename_component(CUDA_LIBRARY_DIR ${CUDA_CUDA_LIBRARY} DIRECTORY)  
	target_link_libraries(MAIN ${CUDA_CUDART_LIBRARY} ${CUDA_CUDA_LIBRARY} "${CUDA_LIBRARY_DIR}/nvrtc.lib")
endif(${ShdrShwdwn_CUDA})

if(${MSVC})
    target_compile_options(MAIN PUBLIC /MP)
	target_link_libraries(MAIN "rpcrt4.lib")
endif()

include(CMakeDependentOption)
CMAKE_DEPENDENT_OPTION(ShdrShwdwn_PIXRUNTIME "" ON ${MSVC} OFF)
if(${MSVC} AND ${ShdrShwdwn_PIXRUNTIME})
	find_program(NUGET nuget)
	if(NOT NUGET)
	  message(FATAL "Cannot find nuget command line tool.\nInstall it with e.g. choco install nuget.commandline")
	else()
		execute_process(
			OUTPUT_VARIABLE stdo1 
			ERROR_VARIABLE std2 
			WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
			COMMAND ${NUGET} Install WinPixEventRuntime -OutputDirectory ${CMAKE_BINARY_DIR}/
		)
		file(GLOB_RECURSE PIXINC ${CMAKE_BINARY_DIR}/WinPixEventRuntime*/**/pix3.h)
		if(PIXINC)
			get_filename_component(WinPixEventRuntimeIncludes ${PIXINC} DIRECTORY) 
			get_filename_component(WinPixEventRuntimeIncludes ${WinPixEventRuntimeIncludes}/../ ABSOLUTE)
			target_include_directories(MAIN PRIVATE ${WinPixEventRuntimeIncludes})
			target_link_libraries(MAIN  "${WinPixEventRuntimeIncludes}/../bin/x64/WinPixEventRuntime.lib")
		endif()
endif()

ENDIF()





