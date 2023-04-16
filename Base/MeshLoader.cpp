#include "GLTFTypes.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "MeshLoader.h"

Scene MeshLoader::LoadGLBMesh(const std::string& path)
{
    Scene outScene = {};

    tinygltf::Model model;

    std::string err;
    std::string warn;

    mLoader.LoadBinaryFromFile(&model, &err, &warn, path);
    if (!warn.empty())
        std::cout << warn << std::endl;
    if (!err.empty())
        std::cout << err << std::endl;

    for (auto& node : model.nodes)
    {
        glm::mat4 matrix = glm::mat4(1.0f);
        glm::vec3 translation = glm::vec3(0.0f);
        glm::vec3 scale = glm::vec3(1.0f);
        glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

        if(node.matrix.size() == 16)
            matrix = node.matrix.size() == 16 ? glm::make_mat4(node.matrix.data()) : glm::dmat4(1.0f);
        if(node.rotation.size() == 4)
        {
            rotation = glm::quat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
			matrix = glm::mat4_cast(rotation);
        }
        if(node.translation.size() == 3)
        {
            translation = glm::make_vec3(node.translation.data());
            matrix = glm::translate(matrix, translation);
        }
        if(node.scale.size() == 3)
        {
            scale = glm::make_vec3(node.scale.data());
            matrix = glm::scale(matrix, scale);
        }

        if (node.mesh != -1)
        {
            AddMeshToScene(model.meshes[node.mesh], model, outScene);
            outScene.Meshes.back().Transform = glm::rowMajor4(matrix);
        }
        if(node.camera != -1)
        {
            auto& camera = model.cameras[node.camera];
            auto& outCamera = outScene.Cameras.emplace_back(Camera{});
            outCamera.Fov = glm::degrees(camera.perspective.yfov);
            outCamera.AspectRatio = camera.perspective.aspectRatio;
            outCamera.NearPlane = camera.perspective.znear;
            outCamera.FarPlane = camera.perspective.zfar;
            outCamera.Rotation = rotation;
            outCamera.UpdateDirections();
            outCamera.Position = translation;
        }
    }
    return outScene;
}

void MeshLoader::AddMeshToScene(const tinygltf::Mesh& mesh, const tinygltf::Model& model, Scene& outScene)
{
    auto& outMesh = outScene.Meshes.emplace_back();
    for (auto& primitive : mesh.primitives)
    {
        // get index of the new geometry
        outMesh.GeometryReferences.push_back(outScene.Geometries.size());
        // add new geometry
        auto& outGeom = outScene.Geometries.emplace_back(Geometry{});

        // Get indices
        auto indices = primitive.indices;
        if (indices != -1)
        {
            auto& indicesAccessor = model.accessors[indices];
            auto& indicesView = model.bufferViews[indicesAccessor.bufferView];
            auto& indicesBuffer = model.buffers[indicesView.buffer];
            auto& indicesData = indicesBuffer.data;
            outGeom.Indices.resize(indicesView.byteLength);
            memcpy(outGeom.Indices.data(), indicesData.data() + indicesView.byteOffset, indicesView.byteLength);

            auto components = GetComponentsFromTinyGLTFType(indicesAccessor.type);
            auto size = GetSizeFromType(indicesAccessor.componentType);
            outGeom.IndexSize = components * size;
            outGeom.IndexFormat = ConvertToIndexType(indicesAccessor.componentType);
        }
        // Get positions
        auto positions = primitive.attributes.find("POSITION");
        if (positions != primitive.attributes.end())
        {
            auto& positionsAccessor = model.accessors[positions->second];
            auto& positionsView = model.bufferViews[positionsAccessor.bufferView];
            auto& positionsBuffer = model.buffers[positionsView.buffer];
            auto& positionsData = positionsBuffer.data;
            outGeom.Vertices.resize(positionsView.byteLength);
            memcpy(outGeom.Vertices.data(), positionsData.data() + positionsView.byteOffset, positionsView.byteLength);

            auto components = GetComponentsFromTinyGLTFType(positionsAccessor.type);
            auto size = GetSizeFromType(positionsAccessor.componentType);
            outGeom.VertexSize = components * size;
            outGeom.VertexFormat = ConvertToVkFormat(components, positionsAccessor.componentType);
        }
    }
}

