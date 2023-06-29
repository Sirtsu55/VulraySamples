#include "Vulray/Vulray.h"
#include "Common.h"
#include "Application.h"
#include "FileRead.h"
#include "ShaderCompiler.h"


class Compaction : public Application
{
public:
    virtual void Start() override;
    virtual void Update(vk::CommandBuffer renderCmd) override;
    virtual void Stop() override;

    //functions to break up the start function
    void CreateAS();
    void CreateRTPipeline();
    void UpdateDescriptorSet();


    void Compact(vk::CommandBuffer cmd);

    // Update the TLAS when the BLAS changes due to compaction
    void UpdateTLAS();
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


    vr::TLASHandle mTLASHandle;
    vr::TLASBuildInfo mTLASBuildInfo; 
    vr::AllocatedBuffer mInstanceBuffer; 
    vr::AllocatedBuffer mScratchBuffer;

    //[POI] - Compaction
    std::vector<vr::BLASHandle*> mBLASToCompact; // all the BLASes that need to be compacted
    std::vector<vr::BLASHandle> mBLASToDestroy; // all the BLASes that need to be destroyed after compaction
    vr::CompactionRequest mCompactionRequest;
    bool Compacted = false;
};

void Compaction::Start()
{
    CreateBaseResources();
    
    CreateAS();
    
    CreateRTPipeline();
    UpdateDescriptorSet();

}

void Compaction::CreateAS()
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
    blasCreateInfo.Flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace | vk::BuildAccelerationStructureFlagBitsKHR::eAllowCompaction;
    
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
    
    vr::TLASCreateInfo tlasCreateInfo = {};
    tlasCreateInfo.Flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
    tlasCreateInfo.MaxInstanceCount = 5; 

    std::tie(mTLASHandle, mTLASBuildInfo) = mVRDev->CreateTLAS(tlasCreateInfo);

    mScratchBuffer = mVRDev->CreateScratchBufferFromBuildInfo(mTLASBuildInfo);

    mInstanceBuffer = mVRDev->CreateInstanceBuffer(1);

	//Specify the instance data
    auto inst = vk::AccelerationStructureInstanceKHR()
        .setInstanceCustomIndex(0)
		.setAccelerationStructureReference(mBLASHandle.Buffer.DevAddress)
		.setFlags(vk::GeometryInstanceFlagBitsKHR::eForceOpaque)
        .setMask(0xFF)
        .setInstanceShaderBindingTableRecordOffset(0);

    // set the transform matrix to identity
    inst.transform = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f
    };

    mVRDev->UpdateBuffer(mInstanceBuffer, &inst, sizeof(vk::AccelerationStructureInstanceKHR), 0);

    auto buildCmd = mVRDev->CreateCommandBuffer(mGraphicsPool); 

    buildCmd.begin(vk::CommandBufferBeginInfo().setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

    std::vector<vr::BLASBuildInfo> buildInfos = { buildInfo }; 
    
    mVRDev->BuildBLAS(buildInfos, buildCmd); 

    mVRDev->AddAccelerationBuildBarrier(buildCmd);

    mVRDev->BuildTLAS(mTLASBuildInfo, mInstanceBuffer, 1, buildCmd); 

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

    // [POI] get a request for compaction

    mBLASToCompact.push_back(&mBLASHandle); // add the BLAS to the list of BLASes to compact
    mCompactionRequest = mVRDev->RequestCompaction(mBLASToCompact); // get the compaction request
}
// A function that updates the TLAS
// Nearly same as DynamicTLAS sample, but with no comments
void Compaction::UpdateTLAS()
{
    // [POI] Put the new BLAS into the TLAS
    auto inst = vk::AccelerationStructureInstanceKHR()
        .setInstanceCustomIndex(0)          
		.setAccelerationStructureReference(mBLASHandle.Buffer.DevAddress) // This remains the same, because we replaced the BLAS with a compacted version
		.setFlags(vk::GeometryInstanceFlagBitsKHR::eForceOpaque)
        .setMask(0xFF)
        .setInstanceShaderBindingTableRecordOffset(0);
    inst.transform = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f
    };
    mVRDev->UpdateBuffer(mInstanceBuffer, &inst, sizeof(vk::AccelerationStructureInstanceKHR), 0);
    auto buildCmd = mVRDev->CreateCommandBuffer(mGraphicsPool);
    buildCmd.begin(vk::CommandBufferBeginInfo().setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
    std::tie(mTLASHandle, mTLASBuildInfo) = mVRDev->UpdateTLAS(mTLASHandle, mTLASBuildInfo, true);
    if(mTLASBuildInfo.BuildSizes.buildScratchSize > mScratchBuffer.Size)
    {
        if(mScratchBuffer.Size > 0) // if not null
            mVRDev->DestroyBuffer(mScratchBuffer);
        mScratchBuffer = mVRDev->CreateScratchBufferFromBuildInfo(mTLASBuildInfo);
    }
    mVRDev->BuildTLAS(mTLASBuildInfo, mInstanceBuffer, 1, buildCmd);
    mVRDev->AddAccelerationBuildBarrier(buildCmd);
    buildCmd.end();
    auto submitInfo = vk::SubmitInfo()
        .setCommandBufferCount(1)
        .setPCommandBuffers(&buildCmd);
    mQueues.GraphicsQueue.submit(submitInfo, nullptr);
    mDevice.waitIdle();
    mVRDev->UpdateDescriptorBuffer( mResourceDescBuffer,
                                    mResourceBindings[0], // the first binding is the TLAS
                                    0, // index of pResources in the binding
                                    vr::DescriptorBufferType::Resource);
}

