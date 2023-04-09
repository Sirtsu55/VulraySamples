#include "ShaderCompiler.h"
#include "FileRead.h"
#include "Vulray/Vulray.h"

static shaderc_shader_kind GetShaderKindFromShaderStage(const vk::ShaderStageFlagBits stage)
{
    switch (stage)
    {
    case vk::ShaderStageFlagBits::eRaygenKHR:
        return shaderc_raygen_shader;
    case vk::ShaderStageFlagBits::eClosestHitKHR:
        return shaderc_closesthit_shader;
    case vk::ShaderStageFlagBits::eAnyHitKHR:
        return shaderc_anyhit_shader;
    case vk::ShaderStageFlagBits::eIntersectionKHR:
        return shaderc_intersection_shader;
    case vk::ShaderStageFlagBits::eMissKHR:
        return shaderc_miss_shader;
    case vk::ShaderStageFlagBits::eCallableKHR:
        return shaderc_callable_shader;
    default:
        return shaderc_glsl_infer_from_source;
    }
}


ShaderCompiler::ShaderCompiler()
{
    mOptions.SetOptimizationLevel(shaderc_optimization_level_performance);
    mOptions.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
}


std::vector<uint32_t> ShaderCompiler::CompileSPIRVFromSource(const vk::ShaderStageFlagBits stage, const std::vector<char>& source)
{
    
    //Compile the shader
    shaderc::SpvCompilationResult result = mCompiler.CompileGlslToSpv(
        source.data(),
        source.size(),
        GetShaderKindFromShaderStage(stage),
        "Shader",
        "main",
        mOptions
    );

    //Check if the compilation was successful
    if (result.GetCompilationStatus() != shaderc_compilation_status_success)
    {
        VULRAY_LOG_ERROR("Failed to compile shader: {0}", result.GetErrorMessage());
        return std::vector<uint32_t>();
    }
    const uint32_t* spirv = result.cbegin();
    return { result.begin(), result.end() };
}

std::vector<uint32_t> ShaderCompiler::CompileSPIRVFromFile(const vk::ShaderStageFlagBits stage, const std::string& file)
{
    std::vector<char> shaderCode;
    FileRead(file, shaderCode);
    return CompileSPIRVFromSource(stage, shaderCode);
}

ShaderCompiler::~ShaderCompiler()
{
}