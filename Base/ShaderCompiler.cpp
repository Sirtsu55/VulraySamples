#include "ShaderCompiler.h"
#include "FileRead.h"
#include "Vulray/Vulray.h"
#include <filesystem>
#include <algorithm>



ShaderCompiler::ShaderCompiler()
{
    DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&mUtils));
    DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&mCompiler));
    mUtils->CreateDefaultIncludeHandler(&mIncludeHandler);
}


std::vector<uint32_t> ShaderCompiler::CompileSPIRVFromSource(const std::vector<char>& source)
{

    CComPtr<IDxcBlobEncoding> pSource;
    mUtils->CreateBlob(source.data(), source.size(), CP_UTF8, &pSource);
    // Preprocess the shader
    std::vector<const wchar_t*> arguments;

    arguments.push_back(L"-T");
    arguments.push_back(L"lib_6_5");

    arguments.push_back(L"-E");
    arguments.push_back(L"main");

    //Strip pdbs 
    arguments.push_back(L"-Qstrip_debug");

    //Compile to SPIR-V
    arguments.push_back(L"-spirv");
    arguments.push_back(L"-fspv-target-env=vulkan1.3");


    DxcBuffer sourceBuffer;
    sourceBuffer.Ptr = pSource->GetBufferPointer();
    sourceBuffer.Size = pSource->GetBufferSize();
    sourceBuffer.Encoding = 0;

    CComPtr<IDxcResult> pCompileResult;
    mCompiler->Compile(&sourceBuffer, arguments.data(), (uint32_t)arguments.size(), mIncludeHandler, IID_PPV_ARGS(&pCompileResult));

    //Error Handling
    CComPtr<IDxcBlobUtf8> pErrors;
    pCompileResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);
    if (pErrors && pErrors->GetStringLength() > 0)
    {
        VULRAY_FLOG_ERROR("DXC Error: %s", pErrors->GetStringPointer());
        return {};
    }

    CComPtr<IDxcBlob> pSpirv;
    pCompileResult->GetResult(&pSpirv);

    uint32_t spirvSize = pSpirv->GetBufferSize() / sizeof(uint32_t); // always a multiple of 4

    return std::vector<uint32_t>((uint32_t*)pSpirv->GetBufferPointer(), (uint32_t*)pSpirv->GetBufferPointer() + spirvSize);
}

std::vector<uint32_t> ShaderCompiler::CompileSPIRVFromFile(const std::string& file)
{
    std::vector<char> shaderCode;
    FileRead(file, shaderCode);
    return CompileSPIRVFromSource(shaderCode);
}

ShaderCompiler::~ShaderCompiler()
{
}