void Compaction::Compact(vk::CommandBuffer cmdBuf)
{
    if(!Compacted)
    {
        std::vector<uint64_t> compactedSizes;
        compactedSizes = mVRDev->GetCompactionSizes(mCompactionRequest, cmdBuf);
        // [POI] compact the BLASes
        // The BLAS compaction sizes may take a few frames to become available, so we need to check if the sizes are valid
        if(compactedSizes.size() > 0)
        {
            // [POI]
            // These function record a copy command of acceleration structures to the compacted BLASes
            // Therefore we need to ensure the copy command is executed before we destroy the original BLASes and before using the compacted BLASes
            // Two options here:
            mBLASToDestroy = mVRDev->CompactBLAS(mCompactionRequest, compactedSizes, mBLASToCompact, cmdBuf);
            // or
            // auto compactedBLASes = mVRDev->CompactBLAS(mCompactionRequest, compactedSizes, cmdBuf);
            // The first option will replace the BLASes in mBLASToCompact with the compacted BLASes and return a vector of the BLAS to destroy
            // The second option will return a vector of the compacted BLASes and the user will have to destroy the original BLASes
            // and replace any references to the original BLASes with the compacted BLASes before destroying the original BLASes
            // We will use the first option here
            Compacted = true;
        }

    }
}

void Compaction::CreateRTPipeline()
{

    mResourceBindings = {
        vr::DescriptorItem(0, vk::DescriptorType::eAccelerationStructureKHR, vk::ShaderStageFlagBits::eRaygenKHR, 1, &mTLASHandle.Buffer.DevAddress),
        vr::DescriptorItem(1, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eRaygenKHR, 1, &mUniformBuffer),
        vr::DescriptorItem(2, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eRaygenKHR,1 , &mOutputImage)
    };


    mResourceDescriptorLayout = mVRDev->CreateDescriptorSetLayout(mResourceBindings); 

    mPipelineLayout = mVRDev->CreatePipelineLayout(mResourceDescriptorLayout);
    
    // create shaders for the ray tracing pipeline
    auto spv = mShaderCompiler.CompileSPIRVFromFile("Shaders/Shading/Shading.hlsl");
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


void Compaction::UpdateDescriptorSet()
{

    mCamera.Position = glm::vec3(0.0f, 0.0f, 5.0f);

    mVRDev->UpdateDescriptorBuffer(mResourceDescBuffer, mResourceBindings, vr::DescriptorBufferType::Resource);    
}

void Compaction::Update(vk::CommandBuffer renderCmd)
{
    renderCmd.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

    Compact(renderCmd);

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

    if(mBLASToDestroy.size() > 0)
    {
        // [POI]
        // Update the TLAS now that the copying of the compacted BLAS to our used BLAS is done, because the command buffer has finished executing
        UpdateTLAS();
        mVRDev->DestroyBLAS(mBLASToDestroy);
        mBLASToDestroy.clear();
    }

    UpdateCamera();
}


void Compaction::Stop()
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
	Application* app = new Compaction();

    app->Start();
    app->Run();
    app->Stop();

    delete app;
}