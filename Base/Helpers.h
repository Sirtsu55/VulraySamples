#include "MeshLoader.h"
#include "Vulray/Vulray.h"

void CalculateBufferSizes(const Scene& scene,
    uint32_t& outVertexBufferSize,
    uint32_t& outIndexBufferSize,
    uint32_t& outTransformBufferSize,
    uint32_t& outMaterialBufferSize)
{
    auto& geometries = scene.Geometries;

    // calculate the size required for the buffers
    for (auto& mesh : scene.Meshes)
    {
        for (auto& geomRef : mesh.GeometryReferences)
        {
            outVertexBufferSize += geometries[geomRef].Vertices.size();
            outIndexBufferSize += sizeof(uint32_t);
            outIndexBufferSize += geometries[geomRef].Indices.size();
            outMaterialBufferSize += sizeof(GPUMaterial);
        }
        outTransformBufferSize += sizeof(vk::TransformMatrixKHR);
    }
}

void CopySceneToBuffers(
    const Scene& scene,
    char* vertexBuffer, 
    char* indexBuffer,
    char* transformBuffer,
    char* materialBuffer,
    vk::DeviceAddress vertexBufferDevAddress,
    vk::DeviceAddress indexBufferDevAddress,
    vk::DeviceAddress transformBufferDevAddress,
    std::vector<uint32_t>& outInsanceIDs,
    std::vector<vr::BLASCreateInfo>& outBlasCreateInfos,
    vk::BuildAccelerationStructureFlagBitsKHR flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace)
{
    auto& geometries = scene.Geometries;

    // copy the scene data to the buffers
    uint32_t vertexOffset = 0;
    uint32_t indexOffset = 0;
    uint32_t transformOffset = 0;
    uint32_t materialOffset = 0;
    for (auto& mesh : scene.Meshes)
    {
        auto& blasinfo = outBlasCreateInfos.emplace_back(vr::BLASCreateInfo{});
        blasinfo.Flags = flags;
        
        outInsanceIDs.push_back(materialOffset / sizeof(GPUMaterial));

        for (auto& geomRef : mesh.GeometryReferences)
        {
			auto& geom = geometries[geomRef];
			vr::GeometryData geomData = {};
			geomData.VertexFormat = geom.VertexFormat;
			geomData.Stride = geom.VertexSize;
			geomData.IndexFormat = geom.IndexFormat;
			geomData.PrimitiveCount = geom.Indices.size() / geom.IndexSize / 3;
			geomData.DataAddresses.VertexDevAddress = vertexBufferDevAddress + vertexOffset;
			geomData.DataAddresses.IndexDevAddress = indexBufferDevAddress + indexOffset;
			geomData.DataAddresses.TransformBuffer = transformBufferDevAddress + transformOffset;
			blasinfo.Geometries.push_back(geomData);
			
            GPUMaterial mat = {}; // create a material for the geometry this material will be copied into the material buffer
            mat.BaseColor = geom.Material.BaseColorFactor;
            mat.Roughness = geom.Material.RoughnessFactor;
            mat.Metallic = geom.Material.MetallicFactor;
            mat.Type = MaterialType::Emissive;

			memcpy(vertexBuffer + vertexOffset, geom.Vertices.data(), geom.Vertices.size());
			memcpy(indexBuffer + indexOffset, geom.Indices.data(), geom.Indices.size());
            memcpy(materialBuffer + materialOffset, &mat, sizeof(GPUMaterial));

			vertexOffset += geom.Vertices.size(); // the size is in bytes
			indexOffset += geom.Indices.size();
            materialOffset += sizeof(GPUMaterial); // material for each geometry
		}

        memcpy(transformBuffer + transformOffset, &mesh.Transform, sizeof(vk::TransformMatrixKHR));
        transformOffset += sizeof(vk::TransformMatrixKHR);
    }

}