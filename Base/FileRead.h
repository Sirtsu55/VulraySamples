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
		VULRAY_LOG_ERROR("Failed to open file: {0}", filePath.c_str());
		VULRAY_LOG_ERROR("Invalid Path or File invalid");



		return false;
	}

	size_t fileSize = (size_t)file.tellg();

	outVector.resize(fileSize / sizeof(T));

		

	//put file cursor at beginning
	file.seekg(0);

	//load the entire file into the buffer
	file.read((char*)outVector.data(), fileSize);

	//now that the file is loaded into the buffer, we can close it
	file.close();
	return true;
}