#include "Vulray/Vulray.h"
#include "Common.h"
#include "Application.h"
#include "FileRead.h"
#include "ShaderCompiler.h"


class DynamicTLAS : public Application
{
public:
    virtual void Start() override;
    virtual void Update(vk::CommandBuffer renderCmd) override;
    virtual void Stop() override;

    //functions to break up the start function
    void CreateAS();
    void CreateRTPipeline();
    void UpdateDescriptorSet();

    void UpdateTLAS();
    void UpdateInstances();
public:

    ShaderCompiler mShaderCompiler;

    vr::AllocatedBuffer mVertexBuffer;
    vr::AllocatedBuffer mIndexBuffer;

	vr::SBTBuffer mSBTBuffer;      

    std::vector<vr::DescriptorItem> mResourceBindings;
    vk::DescriptorSetLayout mResourceDescriptorLayout;
    vr::DescriptorBuffer mResourceDescBuffer;

    vk::Pipeline mRTPipeline = nullptr;
    vk::PipelineLayout mPipelineLayout = nullptr;

    vr::BLASHandle mBLASHandle;


    //[POI]
    vr::TLASHandle mTLASHandle;
    vr::TLASBuildInfo mTLASBuildInfo; // save the build info so we can update the TLAS
    std::vector<vk::AccelerationStructureInstanceKHR> mInstanceData; // Keep the instance data in the cpu
    vr::AllocatedBuffer mInstanceBuffer; // for the TLAS
    vr::AllocatedBuffer mScratchBuffer; // for the TLAS so we don't have to create a new one every frame and can reuse it


};

void DynamicTLAS::Start()
{
    CreateBaseResources();
    
    CreateAS();
    
    CreateRTPipeline();
    UpdateDescriptorSet();

}

