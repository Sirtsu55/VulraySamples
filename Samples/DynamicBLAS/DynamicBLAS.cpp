#include "Vulray/Vulray.h"
#include "Common.h"
#include "Application.h"
#include "FileRead.h"
#include "ShaderCompiler.h"


class DynamicBLAS : public Application
{
public:
    virtual void Start() override;
    virtual void Update(vk::CommandBuffer renderCmd) override;
    virtual void Stop() override;

    //functions to break up the start function
    void CreateAS();
    void CreateRTPipeline();
    void UpdateDescriptorSet();

    void UpdateBLAS(vk::CommandBuffer cmd);

public:

    ShaderCompiler mShaderCompiler;

    vr::AllocatedBuffer mVertexBuffer;
    vr::AllocatedBuffer mIndexBuffer;

    vr::ShaderBindingTable mSBT;    
	vr::SBTBuffer mSBTBuffer;      

    std::vector<vr::DescriptorItem> mResourceBindings;
    vk::DescriptorSetLayout mResourceDescriptorLayout;
    vr::DescriptorBuffer mResourceDescBuffer;

    vk::Pipeline mRTPipeline = nullptr;
    vk::PipelineLayout mPipelineLayout = nullptr;

    vr::BLASHandle mBLASHandle;

    // Save the build info for the BLAS so we can update it later
    vr::BLASBuildInfo mBLASBuildInfo;

    vr::AllocatedBuffer mUpdateScratchBuffer;

    vr::TLASHandle mTLASHandle;


};

void DynamicBLAS::Start()
{
    CreateBaseResources();
    
    CreateAS();
    
    CreateRTPipeline();
    UpdateDescriptorSet();

}

void DynamicBLAS::CreateAS()
{
    // vertex and index data for the triangle

    float vertices[] = {
        1.0f, 1.0f, 0.0f,
        -1.0f, 1.0f, 0.0f,
        0.0f,  -1.0f, 0.0f
    };
    uint32_t indices[] = { 0, 1, 2 };

    mVertexBuffer = mVRDev->CreateBuffer(
        sizeof(float) * 3 * 3,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR); // this buffer will be used as a source for the BLAS

    mIndexBuffer = mVRDev->CreateBuffer(
        sizeof(uint32_t) * 3, // 3 vertices, 3 floats per vertex
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR);


    mVRDev->UpdateBuffer(mVertexBuffer, vertices, sizeof(float) * 3 * 3);
    mVRDev->UpdateBuffer(mIndexBuffer, indices, sizeof(uint32_t) * 3); 
    

    vr::BLASCreateInfo blasCreateInfo = {};
    blasCreateInfo.Flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace | vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate;
    
    vr::GeometryData geomData = {};

    geomData.VertexFormat = vk::Format::eR32G32B32Sfloat;
    geomData.Stride = sizeof(float) * 3;
    geomData.IndexFormat = vk::IndexType::eUint32;
    geomData.PrimitiveCount = 1;
    geomData.DataAddresses.VertexDevAddress = mVertexBuffer.DevAddress;
    geomData.DataAddresses.IndexDevAddress = mIndexBuffer.DevAddress;

    blasCreateInfo.Geometries.push_back(geomData);

   std::tie(mBLASHandle, mBLASBuildInfo) = mVRDev->CreateBLAS(blasCreateInfo); 

    // create a TLAS
    vr::TLASCreateInfo tlasCreateInfo = {};
    tlasCreateInfo.Flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
    tlasCreateInfo.MaxInstanceCount = 1; // Max number of instances in the TLAS, when building the TLAS num of instances may be lower

    auto [tlasHandle, tlasBuildInfo] = mVRDev->CreateTLAS(tlasCreateInfo);

    mTLASHandle = tlasHandle;

    // create a buffer for the instance data
    auto InstanceBuffer = mVRDev->CreateInstanceBuffer(1); // 1 instance

    
	//Specify the instance data
    auto inst = vk::AccelerationStructureInstanceKHR()
        .setInstanceCustomIndex(0)
		.setAccelerationStructureReference(mBLASHandle.BLASBuffer.DevAddress)
		.setFlags(vk::GeometryInstanceFlagBitsKHR::eForceOpaque)
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

    std::vector<vr::BLASBuildInfo> buildInfos = { mBLASBuildInfo }; 
    
    auto BLASscratchBuffer = mVRDev->BuildBLAS(buildInfos, buildCmd); 

    mVRDev->AddAccelerationBuildBarrier(buildCmd); // Add a barrier to the command buffer to make sure the BLAS build is finished before the TLAS build starts

    auto TLASScratchBuffer = mVRDev->BuildTLAS(tlasBuildInfo, InstanceBuffer, 1, buildCmd); 

    buildCmd.end();

    // submit the command buffer and wait for it to finish
    auto submitInfo = vk::SubmitInfo()
        .setCommandBufferCount(1)
        .setPCommandBuffers(&buildCmd);

    mQueues.GraphicsQueue.submit(submitInfo, nullptr);
    
    mDevice.waitIdle();

    // Free the scratch buffers
    mVRDev->DestroyBuffer(BLASscratchBuffer); 
    mVRDev->DestroyBuffer(TLASScratchBuffer);

    mVRDev->DestroyBuffer(InstanceBuffer);

    mDevice.freeCommandBuffers(mGraphicsPool, buildCmd);
}

