
cmake_minimum_required(VERSION 3.24)


project("VulraySamples")

find_package(Vulkan REQUIRED COMPONENTS dxc)


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
	"${Vulkan_INCLUDE_DIRS}"
	)
	
link_directories("Vulray" "Vendor/glfw/src")


link_libraries(Vulray glfw Vulkan::dxc_lib)

# helper function to copy shaders to the output directory for each sample
function(ConfigureTarget tgt)
	set_property(TARGET ${tgt} PROPERTY CXX_STANDARD 20)
	add_custom_command(
         TARGET ${tgt} POST_BUILD
         COMMAND ${CMAKE_COMMAND} 
		 -E copy_directory ${CMAKE_SOURCE_DIR}/Shaders $<TARGET_FILE_DIR:${tgt}>/Shaders
         COMMENT "Copying shaders for ${tgt}" VERBATIM)
	if(WIN32)
		add_custom_command(
			TARGET ${tgt} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy ${DXC_DLL} $<TARGET_FILE_DIR:${tgt}>
			COMMENT "Copying DXC dll" VERBATIM)
	endif()
endfunction()


# add samples
add_subdirectory("Samples/HelloTriangle")
add_subdirectory("Samples/DynamicBLAS")
