#This is the main CMakeLists.txt file for the HelloTriangle sample.



file(GLOB_RECURSE APP_BASE_SRC "${PROJECT_SOURCE_DIR}/Base/*.cpp")

add_executable( "HelloTriangle"
	${APP_BASE_SRC}	 # base app code
	HelloTriangle.cpp)


set_property(TARGET "HelloTriangle" PROPERTY CXX_STANDARD 20)
