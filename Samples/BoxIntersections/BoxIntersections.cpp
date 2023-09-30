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

    // functions to break up the start function
    void CreateAS();
    void CreateRTPipeline();
    void UpdateDescriptorSet();

public:
    ShaderCompiler mShaderCompiler;

    vr::AllocatedBuffer mAABBBuffer;

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

    uint32_t boxSize = sizeof(vk::AabbPositionsKHR); // 2 vec3s for the min and max of the AABB

    // [POI]
    // AABB for a box
    auto box = vk::AabbPositionsKHR(-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f);

    mAABBBuffer = mVRDev->CreateBuffer(
        boxSize,
        vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT); // this buffer will be used as a source for the BLAS

    // copy the AABB to the buffer
    mVRDev->UpdateBuffer(mAABBBuffer, &box, boxSize);

    vr::BLASCreateInfo blasCreateInfo = {};
    blasCreateInfo.Flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;

    // [POI]
    vr::GeometryData geomData = {};
    // Set the geometry type to AABBs, default is Triangles
    geomData.Type = vk::GeometryTypeKHR::eAabbs;
    geomData.PrimitiveCount = 1;         // 1 box
    geomData.Stride = sizeof(float) * 6; // vec3 min, vec3 max
    // All other data is not used for AABBs, like index and vertex buffers, and index/vertex format

    // Set the AABB buffer as the data source
    geomData.DataAddresses.AABBDevAddress = mAABBBuffer.DevAddress;

    blasCreateInfo.Geometries.push_back(geomData);

    auto [blasHandle, blasBuildInfo] = mVRDev->CreateBLAS(blasCreateInfo);

    mBLASHandle = blasHandle;

    auto BLASscratchBuffer = mVRDev->CreateScratchBufferFromBuildInfo(blasBuildInfo);

    // create a TLAS
    vr::TLASCreateInfo tlasCreateInfo = {};
    tlasCreateInfo.Flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
    tlasCreateInfo.MaxInstanceCount = 1; // Max number of instances in the TLAS, when building the TLAS num of instances may be lower

    auto [tlasHandle, tlasBuildInfo] = mVRDev->CreateTLAS(tlasCreateInfo);

    mTLASHandle = tlasHandle;

    auto TLASScratchBuffer = mVRDev->CreateScratchBufferFromBuildInfo(tlasBuildInfo);

    // create a buffer for the instance data
    auto InstanceBuffer = mVRDev->CreateInstanceBuffer(1); // 1 instance

    // Specify the instance data
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
        0.0f, 0.0f, 1.0f, 0.0f};

    mVRDev->UpdateBuffer(InstanceBuffer, &inst, sizeof(vk::AccelerationStructureInstanceKHR), 0);

    auto buildCmd = mDevice.allocateCommandBuffers(vk::CommandBufferAllocateInfo(mGraphicsPool, vk::CommandBufferLevel::ePrimary, 1))[0];

    buildCmd.begin(vk::CommandBufferBeginInfo().setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

    std::vector<vr::BLASBuildInfo> buildInfos = {blasBuildInfo};

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
        vr::DescriptorItem(0, vk::DescriptorType::eAccelerationStructureKHR, vk::ShaderStageFlagBits::eRaygenKHR, 1, &mTLASHandle.Buffer.DevAddress),
        vr::DescriptorItem(1, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eRaygenKHR, 1, &mUniformBuffer),
        vr::DescriptorItem(2, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eRaygenKHR, 1, &mOutputImage)};

    mResourceDescriptorLayout = mVRDev->CreateDescriptorSetLayout(mResourceBindings);

    mPipelineLayout = mVRDev->CreatePipelineLayout(mResourceDescriptorLayout);

    // create shaders for the ray tracing pipeline

    // [POI]
    // Have a look at the shader file to see how the intersection shader is set up
    auto spv = mShaderCompiler.CompileSPIRVFromFile("Shaders/CustomIntersection/RaytracedBoxes.hlsl");
    auto shaderModule = mVRDev->CreateShaderFromSPV(spv);

    vr::PipelineSettings pipelineSettings = {};
    pipelineSettings.PipelineLayout = mPipelineLayout;
    pipelineSettings.MaxRecursionDepth = 1;
    pipelineSettings.MaxPayloadSize = sizeof(glm::vec3);
    // [POI]
    // The size of the data that is passed to the hit shader, from the intersection shader
    // In the shadercode BoxHitAttributes is a struct with float2 = 8 bytes and glm::vec2 = 8 bytes
    // So this has to match the size of the struct
    pipelineSettings.MaxHitAttributeSize = sizeof(glm::vec3);

    vr::RayTracingShaderCollection shaderCollection = {};
    shaderCollection.RayGenShaders.push_back(shaderModule);
    shaderCollection.RayGenShaders.back().EntryPoint = "rgen";

    shaderCollection.MissShaders.push_back(shaderModule);
    shaderCollection.MissShaders.back().EntryPoint = "miss";

    vr::HitGroup hitGroup = {};
    hitGroup.ClosestHitShader = shaderModule;
    hitGroup.ClosestHitShader.EntryPoint = "chit";
    // [POI]
    // Set the intersection shader to the same hitgroup as the closest hit shader
    // The intersection shader will call the closest hit shader in the same hitgroup if it reports a hit
    hitGroup.IntersectionShader = shaderModule;
    hitGroup.IntersectionShader.EntryPoint = "isect";
    shaderCollection.HitGroups.push_back(hitGroup);

    auto [pipeline, sbtInfo] = mVRDev->CreateRayTracingPipeline(shaderCollection, pipelineSettings);
    mRTPipeline = pipeline;

    mSBTBuffer = mVRDev->CreateSBT(mRTPipeline, sbtInfo);

    // create a descriptor buffer for the ray tracing pipeline
    mResourceDescBuffer = mVRDev->CreateDescriptorBuffer(mResourceDescriptorLayout, mResourceBindings, vr::DescriptorBufferType::Resource);

    mDevice.destroyShaderModule(shaderModule.Module);
}

void BoxIntersections::UpdateDescriptorSet()
{

    mCamera.Position = glm::vec3(0.0f, 0.0f, 5.0f);

    mVRDev->UpdateDescriptorBuffer(mResourceDescBuffer, mResourceBindings, vr::DescriptorBufferType::Resource);
}

void BoxIntersections::Update(vk::CommandBuffer renderCmd)
{
    renderCmd.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

    mVRDev->BindDescriptorBuffer({mResourceDescBuffer}, renderCmd);
    mVRDev->BindDescriptorSet(mPipelineLayout, 0, 0, 0, renderCmd);

    mVRDev->TransitionImageLayout(
        mOutputImageBuffer.Image,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eGeneral,
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1),
        renderCmd);

    mVRDev->DispatchRays(mRTPipeline, mSBTBuffer, mWindowWidth, mWindowHeight, 1, renderCmd);

    // Helper function in Application Class to blit the image to the swapchain image
    BlitImage(renderCmd);

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
    Application *app = new BoxIntersections();

    app->Start();
    app->Run();
    app->Stop();

    delete app;
}