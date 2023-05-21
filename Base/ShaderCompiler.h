#pragma once
#include <vulkan/vulkan.hpp>

#ifdef _WIN32
    #include <atlbase.h>
#else
    #include <dxc/WinAdapter.h>
#endif

#include <dxc/dxcapi.h>

class ShaderCompiler
{
public:
    ShaderCompiler();

    ~ShaderCompiler();

    std::vector<uint32_t> CompileSPIRVFromSource(const std::vector<char>& source);
    std::vector<uint32_t> CompileSPIRVFromFile(const std::string& file);
private:

    CComPtr<IDxcUtils> mUtils;
    CComPtr<IDxcCompiler3> mCompiler;
    CComPtr<IDxcIncludeHandler> mIncludeHandler;
};


