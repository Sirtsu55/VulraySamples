#This is the main CMakeLists.txt file for the HelloTriangle sample.



file(GLOB_RECURSE APP_BASE_SRC "${PROJECT_SOURCE_DIR}/Base/*.cpp")

add_executable("SBTData"
	${APP_BASE_SRC}	 # base app code
	SBTData.cpp)


ConfigureTarget("SBTData")