void DynamicBLAS::UpdateBLAS(vk::CommandBuffer cmd)
{
    // modify the triangle
    float size = sinf(glfwGetTime()) / 2.0f + 0.5f;
    float vertices[] = {
        size, size, 0.0f,
        -size, size, 0.0f,
        0.0f, -size, 0.0f
    };

    // [POI] Additional Info
    // Vulkan requires the whole buffer with same size and the same number of primitives as the source BLAS, so if you want to update only one primitive, 
    // you still have to give vulkan the whole buffer, not parts that you want to update

    //Update the vertex buffer
    mVRDev->UpdateBuffer(mVertexBuffer, vertices, sizeof(float) * 3 * 3);

    // [POI] set the BLAS to update
    vr::BLASUpdateInfo updateInfo = {};

    updateInfo.SourceBLAS = &mBLASHandle;
    updateInfo.SourceBuildInfo = mBLASBuildInfo;
    // [POI] This vector has to be the same size as the vector of geometries in the BLASCreateInfo if using new device addresses / buffers
    // if the vector is empty, then the device addresses used to build the source BLAS will be used
    // this line can be removed, but to demonstrate how to use it, we will set the device addresses to the new ones, although they remain unchanged
    
    updateInfo.UpdatedGeometryAddresses.push_back(vr::GeometryDeviceAddress(mVertexBuffer.DevAddress, mIndexBuffer.DevAddress));



    auto buildInfo = mVRDev->UpdateBLAS(updateInfo);


    auto newScratch = mVRDev->BuildBLAS({ buildInfo }, cmd, &mUpdateScratchBuffer);
    mVRDev->AddAccelerationBuildBarrier(cmd);
    //if they are the same size, then BuildBLAS returned the same buffer, leave as it is
    // but if they are different, then BuildBLAS returned a new buffer, so destroy the old one and set the new one
    if (newScratch.Size != mUpdateScratchBuffer.Size)
    {
        if(mUpdateScratchBuffer.Size > 0)
            mVRDev->DestroyBuffer(mUpdateScratchBuffer);
        mUpdateScratchBuffer = newScratch;
    }
}


void DynamicBLAS::CreateRTPipeline()
{

    mResourceBindings = {
        vr::DescriptorItem(0, vk::DescriptorType::eAccelerationStructureKHR, vk::ShaderStageFlagBits::eRaygenKHR, 1, &mTLASHandle.TLASBuffer.DevAddress),
        vr::DescriptorItem(1, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eRaygenKHR, 1, &mCameraUniformBuffer),
        vr::DescriptorItem(2, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eRaygenKHR,1 , &mOutputImage)
    };


    mResourceDescriptorLayout = mVRDev->CreateDescriptorSetLayout(mResourceBindings); 

    
    // create shaders for the ray tracing pipeline

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
    
    mPipelineLayout = mVRDev->CreatePipelineLayout(mResourceDescriptorLayout);

    mRTPipeline = mVRDev->CreateRayTracingPipeline(mPipelineLayout, mSBT, 1);

    mSBTBuffer = mVRDev->CreateSBT(mRTPipeline, mSBT);

    // create a descriptor set for the ray tracing pipeline
    mResourceDescBuffer = mVRDev->CreateDescriptorBuffer(mResourceDescriptorLayout, mResourceBindings, vr::DescriptorBufferType::Resource);

}


void DynamicBLAS::UpdateDescriptorSet()
{

    mCamera.Pos = glm::vec3(0.0f, 0.0f, 5.0f);

    mVRDev->UpdateDescriptorBuffer(mResourceDescBuffer, mResourceBindings, vr::DescriptorBufferType::Resource);    
}

void DynamicBLAS::Update(vk::CommandBuffer renderCmd)
{
    renderCmd.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

    UpdateBLAS(renderCmd);

    mVRDev->BindDescriptorBuffer({ mResourceDescBuffer }, renderCmd);
    mVRDev->BindDescriptorSet(mPipelineLayout, 0, 0, 0, renderCmd);

    mVRDev->TransitionImageLayout(
        mOutputImageBuffer.Image,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eGeneral,
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1),
        renderCmd);

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


void DynamicBLAS::Stop()
{
    auto _ = mDevice.waitForFences(mRenderFence, VK_TRUE, UINT64_MAX);
    
    mVRDev->DestroyBuffer(mUpdateScratchBuffer);

    // destroy all the resources we created
    mVRDev->DestroySBTBuffer(mSBTBuffer);

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
	Application* app = new DynamicBLAS();

    app->Start();
    app->Run();
    app->Stop();

    delete app;
}