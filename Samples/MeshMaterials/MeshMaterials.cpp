#include "Vulray/Vulray.h"
#include "Common.h"
#include "Application.h"
#include "FileRead.h"
#include "ShaderCompiler.h"
#include "MeshLoader.h"
#include <filesystem>

enum class MaterialType : uint32_t
{
    Opaque = 0, // opaque material
    Emissive = 1 // emissive material
};

struct GPUMaterial // has to be aligned to 16 bytes
{
    glm::vec3 BaseColor = glm::vec3(1.0f);
    float Metallic = 1.0f;
    
    glm::vec3 Emissive = glm::vec3(0.0f);
    float Roughness = 1.0f;
    MaterialType Type; // add padding to make sure the struct is 16 byte aligned

    float Padding[3];
}; 

class MeshMaterials : public Application
{
public:
    virtual void Start() override;
    virtual void Update(vk::CommandBuffer renderCmd) override;
    virtual void Stop() override;

    //functions to break up the start function
    void CreateAS();
    void CreateRTPipeline();
    void UpdateDescriptorSet();

public:
    MeshLoader mMeshLoader;
    
    ShaderCompiler mShaderCompiler;
    
    vr::AllocatedBuffer mVertexBuffer;
    vr::AllocatedBuffer mIndexBuffer;
    vr::AllocatedBuffer mTransformBuffer;

    std::vector<vr::DescriptorItem> mResourceBindings;
    vk::DescriptorSetLayout mResourceDescriptorLayout;
    vr::DescriptorBuffer mResourceDescBuffer;


    vr::AllocatedBuffer mMaterialBuffer;

    vr::ShaderBindingTable mSBT;    // contains the raygen, miss and hit groups
	vr::SBTBuffer mSBTBuffer;       // contains the shader records for the SBT

    vk::Pipeline mRTPipeline = nullptr;
    vk::PipelineLayout mPipelineLayout = nullptr;

    std::vector<vr::BLASHandle> mBLASHandles;
    vr::TLASHandle mTLASHandle;


};

void MeshMaterials::Start()
{
    //defined in the base application class, creates an output image to render to and a camera uniform buffer
    CreateBaseResources();
    
    CreateAS();
    
    CreateRTPipeline();
    UpdateDescriptorSet();

}

