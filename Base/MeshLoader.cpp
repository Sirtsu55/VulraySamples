#include "GLTFTypes.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "MeshLoader.h"

Scene MeshLoader::LoadGLBMesh(const std::string& path)
{
    Scene outScene = {};

    auto& outMeshes = outScene.Meshes;

    tinygltf::Model model;

    std::string err;
    std::string warn;

    mLoader.LoadBinaryFromFile(&model, &err, &warn, path);
    if (!warn.empty())
        std::cout << warn << std::endl;
    if (!err.empty())
        std::cout << err << std::endl;

    for(auto& mesh : model.meshes)
    {
        for(auto& primitive : mesh.primitives)
        {
            outMeshes.push_back(Mesh{});
            auto& outMesh = outMeshes.back();
            // Get indices
            auto indices = primitive.indices;
            if (indices != -1)
            {
                auto& indicesAccessor = model.accessors[indices];
                auto& indicesView = model.bufferViews[indicesAccessor.bufferView];
                auto& indicesBuffer = model.buffers[indicesView.buffer];
                auto& indicesData = indicesBuffer.data;
                outMesh.Indices.resize(indicesView.byteLength);
                memcpy(outMesh.Indices.data(), indicesData.data() + indicesView.byteOffset, indicesView.byteLength);

                auto components = GetComponentsFromTinyGLTFType(indicesAccessor.type);
                auto size = GetSizeFromType(indicesAccessor.componentType);
                outMesh.IndexSize = components * size;
                outMesh.IndexFormat = ConvertToIndexType(indicesAccessor.componentType);
            }
            // Get positions
            auto positions = primitive.attributes.find("POSITION");
            if (positions != primitive.attributes.end())
            {
                auto& positionsAccessor = model.accessors[positions->second];
                auto& positionsView = model.bufferViews[positionsAccessor.bufferView];
                auto& positionsBuffer = model.buffers[positionsView.buffer];
                auto& positionsData = positionsBuffer.data;
                outMesh.Vertices.resize(positionsView.byteLength);
                memcpy(outMesh.Vertices.data(), positionsData.data() + positionsView.byteOffset, positionsView.byteLength);

                auto components = GetComponentsFromTinyGLTFType(positionsAccessor.type);
                auto size = GetSizeFromType(positionsAccessor.componentType);
                outMesh.VertexSize = components * size;
                outMesh.VertexFormat = ConvertToVkFormat(components, positionsAccessor.componentType);
            }
        }

    }
    
    return std::move(outScene);
}

