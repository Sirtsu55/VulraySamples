
cmake_minimum_required(VERSION 3.25)


project("VulraySamples")

find_package(Vulkan REQUIRED)


if(WIN32) # on Windows, we need to copy the vulkan dlls to the output directory, but on Linux, we don't need to do that, because its in the system path
	set(DXC_DLL $ENV{VULKAN_SDK}/Bin/dxcompiler.dll)
endif()


#add 3rd party libraries

add_subdirectory("Vulray")
add_subdirectory("Vendor/glfw")
add_subdirectory("Vendor/glm")

include_directories(
	"Base/" 
	"Vulray/Include/" 
	"Vendor/glfw/" 
	"Vendor/glm/"
	"Vendor/tinygltf/"
	"${Vulkan_INCLUDE_DIRS}"
	"${PROJECT_SOURCE_DIR}/"
	)
	
link_directories("Vulray" "Vendor/glfw/src" "${PROJECT_SOURCE_DIR}/dxc")


link_libraries(Vulray glfw dxcompiler)

# helper function to copy shaders to the output directory for each sample
function(ConfigureTarget tgt)
	set_property(TARGET ${tgt} PROPERTY CXX_STANDARD 20)
	target_compile_definitions(${tgt} PRIVATE SAMPLE_NAME="${tgt}")
	target_precompile_headers(${tgt} PRIVATE "${PROJECT_SOURCE_DIR}/Base/Common.h")

	add_custom_command(
         TARGET ${tgt} POST_BUILD
         COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Shaders $<TARGET_FILE_DIR:${tgt}>/Shaders
         COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Assets $<TARGET_FILE_DIR:${tgt}>/Assets
         COMMENT "Copying shaders for ${tgt}")
	if(WIN32)
		add_custom_command(
			TARGET ${tgt} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy ${DXC_DLL} $<TARGET_FILE_DIR:${tgt}>
			COMMAND ${CMAKE_COMMAND} -E echo "Copying DXC dll")
	endif()

endfunction()


# add samples
add_subdirectory("Samples/HelloTriangle")
add_subdirectory("Samples/DynamicBLAS")
add_subdirectory("Samples/DynamicTLAS")
add_subdirectory("Samples/MeshMaterials")
add_subdirectory("Samples/Shading")
add_subdirectory("Samples/BoxIntersections")
add_subdirectory("Samples/SBTData")
add_subdirectory("Samples/Compaction")
add_subdirectory("Samples/Callable")
add_subdirectory("Samples/GaussianBlurDenoising")
