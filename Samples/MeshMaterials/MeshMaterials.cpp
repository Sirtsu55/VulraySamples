#include "Vulray/Vulray.h"
#include "Common.h"
#include "Application.h"
#include "FileRead.h"
#include "ShaderCompiler.h"
#include "MeshLoader.h"
#include "GPUMaterial.h"


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
    auto scene = mMeshLoader.LoadGLBMesh("Assets/cornell_box.glb");

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
            vertBufferSize += geometries[geomRef].Vertices.size() * sizeof(Vertex);
            idxBufferSize += geometries[geomRef].Indices.size() * sizeof(uint32_t);
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
        vk::BufferUsageFlagBits::eStorageBuffer
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

    Vertex* vertData = (Vertex*)mVRDev->MapBuffer(mVertexBuffer);
    uint32_t* idxData = (uint32_t*)mVRDev->MapBuffer(mIndexBuffer);
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
			geomData.VertexFormat = vk::Format::eR32G32B32Sfloat;
			geomData.Stride = sizeof(Vertex);
			geomData.IndexFormat = vk::IndexType::eUint32;
			geomData.PrimitiveCount = geom.Indices.size() / 3;
			geomData.DataAddresses.VertexDevAddress = mVertexBuffer.DevAddress + vertOffset * sizeof(Vertex);
			geomData.DataAddresses.IndexDevAddress = mIndexBuffer.DevAddress + idxOffset * sizeof(uint32_t);
			geomData.DataAddresses.TransformDevAddress = mTransformBuffer.DevAddress + transOffset;
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
            .setAccelerationStructureReference(mBLASHandles[i].Buffer.DevAddress)
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
    auto BLASscratchBuffer = mVRDev->CreateScratchBufferFromBuildInfos(buildInfos);
    auto TLASScratchBuffer = mVRDev->CreateScratchBufferFromBuildInfo(tlasBuildInfo);

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
        vr::DescriptorItem(0, vk::DescriptorType::eAccelerationStructureKHR, vk::ShaderStageFlagBits::eRaygenKHR, 1, &mTLASHandle.Buffer.DevAddress),
        vr::DescriptorItem(1, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eRaygenKHR, 1, &mUniformBuffer),
        vr::DescriptorItem(2, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eRaygenKHR, 1, &mOutputImage),
        vr::DescriptorItem(3, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eClosestHitKHR, 1, &mMaterialBuffer)
    };


    mResourceDescriptorLayout = mVRDev->CreateDescriptorSetLayout(mResourceBindings); 
    
    mPipelineLayout = mVRDev->CreatePipelineLayout(mResourceDescriptorLayout);

    auto spv = mShaderCompiler.CompileSPIRVFromFile("Shaders/ColorfulGeometry/ColorfulGeometry.hlsl");
    auto shaderModule = mVRDev->CreateShaderFromSPV(spv);

    vr::PipelineSettings pipelineSettings = {};
    pipelineSettings.PipelineLayout = mPipelineLayout;
    pipelineSettings.MaxRecursionDepth = 1;
    pipelineSettings.MaxPayloadSize = sizeof(glm::vec3);
    pipelineSettings.MaxHitAttributeSize = sizeof(glm::vec2);

    vr::RayTracingShaderCollection shaderCollection = {};

    shaderCollection.RayGenShaders.push_back(shaderModule);
    shaderCollection.RayGenShaders.back().EntryPoint = "rgen"; 

    shaderCollection.MissShaders.push_back(shaderModule);
    shaderCollection.MissShaders.back().EntryPoint = "miss";

    vr::HitGroup hitGroup = {};
    hitGroup.ClosestHitShader = shaderModule;
    hitGroup.ClosestHitShader.EntryPoint = "chit";
    shaderCollection.HitGroups.push_back(hitGroup);

    auto[pipeline, sbtInfo] = mVRDev->CreateRayTracingPipeline(shaderCollection, pipelineSettings);
    mRTPipeline = pipeline;

    mSBTBuffer = mVRDev->CreateSBT(mRTPipeline, sbtInfo);

    mResourceDescBuffer = mVRDev->CreateDescriptorBuffer(mResourceDescriptorLayout, mResourceBindings, vr::DescriptorBufferType::Resource);
    
    mDevice.destroyShaderModule(shaderModule.Module);
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

    mVRDev->DispatchRays(renderCmd, mRTPipeline, mSBTBuffer, mWindowWidth, mWindowHeight);

    // Helper function in Application Class to blit the image to the swapchain image
    BlitImage(renderCmd);

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