void DynamicTLAS::CreateAS()
{
    // vertex and index data for the triangle

    float vertices[] = {
        1.0f, -1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f,
        0.0f,  1.0f, 0.0f
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

   auto [blasHandle, buildInfo] = mVRDev->CreateBLAS(blasCreateInfo); 

    auto BLASscratchBuffer = mVRDev->CreateScratchBufferFromBuildInfo(buildInfo); 

    mBLASHandle = blasHandle;
    
    // [POI]
    // create a TLAS now, we will build / update it in the UpdateTLAS(...) function
    // We could build it here, but to simplify the code we will build it in the UpdateTLAS(...) function
    // The TLAS has to be valid before dispatching rays though, but our UpdateTLAS(...) function will be called before the first ray dispatch
    vr::TLASCreateInfo tlasCreateInfo = {};
    tlasCreateInfo.Flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
    tlasCreateInfo.MaxInstanceCount = 5; // Max number of instances in the TLAS, when building the TLAS num of instances may be lower

    std::tie(mTLASHandle, mTLASBuildInfo) = mVRDev->CreateTLAS(tlasCreateInfo);

    // create a buffer for the instance data
    mInstanceData = std::vector<vk::AccelerationStructureInstanceKHR>(5); // 5 instances
    mInstanceBuffer = mVRDev->CreateInstanceBuffer(5); // 5 instances



    auto buildCmd = mVRDev->CreateCommandBuffer(mGraphicsPool); 

    buildCmd.begin(vk::CommandBufferBeginInfo().setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

    std::vector<vr::BLASBuildInfo> buildInfos = { buildInfo }; 
    
    mVRDev->BuildBLAS(buildInfos, buildCmd); 

    buildCmd.end();

    // submit the command buffer and wait for it to finish
    auto submitInfo = vk::SubmitInfo()
        .setCommandBufferCount(1)
        .setPCommandBuffers(&buildCmd);

    mQueues.GraphicsQueue.submit(submitInfo, nullptr);
    
    mDevice.waitIdle();

    // Free the scratch buffers
    mVRDev->DestroyBuffer(BLASscratchBuffer); 

    mDevice.freeCommandBuffers(mGraphicsPool, buildCmd);
}

void DynamicTLAS::UpdateInstances()
{
    float x = 0.0f, z = 0.0f;
    float time = glfwGetTime();

    for(int i = 0; i < mInstanceData.size(); i++)
    {
        // cool animation for the instances
        x = cosf(time + i) + i;
        z = sinf(time + i) + i;

        VkTransformMatrixKHR transform = { // 3x4 matrix
            1.0f, 0.0f, 0.0f, x,
            0.0f, 1.0f, 0.0f, 0.0,
            0.0f, 0.0f, 1.0f, z
        };

        // copy the matrix data

        mInstanceData[i] = vk::AccelerationStructureInstanceKHR()
            .setTransform(transform)
            .setInstanceCustomIndex(0)
            .setMask(0xFF)
            .setInstanceShaderBindingTableRecordOffset(0)
            .setFlags(vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable)
            .setAccelerationStructureReference(mBLASHandle.Buffer.DevAddress);
    }

    mVRDev->UpdateBuffer(mInstanceBuffer, mInstanceData.data(), sizeof(vk::AccelerationStructureInstanceKHR) * mInstanceData.size());

}

void DynamicTLAS::UpdateTLAS()
{
    UpdateInstances();
    auto buildCmd = mVRDev->CreateCommandBuffer(mGraphicsPool);
    buildCmd.begin(vk::CommandBufferBeginInfo().setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

    // [POI]
    // Update the TLAS
    // We have to update the TLAS every frame, because the instance data has changed
    // We give the function the TLAS handle and the TLAS build info, and get new handles and build infos back
    // there is a bool to specify if we want destruction of the old TLAS, we set it to true, because we don't need the old TLAS anymore
    // we would have to keep it if we wanted to update the TLAS while the old one is still in use, and then after the frame finishes, we would have to destroy the old TLAS
    // and update the descriptor set to point to the new TLAS.
    // Keep in mind that the UpdateTLAS(...) creates a new TLAS, so it is not actually an update, but a rebuild, Vulray doesn't offer TLAS updates, but rebuilds
    // This is because the TLAS degrades over time, so it is better to rebuild it every frame and the build time is negligible in real time applications
    // NVIDIA best practices: https://developer.nvidia.com/blog/rtx-best-practices/ 
    std::tie(mTLASHandle, mTLASBuildInfo) = mVRDev->UpdateTLAS(mTLASHandle, mTLASBuildInfo, true);

    // if the new build scratch size is greater than the old one, we have to create a new scratch buffer
    // NOTE: When updating a TLAS Vulray actually recreates it, so we have to use buildScratchSize instead of updateScratchSize
    if(mTLASBuildInfo.BuildSizes.buildScratchSize > mScratchBuffer.Size)
    {
        if(mScratchBuffer.Size > 0) // if not null
            mVRDev->DestroyBuffer(mScratchBuffer);

        mScratchBuffer = mVRDev->CreateScratchBufferFromBuildInfo(mTLASBuildInfo);
    }

    mVRDev->BuildTLAS(mTLASBuildInfo, mInstanceBuffer, mInstanceData.size(), buildCmd);


    mVRDev->AddAccelerationBuildBarrier(buildCmd);

    buildCmd.end();

    // submit the command buffer and wait for it to finish
    // ideally semaphores should be used here, but for simplicity we will just wait for the command buffer to finish
    auto submitInfo = vk::SubmitInfo()
        .setCommandBufferCount(1)
        .setPCommandBuffers(&buildCmd);

    mQueues.GraphicsQueue.submit(submitInfo, nullptr);
    
    mDevice.waitIdle();

    // Update the descriptor, if we had used a semaphores, we would have to wait for the build to finish before updating the descriptor
    // and make sure the descriptor is not used by the GPU at the same time
    mVRDev->UpdateDescriptorBuffer( mResourceDescBuffer,
                                    mResourceBindings[0], // the first binding is the TLAS
                                    0, // index of pResources in the binding
                                    vr::DescriptorBufferType::Resource,
                                    0); // index of the set in the buffer
}


void DynamicTLAS::CreateRTPipeline()
{

    mResourceBindings = {
        vr::DescriptorItem(0, vk::DescriptorType::eAccelerationStructureKHR, vk::ShaderStageFlagBits::eRaygenKHR, 1, &mTLASHandle.Buffer.DevAddress),
        vr::DescriptorItem(1, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eRaygenKHR, 1, &mUniformBuffer),
        vr::DescriptorItem(2, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eRaygenKHR,1 , &mOutputImage)
    };


    mResourceDescriptorLayout = mVRDev->CreateDescriptorSetLayout(mResourceBindings); 

    mPipelineLayout = mVRDev->CreatePipelineLayout(mResourceDescriptorLayout);
    
    // create shaders for the ray tracing pipeline

    vr::ShaderCreateInfo shaderCreateInfo = {};

    shaderCreateInfo.SPIRVCode = mShaderCompiler.CompileSPIRVFromFile("Shaders/ColorfulTriangle/ColorfulTriangle.hlsl");
    auto shaderModule = mVRDev->CreateShaderFromSPV(shaderCreateInfo);

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


void DynamicTLAS::UpdateDescriptorSet()
{

    mCamera.Position = glm::vec3(0.0f, 0.0f, 5.0f);

    mVRDev->UpdateDescriptorBuffer(mResourceDescBuffer, mResourceBindings, vr::DescriptorBufferType::Resource);    
}

void DynamicTLAS::Update(vk::CommandBuffer renderCmd)
{
    renderCmd.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

    UpdateTLAS();

    mVRDev->BindDescriptorBuffer({ mResourceDescBuffer }, renderCmd);
    mVRDev->BindDescriptorSet(mPipelineLayout, 0, 0, 0, renderCmd);

    mVRDev->TransitionImageLayout(
        mOutputImageBuffer.Image,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eGeneral,
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1),
        renderCmd);

    mVRDev->DispatchRays(renderCmd, mRTPipeline, mSBTBuffer, mWindowWidth, mWindowHeight);

    // Helper function in Application Class to blit the image to the swapchain image
    BlitImage(renderCmd);

    renderCmd.end();



    WaitForRendering();

    Present(renderCmd);

    UpdateCamera();
}


void DynamicTLAS::Stop()
{
    auto _ = mDevice.waitForFences(mRenderFence, VK_TRUE, UINT64_MAX);
    
    mVRDev->DestroyBuffer(mScratchBuffer);
    mVRDev->DestroyBuffer(mInstanceBuffer);

    // destroy all the resources we created
    mVRDev->DestroySBTBuffer(mSBTBuffer);

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
	Application* app = new DynamicTLAS();

    app->Start();
    app->Run();
    app->Stop();

    delete app;
}