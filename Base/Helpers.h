#pragma once

#include "MeshLoader.h"
#include "Vulray/Vulray.h"
#include "GPUMaterial.h"


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
            outVertexBufferSize += geometries[geomRef].Vertices.size() * sizeof(Vertex);
            outIndexBufferSize += geometries[geomRef].Indices.size() * sizeof(uint32_t);
            outMaterialBufferSize += sizeof(GPUMaterial);
        }
        outTransformBufferSize += sizeof(vk::TransformMatrixKHR);
    }
}

void CopySceneToBuffers(
    const Scene& scene,
    Vertex* vertData, 
    uint32_t* idxData,
    char* transData,
    char* matData,
    vk::DeviceAddress vertexBufferDevAddress,
    vk::DeviceAddress indexBufferDevAddress,
    vk::DeviceAddress transformBufferDevAddress,
    std::vector<uint32_t>& outInsanceIDs,
    std::vector<vr::BLASCreateInfo>& outBlasCreateInfos,
    vk::BuildAccelerationStructureFlagBitsKHR flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace)
{
    auto& geometries = scene.Geometries;

    // copy the scene data to the buffers
    uint32_t vertOffset = 0;
    uint32_t idxOffset = 0;
    uint32_t transOffset = 0;
    uint32_t matOffset = 0;

    for (auto& mesh : scene.Meshes)
    {
        auto& blasinfo = outBlasCreateInfos.emplace_back(vr::BLASCreateInfo{});
        blasinfo.Flags = flags;

        outInsanceIDs.push_back(matOffset / sizeof(GPUMaterial));

        for (auto& geomRef : mesh.GeometryReferences)
        {
			auto& geom = geometries[geomRef];
			vr::GeometryData geomData = {};
			geomData.VertexFormat = vk::Format::eR32G32B32Sfloat;
			geomData.Stride = sizeof(Vertex);
			geomData.IndexFormat = vk::IndexType::eUint32;
			geomData.PrimitiveCount = geom.Indices.size() / 3;
			geomData.DataAddresses.VertexDevAddress = vertexBufferDevAddress + vertOffset * sizeof(Vertex);
			geomData.DataAddresses.IndexDevAddress = indexBufferDevAddress + idxOffset * sizeof(uint32_t);
			geomData.DataAddresses.TransformBuffer = transformBufferDevAddress + transOffset;
			blasinfo.Geometries.push_back(geomData);
			
            GPUMaterial mat = {}; // create a material for the geometry this material will be copied into the material buffer
            mat.BaseColor = geom.Material.BaseColorFactor;
            mat.Roughness = geom.Material.RoughnessFactor;
            mat.Metallic = geom.Material.MetallicFactor;
            mat.VertBufferOffset = vertOffset;
            mat.IndexBufferOffset = idxOffset;

			memcpy(vertData + vertOffset, geom.Vertices.data(), geom.Vertices.size() * sizeof(Vertex));
			memcpy(idxData + idxOffset, geom.Indices.data(), geom.Indices.size() * sizeof(uint32_t));
            memcpy(matData + matOffset, &mat, sizeof(GPUMaterial));

			vertOffset += geom.Vertices.size();
			idxOffset += geom.Indices.size();
            matOffset += sizeof(GPUMaterial); // material for each geometry
		}

        memcpy(transData + transOffset, &mesh.Transform, sizeof(vk::TransformMatrixKHR));
        transOffset += sizeof(vk::TransformMatrixKHR);
    }

}