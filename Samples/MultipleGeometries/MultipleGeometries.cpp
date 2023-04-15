#include "Vulray/Vulray.h"
#include "Common.h"
#include "Application.h"
#include "FileRead.h"
#include "ShaderCompiler.h"
#include "MeshLoader.h"
#include <filesystem>


class MultipleGeometries : public Application
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

    std::vector<vr::DescriptorItem> mResourceBindings;

    vk::DescriptorSetLayout mResourceDescriptorLayout;
    
    vr::DescriptorBuffer mResourceDescBuffer;

    vr::ShaderBindingTable mSBT;    // contains the raygen, miss and hit groups
	vr::SBTBuffer mSBTBuffer;       // contains the shader records for the SBT

    vk::Pipeline mRTPipeline = nullptr;
    vk::PipelineLayout mPipelineLayout = nullptr;

    vr::BLASHandle mBLASHandle;
    vr::TLASHandle mTLASHandle;


};

void MultipleGeometries::Start()
{
    //defined in the base application class, creates an output image to render to and a camera uniform buffer
    CreateBaseResources();
    
    CreateAS();
    
    CreateRTPipeline();
    UpdateDescriptorSet();

}

void MultipleGeometries::CreateAS()
{
    mMeshLoader = MeshLoader();
    auto scene = mMeshLoader.LoadGLBMesh("Assets/cornell_box.glb");

    if(scene.Cameras.size() != 0)
        mCamera = scene.Cameras[0];
    else
        mCamera.Position = glm::vec3(0.0f, 0.0f, 5.0f);


    auto& meshes = scene.Meshes;


    uint32_t vertBufferSize = 0;
    uint32_t idxBufferSize = 0;

    // calculate the size of the buffers
    for (auto& m : meshes)
        vertBufferSize += m.Vertices.size();
    for (auto& m : meshes)
        idxBufferSize += m.Indices.size();

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

    
	// Create info struct for the BLAS
    vr::BLASCreateInfo blasCreateInfo = {};
    blasCreateInfo.Flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
    
    // Copy the vertex and index data into the buffers
    uint32_t vertOffset = 0;
    uint32_t idxOffset = 0;
    char* vertData = (char*)mVRDev->MapBuffer(mVertexBuffer);
    char* idxData = (char*)mVRDev->MapBuffer(mIndexBuffer);
    for (auto& m : meshes)
    {
        vr::GeometryData geomData = {};
        geomData.VertexFormat = m.VertexFormat;
        geomData.Stride = m.VertexSize;
        geomData.IndexFormat = m.IndexFormat;
        geomData.PrimitiveCount = m.Indices.size() / m.IndexSize / 3;
        geomData.DataAddresses.VertexDevAddress = mVertexBuffer.DevAddress + vertOffset;
        geomData.DataAddresses.IndexDevAddress = mIndexBuffer.DevAddress + idxOffset;
        blasCreateInfo.Geometries.push_back(geomData);
        
        memcpy(vertData + vertOffset, m.Vertices.data(), m.Vertices.size());
        memcpy(idxData + idxOffset, m.Indices.data(), m.Indices.size());
        vertOffset += m.Vertices.size();
        idxOffset += m.Indices.size();
    }
    mVRDev->UnmapBuffer(mVertexBuffer);
    mVRDev->UnmapBuffer(mIndexBuffer);


    auto[blasHandle, blasBuildInfo] = mVRDev->CreateBLAS(blasCreateInfo); 

    mBLASHandle = blasHandle;

    vr::TLASCreateInfo tlasCreateInfo = {};
    tlasCreateInfo.Flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
    tlasCreateInfo.MaxInstanceCount = 1; 

    auto [tlasHandle, tlasBuildInfo] = mVRDev->CreateTLAS(tlasCreateInfo);

    mTLASHandle = tlasHandle;

    auto InstanceBuffer = mVRDev->CreateInstanceBuffer(1); // 1 instance

    auto inst = vk::AccelerationStructureInstanceKHR()
        .setInstanceCustomIndex(0)
		.setAccelerationStructureReference(mBLASHandle.BLASBuffer.DevAddress)
		.setFlags(vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable)
        .setMask(0xFF)
        .setInstanceShaderBindingTableRecordOffset(0);

    // set the transform matrix to identity
    inst.transform = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f
    };

    mVRDev->UpdateBuffer(InstanceBuffer, &inst, sizeof(vk::AccelerationStructureInstanceKHR), 0);


    auto buildCmd = mVRDev->CreateCommandBuffer(mGraphicsPool); 

    buildCmd.begin(vk::CommandBufferBeginInfo().setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

    // build the AS
    std::vector<vr::BLASBuildInfo> buildInfos = { blasBuildInfo }; 

    auto BLASscratchBuffer = mVRDev->BuildBLAS(buildInfos, buildCmd); 

    mVRDev->AddAccelerationBuildBarrier(buildCmd); 

    auto TLASScratchBuffer = mVRDev->BuildTLAS(tlasBuildInfo, InstanceBuffer, 1, buildCmd); 

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


void MultipleGeometries::CreateRTPipeline()
{
    mResourceBindings = {
        vr::DescriptorItem(0, vk::DescriptorType::eAccelerationStructureKHR, vk::ShaderStageFlagBits::eRaygenKHR, 1, &mTLASHandle.TLASBuffer.DevAddress),
        vr::DescriptorItem(1, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eRaygenKHR, 1, &mCameraUniformBuffer),
        vr::DescriptorItem(2, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eRaygenKHR, 10, &mOutputImage, 1)
    };


    mResourceDescriptorLayout = mVRDev->CreateDescriptorSetLayout(mResourceBindings); 
    
    vr::ShaderCreateInfo shaderCreateInfo = {};

    shaderCreateInfo.SPIRVCode = mShaderCompiler.CompileSPIRVFromFile(vk::ShaderStageFlagBits::eRaygenKHR, "Shaders/ColorfulTriangle/ColorfulTriangle.hlsl");
    auto shaderModule = mVRDev->CreateShaderFromSPV(shaderCreateInfo);

    mSBT.RayGenShader = shaderModule;
    mSBT.RayGenShader.Stage = vk::ShaderStageFlagBits::eRaygenKHR;
    mSBT.RayGenShader.EntryPoint = "rgen"; 


    mSBT.MissShaders.push_back(shaderModule);
    mSBT.MissShaders.back().Stage = vk::ShaderStageFlagBits::eMissKHR;
    mSBT.MissShaders.back().EntryPoint = "miss";

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


void MultipleGeometries::UpdateDescriptorSet()
{
    mVRDev->UpdateDescriptorBuffer(mResourceDescBuffer, mResourceBindings, vr::DescriptorBufferType::Resource);
}

void MultipleGeometries::Update(vk::CommandBuffer renderCmd)
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


void MultipleGeometries::Stop()
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
    mVRDev->DestroyBLAS(mBLASHandle);
    mVRDev->DestroyTLAS(mTLASHandle);
}


int main()
{
    // Create the application, start it, run it and stop it, boierplate code, eg initialising vulkan, glfw, etc
    // that is the same for every application is handled by the Application class
    // it can be found in the Base folder
	Application* app = new MultipleGeometries();

    app->Start();
    app->Run();
    app->Stop();

    delete app;
}