#include "Vulray/Vulray.h"
#include "Common.h"
#include "Application.h"
#include "FileRead.h"
#include "ShaderCompiler.h"


class SBTData : public Application
{
public:
    virtual void Start() override;
    virtual void Update(vk::CommandBuffer renderCmd) override;
    virtual void Stop() override;

    //functions to break up the start function
    void CreateAS();
    void CreateRTPipeline();
    void UpdateDescriptorSet();

    void UpdateSBT();

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

    vr::TLASHandle mTLASHandle;


};

void SBTData::Start()
{
    CreateBaseResources();
    CreateAS();
    CreateRTPipeline();
    UpdateSBT();
    UpdateDescriptorSet();

}

void SBTData::CreateAS()
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

   auto [blasHandle, blasBuildInfo] = mVRDev->CreateBLAS(blasCreateInfo); 
    mBLASHandle = blasHandle;

    auto BLASscratchBuffer = mVRDev->CreateScratchBufferBLAS(blasBuildInfo);

    // create a TLAS
    vr::TLASCreateInfo tlasCreateInfo = {};
    tlasCreateInfo.Flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
    tlasCreateInfo.MaxInstanceCount = 1; // Max number of instances in the TLAS, when building the TLAS num of instances may be lower

    auto [tlasHandle, tlasBuildInfo] = mVRDev->CreateTLAS(tlasCreateInfo);

    mTLASHandle = tlasHandle;

    auto TLASScratchBuffer = mVRDev->CreateScratchBufferTLAS(tlasBuildInfo);

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

    std::vector<vr::BLASBuildInfo> buildInfos = { blasBuildInfo }; 
    
    mVRDev->BuildBLAS(buildInfos, buildCmd); 

    mVRDev->AddAccelerationBuildBarrier(buildCmd); // Add a barrier to the command buffer to make sure the BLAS build is finished before the TLAS build starts

    mVRDev->BuildTLAS(tlasBuildInfo, InstanceBuffer, 1, buildCmd); 

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

void SBTData::CreateRTPipeline()
{

    mResourceBindings = {
        vr::DescriptorItem(0, vk::DescriptorType::eAccelerationStructureKHR, vk::ShaderStageFlagBits::eRaygenKHR, 1, &mTLASHandle.TLASBuffer.DevAddress),
        vr::DescriptorItem(1, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eRaygenKHR, 1, &mUniformBuffer),
        vr::DescriptorItem(2, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eRaygenKHR,1 , &mOutputImage)
    };


    mResourceDescriptorLayout = mVRDev->CreateDescriptorSetLayout(mResourceBindings); 

    
    // create shaders for the ray tracing pipeline

    vr::ShaderCreateInfo shaderCreateInfo = {};

    shaderCreateInfo.SPIRVCode = mShaderCompiler.CompileSPIRVFromFile("Shaders/SBTShader/SBTShader.hlsl");
    auto shaderModule = mVRDev->CreateShaderFromSPV(shaderCreateInfo);

    // [POI]
    // Set the shader record sizes for our own data in the SBT

    mSBT.RayGenShader = shaderModule;
    mSBT.RayGenShader.EntryPoint = "rgen"; 

    mSBT.MissShaders.push_back(shaderModule);
    mSBT.MissShaders.back().EntryPoint = "miss";
    // Even if the size isn't aligned properly now, it will be when creating the SBT
    mSBT.MissShaderRecordSize = sizeof(glm::vec3); // background color

    vr::HitGroup hitGroup = {};
    hitGroup.ClosestHitShader = shaderModule;
    hitGroup.ClosestHitShader.EntryPoint = "chit";
    mSBT.HitGroups.push_back(hitGroup);
    mSBT.HitShaderRecordSize = sizeof(glm::vec3); // color of the triangle

    mPipelineLayout = mVRDev->CreatePipelineLayout(mResourceDescriptorLayout);

    mRTPipeline = mVRDev->CreateRayTracingPipeline(mPipelineLayout, mSBT, 1);

    mSBTBuffer = mVRDev->CreateSBT(mRTPipeline, mSBT);

    // create a descriptor set for the ray tracing pipeline
    mResourceDescBuffer = mVRDev->CreateDescriptorBuffer(mResourceDescriptorLayout, mResourceBindings, vr::DescriptorBufferType::Resource);

}

void SBTData::UpdateDescriptorSet()
{
    mCamera.Position = glm::vec3(0.0f, 0.0f, 5.0f);

    mVRDev->UpdateDescriptorBuffer(mResourceDescBuffer, mResourceBindings, vr::DescriptorBufferType::Resource);    
}

void SBTData::UpdateSBT()
{
    // [POI]
    // Change the background color and triangle color
    // Try changing the values and see what happens
    
    glm::vec3 backgroundColor = glm::vec3(0.0f, 0.0f, 1.0f);
    mVRDev->WriteToSBT(mSBTBuffer, vr::ShaderGroup::Miss, 0, &backgroundColor, sizeof(glm::vec3)); // write the background color to the miss shader
    glm::vec3 triangleColor = glm::vec3(1.0f, 0.0f, 0.0f);
    mVRDev->WriteToSBT(mSBTBuffer, vr::ShaderGroup::HitGroup, 0, &triangleColor, sizeof(glm::vec3)); // write the triangle color to the hit shader

}

void SBTData::Update(vk::CommandBuffer renderCmd)
{
    renderCmd.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

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


void SBTData::Stop()
{
    auto _ = mDevice.waitForFences(mRenderFence, VK_TRUE, UINT64_MAX);
    
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
	Application* app = new SBTData();

    app->Start();
    app->Run();
    app->Stop();

    delete app;
}