#include <vulkan/vulkan.hpp>
#include <shaderc/shaderc.hpp>


class ShaderCompiler
{
public:
    ShaderCompiler();

    ~ShaderCompiler();

    std::vector<uint32_t> CompileSPIRVFromSource(const vk::ShaderStageFlagBits stage, const std::vector<char>& source);
    std::vector<uint32_t> CompileSPIRVFromFile(const vk::ShaderStageFlagBits stage, const std::string& file);
private:

    shaderc::Compiler mCompiler = {};
    shaderc::CompileOptions mOptions = {};
};