void MeshMaterials::CreateAS()
{
    mMeshLoader = MeshLoader();
    // Get the scene info from the glb file
    auto scene = mMeshLoader.LoadGLBMesh("Assets/cornell_box2.glb");

    // Set the camera position to the center of the scene
    if(scene.Cameras.size() > 0)
        mCamera = scene.Cameras[0];
    mCamera.Speed = 25.0f;

    auto& geometries = scene.Geometries;
   
    uint32_t vertBufferSize = 0;
    uint32_t idxBufferSize = 0;
    uint32_t transBufferSize = 0;
    uint32_t matBufferSize = 0;

    // calculate the size required for the buffers
    for (auto& mesh : scene.Meshes)
    {
        for (auto& geomRef : mesh.GeometryReferences)
        {
            vertBufferSize += geometries[geomRef].Vertices.size();
            vertBufferSize += sizeof(uint32_t);
            idxBufferSize += geometries[geomRef].Indices.size();
            matBufferSize += sizeof(GPUMaterial);
        }
        transBufferSize += sizeof(vk::TransformMatrixKHR);
    }

    // Store all the primitives in a single buffer, it is efficient to do so
    mVertexBuffer = mVRDev->CreateBuffer(
        vertBufferSize, 
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR
    );

    mIndexBuffer = mVRDev->CreateBuffer(
        idxBufferSize, 
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR
    );

    mMaterialBuffer = mVRDev->CreateBuffer(
        matBufferSize, 
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR
    );

    // Create a buffer to store the transform for the BLAS
    mTransformBuffer = mVRDev->CreateBuffer(
        transBufferSize, 
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR
    );


	// Create info struct for the BLAS
    vr::BLASCreateInfo blasCreateInfo = {};
    std::vector<vr::BLASCreateInfo> blasCreateInfos;

    // Store the instanceID for each mesh so that we can use it to index into the material buffer
    std::vector<uint32_t> instanceIDs;

    // Copy the vertex and index data into the buffers
    uint32_t vertOffset = 0;
    uint32_t idxOffset = 0;
    uint32_t transOffset = 0;
    uint32_t matOffset = 0;

    char* vertData = (char*)mVRDev->MapBuffer(mVertexBuffer);
    char* idxData = (char*)mVRDev->MapBuffer(mIndexBuffer);
    char* transData = (char*)mVRDev->MapBuffer(mTransformBuffer);
    char* matData = (char*)mVRDev->MapBuffer(mMaterialBuffer);

    for (auto& mesh : scene.Meshes)
    {
        auto& blasinfo = blasCreateInfos.emplace_back(vr::BLASCreateInfo{});
        blasinfo.Flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
        // [POI]
        // The materials are stored like this  
        // The instance ID for the TLAS is n Meshes + n Geometries
        // if Mesh at index 0 has 2 geometries, the instance ID for the first geometry is 0 and the second Mesh is 2, because there 
        // are 2 materials before the second mesh, because Mesh 0 has 2 geometries.
        // Similarly, if Mesh at index 1 has 3 geometries, the next Mesh Instance ID is 2(Geometries) + 3(Geometries) = 5
        // This is the calculation for the instance ID
        instanceIDs.push_back(matOffset / sizeof(GPUMaterial)); // store the instanceID for the mesh

        for (auto& geomRef : mesh.GeometryReferences)
        {
			auto& geom = geometries[geomRef];
			vr::GeometryData geomData = {};
			geomData.VertexFormat = geom.VertexFormat;
			geomData.Stride = geom.VertexSize;
			geomData.IndexFormat = geom.IndexFormat;
			geomData.PrimitiveCount = geom.Indices.size() / geom.IndexSize / 3;
			geomData.DataAddresses.VertexDevAddress = mVertexBuffer.DevAddress + vertOffset;
			geomData.DataAddresses.IndexDevAddress = mIndexBuffer.DevAddress + idxOffset;
			geomData.DataAddresses.TransformBuffer = mTransformBuffer.DevAddress;
			blasinfo.Geometries.push_back(geomData);
			
            GPUMaterial mat = {}; // create a material for the geometry this material will be copied into the material buffer
            mat.BaseColor = geom.Material.BaseColorFactor;
            mat.Roughness = geom.Material.RoughnessFactor;
            mat.Metallic = geom.Material.MetallicFactor;
            mat.Type = MaterialType::Emissive;

			memcpy(vertData + vertOffset, geom.Vertices.data(), geom.Vertices.size());
			memcpy(idxData + idxOffset, geom.Indices.data(), geom.Indices.size());
            memcpy(matData + matOffset, &mat, sizeof(GPUMaterial));

			vertOffset += geom.Vertices.size(); // the size is in bytes
			idxOffset += geom.Indices.size();
            matOffset += sizeof(GPUMaterial); // material for each geometry
		}

        memcpy(transData + transOffset, &mesh.Transform, sizeof(vk::TransformMatrixKHR));
        transOffset += sizeof(vk::TransformMatrixKHR);
    }

    mVRDev->UnmapBuffer(mVertexBuffer);
    mVRDev->UnmapBuffer(mIndexBuffer);
    mVRDev->UnmapBuffer(mTransformBuffer);
    mVRDev->UnmapBuffer(mMaterialBuffer);

    // Create the BLASes, one BLAS for each mesh in the scene
    // create build info for the BLASSes
    std::vector<vr::BLASBuildInfo> buildInfos; 

    mBLASHandles.reserve(blasCreateInfos.size());
    buildInfos.reserve(blasCreateInfos.size());

    for (auto& info : blasCreateInfos)
    {
        auto& blas = mBLASHandles.emplace_back(vr::BLASHandle{});
        auto& buildInfo = buildInfos.emplace_back(vr::BLASBuildInfo{});  
        std::tie(blas, buildInfo) = mVRDev->CreateBLAS(info);
    }


    vr::TLASCreateInfo tlasCreateInfo = {};
    tlasCreateInfo.Flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
    tlasCreateInfo.MaxInstanceCount = mBLASHandles.size(); 

    auto [tlasHandle, tlasBuildInfo] = mVRDev->CreateTLAS(tlasCreateInfo);

    mTLASHandle = tlasHandle;

    auto InstanceBuffer = mVRDev->CreateInstanceBuffer(mBLASHandles.size()); 

    std::vector<vk::AccelerationStructureInstanceKHR> instances;

    // Create as many instances as there are BLASes
    for (uint32_t i = 0; i < mBLASHandles.size(); i++)
    {
        auto inst = vk::AccelerationStructureInstanceKHR()
            .setInstanceCustomIndex(instanceIDs[i]) // set the instance ID 
            .setAccelerationStructureReference(mBLASHandles[i].BLASBuffer.DevAddress)
            .setFlags(vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable)
            .setMask(0xFF)
            .setInstanceShaderBindingTableRecordOffset(0);

        // set the transform matrix to identity
        inst.transform = {
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f
        };

        instances.push_back(inst);
    }

    mVRDev->UpdateBuffer(InstanceBuffer, instances.data(), sizeof(vk::AccelerationStructureInstanceKHR) * instances.size());

    // create the scratch buffers
    auto BLASscratchBuffer = mVRDev->CreateScratchBufferBLAS(buildInfos);
    auto TLASScratchBuffer = mVRDev->CreateScratchBufferTLAS(tlasBuildInfo);

    auto buildCmd = mVRDev->CreateCommandBuffer(mGraphicsPool); 


    buildCmd.begin(vk::CommandBufferBeginInfo().setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

    // build the AS

    mVRDev->BuildBLAS(buildInfos, buildCmd); 

    mVRDev->AddAccelerationBuildBarrier(buildCmd); 


    mVRDev->BuildTLAS(tlasBuildInfo, InstanceBuffer, instances.size(), buildCmd); 

    buildCmd.end();

    // submit the command buffer and wait for it to finish
    auto submitInfo = vk::SubmitInfo()
        .setCommandBufferCount(1)
        .setPCommandBuffers(&buildCmd);

    mQueues.GraphicsQueue.submit(submitInfo, nullptr);
    
    mDevice.waitIdle();

    mVRDev->DestroyBuffer(BLASscratchBuffer); 
    mVRDev->DestroyBuffer(TLASScratchBuffer);

    mVRDev->DestroyBuffer(InstanceBuffer);

    mDevice.freeCommandBuffers(mGraphicsPool, buildCmd);
}


void MeshMaterials::CreateRTPipeline()
{
    mResourceBindings = {
        vr::DescriptorItem(0, vk::DescriptorType::eAccelerationStructureKHR, vk::ShaderStageFlagBits::eRaygenKHR, 1, &mTLASHandle.TLASBuffer.DevAddress),
        vr::DescriptorItem(1, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eRaygenKHR, 1, &mCameraUniformBuffer),
        vr::DescriptorItem(2, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eRaygenKHR, 1, &mOutputImage),
        vr::DescriptorItem(3, vk::DescriptorType::eStorageBuffer,
        vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eCallableKHR,
        1, &mMaterialBuffer)
    };


    mResourceDescriptorLayout = mVRDev->CreateDescriptorSetLayout(mResourceBindings); 
    
    vr::ShaderCreateInfo shaderCreateInfo = {};

    shaderCreateInfo.SPIRVCode = mShaderCompiler.CompileSPIRVFromFile(vk::ShaderStageFlagBits::eRaygenKHR, "Shaders/MultipleGeometriesBLAS/BLASShading.hlsl");
    auto shaderModule = mVRDev->CreateShaderFromSPV(shaderCreateInfo);

    mSBT.RayGenShader = shaderModule;
    mSBT.RayGenShader.Stage = vk::ShaderStageFlagBits::eRaygenKHR;
    mSBT.RayGenShader.EntryPoint = "rgen"; 


    mSBT.MissShaders.push_back(shaderModule);
    mSBT.MissShaders.back().Stage = vk::ShaderStageFlagBits::eMissKHR;
    mSBT.MissShaders.back().EntryPoint = "miss";

    // [POI]
    // Add Callable shaders so that we can shade the emissive materials and opaque materials 
    // Callable shader are an efficient way to split up work in a workgroup without if-else statements that would 
    // otherwise hinder the ray tracing performance

    // Callable Shader at index 0 is the opaque material
    mSBT.CallableShaders.push_back(shaderModule);
    mSBT.CallableShaders.back().Stage = vk::ShaderStageFlagBits::eCallableKHR;
    mSBT.CallableShaders.back().EntryPoint = "opqshading";

    // Callable Shader at index 1 is the opaque material
    mSBT.CallableShaders.push_back(shaderModule);
    mSBT.CallableShaders.back().Stage = vk::ShaderStageFlagBits::eCallableKHR;
    mSBT.CallableShaders.back().EntryPoint = "eshading";


    vr::HitGroup hitGroup = {};
    hitGroup.ClosestHitShader = shaderModule;
    hitGroup.ClosestHitShader.EntryPoint = "chit";
    hitGroup.ClosestHitShader.Stage = vk::ShaderStageFlagBits::eClosestHitKHR;
    mSBT.HitGroups.push_back(hitGroup);
    

    // Create a layout for the pipeline
    mPipelineLayout = mVRDev->CreatePipelineLayout(mResourceDescriptorLayout);

    // create the ray tracing pipeline, a vk::Pipeline object
    mRTPipeline = mVRDev->CreateRayTracingPipeline(mPipelineLayout, mSBT, 1);

    mSBTBuffer = mVRDev->CreateSBT(mRTPipeline, mSBT);

    // create a descriptor buffer for the ray tracing pipeline
    mResourceDescBuffer = mVRDev->CreateDescriptorBuffer(mResourceDescriptorLayout, mResourceBindings, vr::DescriptorBufferType::Resource);


}


void MeshMaterials::UpdateDescriptorSet()
{
    mVRDev->UpdateDescriptorBuffer(mResourceDescBuffer, mResourceBindings, vr::DescriptorBufferType::Resource);
}

void MeshMaterials::Update(vk::CommandBuffer renderCmd)
{
    // begin the command buffer
    renderCmd.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));


    mVRDev->BindDescriptorBuffer({ mResourceDescBuffer }, renderCmd);

    mVRDev->BindDescriptorSet(mPipelineLayout, 0, 0, 0, renderCmd);

    mVRDev->TransitionImageLayout(
        mOutputImageBuffer.Image,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eGeneral,
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1),
        renderCmd);

    renderCmd.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, mRTPipeline);

    mVRDev->DispatchRays(renderCmd, mRTPipeline, mSBTBuffer, mWidth, mHeight);

    mVRDev->TransitionImageLayout(mSwapchainStructs.SwapchainImages[mCurrentSwapchainImage],
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eTransferDstOptimal,
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1),
        renderCmd);

    mVRDev->TransitionImageLayout(mOutputImageBuffer.Image,
        vk::ImageLayout::eGeneral,
        vk::ImageLayout::eTransferSrcOptimal,
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1),
        renderCmd);

    renderCmd.blitImage(
        mOutputImageBuffer.Image, vk::ImageLayout::eTransferSrcOptimal,
        mSwapchainStructs.SwapchainImages[mCurrentSwapchainImage], vk::ImageLayout::eTransferDstOptimal,
        vk::ImageBlit(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
            { vk::Offset3D(0, 0, 0), vk::Offset3D(mWidth, mHeight, 1) },
            vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
            { vk::Offset3D(0, 0, 0), vk::Offset3D(mWidth, mHeight, 1) }),
        vk::Filter::eNearest);
    
    mVRDev->TransitionImageLayout(mOutputImageBuffer.Image,
        vk::ImageLayout::eTransferSrcOptimal,
        vk::ImageLayout::eGeneral,
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1),
        renderCmd);

    mVRDev->TransitionImageLayout(mSwapchainStructs.SwapchainImages[mCurrentSwapchainImage],
        vk::ImageLayout::eTransferDstOptimal,
        vk::ImageLayout::ePresentSrcKHR,
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1),
        renderCmd);

    renderCmd.end();



    WaitForRendering();

    Present(renderCmd);

    UpdateCamera();

}


