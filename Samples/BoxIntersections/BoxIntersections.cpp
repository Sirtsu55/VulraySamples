#include "Vulray/Vulray.h"
#include "Common.h"
#include "ShaderCompiler.h"
#include "Application.h"
#include "FileRead.h"


class BoxIntersections : public Application
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

    vr::AllocatedBuffer mAABBBuffer;

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

void BoxIntersections::Start()
{
    CreateBaseResources();
    
    CreateRTPipeline();

    CreateAS();
    
    UpdateDescriptorSet();

}

void BoxIntersections::CreateAS()
{
    // vertex and index data for the triangle

    uint32_t boxSize = sizeof(float) * 2; // min and max for each axis

    float boxes[] = {
        1.0f, 1.0f, 0.0f,
        -1.0f, -1.0f, -1.0f,
    };
    uint32_t indices[] = { 0, 1, 2 };

    mAABBBuffer = mVRDev->CreateBuffer(
        boxSize,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR); // this buffer will be used as a source for the BLAS


    mVRDev->UpdateBuffer(mAABBBuffer, boxes, boxSize);

    vr::BLASCreateInfo blasCreateInfo = {};
    blasCreateInfo.Flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace | vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate;
    
    vr::GeometryData geomData = {};

    geomData.VertexFormat = vk::Format::eR32G32B32Sfloat;
    geomData.PrimitiveCount = 1; // 1 box
    geomData.Stride = boxSize;
    
    geomData.DataAddresses.AABBDevAddress = mAABBBuffer.DevAddress;

    blasCreateInfo.Geometries.push_back(geomData);

   auto [blasHandle, blasBuildInfo] = mVRDev->CreateBLAS(blasCreateInfo); 



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

void BoxIntersections::CreateRTPipeline()
{

    mResourceBindings = {
        vr::DescriptorItem(0, vk::DescriptorType::eAccelerationStructureKHR, vk::ShaderStageFlagBits::eRaygenKHR, 1, &mTLASHandle.TLASBuffer.DevAddress),
        vr::DescriptorItem(1, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eRaygenKHR, 1, &mUniformBuffer),
        vr::DescriptorItem(2, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eRaygenKHR,1 , &mOutputImage)
    };


    mResourceDescriptorLayout = mVRDev->CreateDescriptorSetLayout(mResourceBindings); 

    
    // create shaders for the ray tracing pipeline

    vr::ShaderCreateInfo shaderCreateInfo = {};

    shaderCreateInfo.SPIRVCode = mShaderCompiler.CompileSPIRVFromFile("Shaders/CustomIntersection/RaytracedBoxes.hlsl");
    auto shaderModule = mVRDev->CreateShaderFromSPV(shaderCreateInfo);

    mSBT.RayGenShader = shaderModule;
    mSBT.RayGenShader.EntryPoint = "rgen"; 


    mSBT.MissShaders.push_back(shaderModule);
    mSBT.MissShaders.back().EntryPoint = "miss";

    vr::HitGroup hitGroup = {};
    hitGroup.IntersectionShader = shaderModule;
    hitGroup.IntersectionShader.EntryPoint = "isect";
    
    hitGroup.ClosestHitShader = shaderModule;
    hitGroup.ClosestHitShader.EntryPoint = "chit";
    mSBT.HitGroups.push_back(hitGroup);
    
    mPipelineLayout = mVRDev->CreatePipelineLayout(mResourceDescriptorLayout);

    mRTPipeline = mVRDev->CreateRayTracingPipeline(mPipelineLayout, mSBT, 1);

    mSBTBuffer = mVRDev->CreateSBT(mRTPipeline, mSBT);

    // create a descriptor set for the ray tracing pipeline
    mResourceDescBuffer = mVRDev->CreateDescriptorBuffer(mResourceDescriptorLayout, mResourceBindings, vr::DescriptorBufferType::Resource);

}


void BoxIntersections::UpdateDescriptorSet()
{

    mCamera.Position = glm::vec3(0.0f, 0.0f, 5.0f);

    mVRDev->UpdateDescriptorBuffer(mResourceDescBuffer, mResourceBindings, vr::DescriptorBufferType::Resource);    
}

void BoxIntersections::Update(vk::CommandBuffer renderCmd)
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


void BoxIntersections::Stop()
{
    auto _ = mDevice.waitForFences(mRenderFence, VK_TRUE, UINT64_MAX);
    

    // destroy all the resources we created
    mVRDev->DestroySBTBuffer(mSBTBuffer);

    mVRDev->DestroyShader(mSBT.RayGenShader); 

    mDevice.destroyPipeline(mRTPipeline);
    mDevice.destroyPipelineLayout(mPipelineLayout);

    mDevice.destroyDescriptorSetLayout(mResourceDescriptorLayout);
    mVRDev->DestroyBuffer(mResourceDescBuffer.Buffer);

    mVRDev->DestroyBuffer(mAABBBuffer);

    mVRDev->DestroyBLAS(mBLASHandle);
    mVRDev->DestroyTLAS(mTLASHandle);
}


int main()
{
    // Create the application, start it, run it and stop it, boierplate code, eg initialising vulkan, glfw, etc
    // that is the same for every application is handled by the Application class
    // it can be found in the Base folder
	Application* app = new BoxIntersections();

    app->Start();
    app->Run();
    app->Stop();

    delete app;
}