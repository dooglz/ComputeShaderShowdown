cmake_minimum_required (VERSION 3.11)

project (shader_showdown)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(ShdrShwdwn_getShadeRC "Get newest libs from github rather than relying on vulkanSDK" ON)

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
	#Using FindOpenCl usually ends at the CUDA install, which has old CL headders.
	#I'm using this instead
	set(OPENCL_DIR "${CMAKE_SOURCE_DIR}/deps/AMD-Opencl/3-0")
	set(OpenCL_LIBRARIES ${OPENCL_DIR}/lib/x86_64/OpenCL.lib)
	set(OpenCL_INCLUDE_DIRS "${OPENCL_DIR}/include")
	
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


#This could be a submodule, but due to its size we'll do it all here incase someone doesn't want it.
if(${ShdrShwdwn_getShadeRC})
	include(ExternalProject)
	ExternalProject_Add ( ThirdParty_ShaderRC
		PREFIX "${CMAKE_BINARY_DIR}/ShaderRC"
		GIT_REPOSITORY "https://github.com/dooglz/shaderc"
		BINARY_DIR ${CMAKE_BINARY_DIR}/ShaderRCBuild
		CONFIGURE_COMMAND 
			py ${CMAKE_BINARY_DIR}/ShaderRC/src/ThirdParty_ShaderRC//utils/git-sync-deps &&
			${CMAKE_COMMAND} ${CMAKE_BINARY_DIR}/ShaderRC/src/ThirdParty_ShaderRC 
			-DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR><SOURCE_DIR>/ShaderRC # Tell CMake to use subdirectory as source.
			-DSKIP_GLSLANG_INSTALL=ON
			-DSKIP_SPIRV_TOOLS_INSTALL=ON
			-DBUILD_GMOCK=OFF
			-DBUILD_TESTING=OFF
			-DSHADERC_SKIP_INSTALL=ON
			-DSHADERC_SKIP_TESTS=ON
			-Wno-dev
		BUILD_COMMAND  ${CMAKE_COMMAND} --build . --target shaderc_shared --config $<CONFIG> 
		INSTALL_COMMAND ""	
		UPDATE_COMMAND "" 
	)
	add_dependencies(MAIN ThirdParty_ShaderRC)
	target_link_libraries(MAIN ShaderRCBuild/libshaderc/$<CONFIG>/shaderc_shared)
	add_custom_command(TARGET MAIN POST_BUILD     
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"ShaderRCBuild/libshaderc/$(Configuration)/shaderc_shared.dll"  
		$<TARGET_FILE_DIR:MAIN>
	)     
endif(${ShdrShwdwn_getShadeRC})



if(${ShdrShwdwn_DX12})
	target_include_directories(MAIN PRIVATE ${D3D12_INCLUDE_DIR} ${DXGI_INCLUDE_DIR})
	target_link_libraries(MAIN ${D3D12_LIBRARY} ${DXGI_LIBRARY} ${D3DCOMPILER_LIBRARY})
	add_compile_definitions("ss_compile_DX")
endif(${ShdrShwdwn_DX12})

if(${ShdrShwdwn_VULKAN})
	target_compile_definitions(MAIN PRIVATE VK_USE_PLATFORM_WIN32_KHR)
	target_include_directories(MAIN PRIVATE Vulkan::Vulkan)
	target_link_libraries(MAIN Vulkan::Vulkan)
	add_compile_definitions("ss_compile_VK")
endif(${ShdrShwdwn_VULKAN})

if(${ShdrShwdwn_OPENCL})
	target_include_directories(MAIN PRIVATE ${OpenCL_INCLUDE_DIRS})
	target_link_libraries(MAIN ${OpenCL_LIBRARIES})
	add_compile_definitions("ss_compile_CL")
endif(${ShdrShwdwn_OPENCL})

if(${ShdrShwdwn_CUDA})
	target_include_directories(MAIN PRIVATE ${CUDA_TOOLKIT_INCLUDE})
	get_filename_component(CUDA_LIBRARY_DIR ${CUDA_CUDA_LIBRARY} DIRECTORY)  
	target_link_libraries(MAIN ${CUDA_CUDART_LIBRARY} ${CUDA_CUDA_LIBRARY} "${CUDA_LIBRARY_DIR}/nvrtc.lib")
	add_compile_definitions("ss_compile_CU")
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


add_subdirectory("deps/CLI11")
target_link_libraries(MAIN  CLI11::CLI11)


