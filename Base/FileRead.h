#pragma once

#include "Common.h"
#include <fstream>

template<typename T>
bool FileRead(const std::string& filePath, std::vector<T>& outVector)
{
	//open the file. With cursor at the end
	std::ifstream file(filePath, std::ios::ate | std::ios::binary);

	if (!file.is_open())
	{
		VULRAY_FLOG_ERROR("Failed to open file: {0}", filePath.c_str());
		VULRAY_LOG_ERROR("Invalid Path or File invalid");

		return false;
	}

	size_t fileSize = (size_t)file.tellg();
	
	uint32_t outVectorSize = 0;

	// if the file size is not a multiple of the size of the type, we need to add one to the size of the vector,
	// so in the case of a vector of uint32_t, if the file size is 5 bytes we need a vector of size 2 of uint32_t, which is 8 bytes
	
	if(fileSize % sizeof(T) != 0)
		outVectorSize = (fileSize / sizeof(T)) + 1;
	else
		outVectorSize = fileSize / sizeof(T);

	outVector.resize(fileSize / sizeof(T));

		

	//put file cursor at beginning
	file.seekg(0);

	//load the entire file into the buffer
	file.read((char*)outVector.data(), fileSize);

	//now that the file is loaded into the buffer, we can close it
	file.close();
	return true;
}