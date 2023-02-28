
cmake_minimum_required(VERSION 3.24)


project("VulraySamples")

find_package(Vulkan REQUIRED)


#set global properties for all samples
set_property(GLOBAL PROPERTY CXX_STANDARD 20)

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
	
link_libraries(Vulray glfw)
	
add_compile_definitions(SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Shaders")

add_subdirectory("Samples/HelloTriangle")


file(GLOB_RECURSE RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/*)


foreach(RESOURCE ${RESOURCES})
	file(COPY ${RESOURCE} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Shaders/)
endforeach()