void MeshMaterials::Stop()
{
    auto _ = mDevice.waitForFences(mRenderFence, VK_TRUE, UINT64_MAX);
    
    // destroy all the resources we created
    mVRDev->DestroySBTBuffer(mSBTBuffer);
    // this was the shader module for all of the shaders in the SBT
    mVRDev->DestroyShader(mSBT.RayGenShader); 

    mDevice.destroyPipeline(mRTPipeline);
    mDevice.destroyPipelineLayout(mPipelineLayout);

    mDevice.destroyDescriptorSetLayout(mResourceDescriptorLayout);
    mVRDev->DestroyBuffer(mResourceDescBuffer.Buffer);

    mVRDev->DestroyBuffer(mVertexBuffer);
    mVRDev->DestroyBuffer(mIndexBuffer);
    mVRDev->DestroyBuffer(mTransformBuffer);
    mVRDev->DestroyBuffer(mMaterialBuffer);

    for(auto& blas : mBLASHandles)
        mVRDev->DestroyBLAS(blas);

    mVRDev->DestroyTLAS(mTLASHandle);
}


int main()
{
    // Create the application, start it, run it and stop it, boierplate code, eg initialising vulkan, glfw, etc
    // that is the same for every application is handled by the Application class
    // it can be found in the Base folder
	Application* app = new MeshMaterials();

    app->Start();
    app->Run();
    app->Stop();

    delete app;
}