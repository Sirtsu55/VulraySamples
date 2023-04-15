#pragma once
#include <vulkan/vulkan.hpp>
#include <tiny_gltf.h>

uint32_t GetSizeFromType(uint32_t type)
{
    if(type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
        return sizeof(unsigned char);
    if(type == TINYGLTF_COMPONENT_TYPE_BYTE)
        return sizeof(char);
    if(type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
        return sizeof(unsigned int);
    if(type == TINYGLTF_COMPONENT_TYPE_INT)
        return sizeof(int);
    if(type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
        return sizeof(unsigned short);
    if(type == TINYGLTF_COMPONENT_TYPE_SHORT)
        return sizeof(short);
    if(type == TINYGLTF_COMPONENT_TYPE_FLOAT)
        return sizeof(float);
    if(type == TINYGLTF_COMPONENT_TYPE_DOUBLE)
        return sizeof(double);
    return 1;
}
uint32_t GetComponentsFromTinyGLTFType(uint32_t type)
{
    if(type == TINYGLTF_TYPE_SCALAR)
        return 1;
    if(type == TINYGLTF_TYPE_VEC2)
        return 2;
    if(type == TINYGLTF_TYPE_VEC3)
        return 3;
    if(type == TINYGLTF_TYPE_VEC4)
        return 4;
    if(type == TINYGLTF_TYPE_MAT2)
        return 4;
    if(type == TINYGLTF_TYPE_MAT3)
        return 9;
    if(type == TINYGLTF_TYPE_MAT4)
        return 16;
    return 1;
}
vk::IndexType ConvertToIndexType(uint32_t type)
{
    if(type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
        return vk::IndexType::eUint8EXT;
    if(type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
        return vk::IndexType::eUint32;
    if(type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
        return vk::IndexType::eUint16;

    return vk::IndexType::eNoneKHR;
}

vk::Format ConvertToVkFormat(uint32_t components, uint32_t type)
{
    if(components == 1)
    {
        if(type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
            return vk::Format::eR8Uint;
        if(type == TINYGLTF_COMPONENT_TYPE_BYTE)
            return vk::Format::eR8Sint;
        if(type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
            return vk::Format::eR32Uint;
        if(type == TINYGLTF_COMPONENT_TYPE_INT)
            return vk::Format::eR32Sint;
        if(type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
            return vk::Format::eR16Uint;
        if(type == TINYGLTF_COMPONENT_TYPE_SHORT)
            return vk::Format::eR16Sint;
        if(type == TINYGLTF_COMPONENT_TYPE_FLOAT)
            return vk::Format::eR32Sfloat;
        if(type == TINYGLTF_COMPONENT_TYPE_DOUBLE)
            return vk::Format::eR64Sfloat;
    }
    if(components == 2)
    {
        if(type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
            return vk::Format::eR8G8Uint;
        if(type == TINYGLTF_COMPONENT_TYPE_BYTE)
            return vk::Format::eR8G8Sint;
        if(type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
            return vk::Format::eR32G32Uint;
        if(type == TINYGLTF_COMPONENT_TYPE_INT)
            return vk::Format::eR32G32Sint;
        if(type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
            return vk::Format::eR16G16Uint;
        if(type == TINYGLTF_COMPONENT_TYPE_SHORT)
            return vk::Format::eR16G16Sint;
        if(type == TINYGLTF_COMPONENT_TYPE_FLOAT)
            return vk::Format::eR32G32Sfloat;
        if(type == TINYGLTF_COMPONENT_TYPE_DOUBLE)
            return vk::Format::eR64G64Sfloat;
    }
    if(components == 3)
    {
        if(type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
            return vk::Format::eR8G8B8Uint;
        if(type == TINYGLTF_COMPONENT_TYPE_BYTE)
            return vk::Format::eR8G8B8Sint;
        if(type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
            return vk::Format::eR32G32B32Uint;
        if(type == TINYGLTF_COMPONENT_TYPE_INT)
            return vk::Format::eR32G32B32Sint;
        if(type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
            return vk::Format::eR16G16B16Uint;
        if(type == TINYGLTF_COMPONENT_TYPE_SHORT)
            return vk::Format::eR16G16B16Sint;
        if(type == TINYGLTF_COMPONENT_TYPE_FLOAT)
            return vk::Format::eR32G32B32Sfloat;
        if(type == TINYGLTF_COMPONENT_TYPE_DOUBLE)
            return vk::Format::eR64G64B64Sfloat;
    }
    if(components == 4)
    {
        if(type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
            return vk::Format::eR8G8B8A8Uint;
        if(type == TINYGLTF_COMPONENT_TYPE_BYTE)
            return vk::Format::eR8G8B8A8Sint;
        if(type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
            return vk::Format::eR32G32B32A32Uint;
        if(type == TINYGLTF_COMPONENT_TYPE_INT)
            return vk::Format::eR32G32B32A32Sint;
        if(type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
            return vk::Format::eR16G16B16A16Uint;
        if(type == TINYGLTF_COMPONENT_TYPE_SHORT)
            return vk::Format::eR16G16B16A16Sint;
        if(type == TINYGLTF_COMPONENT_TYPE_FLOAT)
            return vk::Format::eR32G32B32A32Sfloat;
        if(type == TINYGLTF_COMPONENT_TYPE_DOUBLE)
            return vk::Format::eR64G64B64A64Sfloat;
    }
    return vk::Format::eUndefined;
}