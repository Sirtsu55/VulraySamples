#This is the main CMakeLists.txt file for the HelloTriangle sample.



file(GLOB_RECURSE APP_BASE_SRC "${PROJECT_SOURCE_DIR}/Base/*.cpp")

add_executable( "MeshMaterials"
	${APP_BASE_SRC}	 # base app code
	MeshMaterials.cpp)


ConfigureTarget("MeshMaterials")
