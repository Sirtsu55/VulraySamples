#include "Vulray/Vulray.h"
#include "Common.h"
#include "Application.h"
#include "FileRead.h"
#include "ShaderCompiler.h"


class Callable : public Application
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
    ShaderCompiler mShaderCompiler;
    
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

void Callable::Start()
{
    //defined in the base application class, creates an output image to render to and a camera uniform buffer
    CreateBaseResources();
    
    CreateAS();
    
    CreateRTPipeline();
    UpdateDescriptorSet();

}

void Callable::CreateAS()
{
// vertex and index data for the triangle

    float vertices[] = {
        1.0f, -1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f,
        0.0f,  1.0f, 0.0f
    };
    uint32_t indices[] = { 0, 1, 2 };

    auto vertBuffer = mVRDev->CreateBuffer(
        sizeof(float) * 3 * 3, 
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, 
        vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR); 
    auto indexBuffer = mVRDev->CreateBuffer(
        sizeof(uint32_t) * 3, 
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, 
        vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR);
    auto transformBuffer = mVRDev->CreateBuffer(
        sizeof(uint32_t) * 3, 
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, 
        vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR);
    
    vk::TransformMatrixKHR transforms[2];
    transforms[0] = {
        1.0f, 0.0f, 0.0f, -1.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f
    };
    transforms[1] = {
        1.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f
    };

    mVRDev->UpdateBuffer(vertBuffer, vertices, sizeof(float) * 3 * 3);
    mVRDev->UpdateBuffer(indexBuffer, indices, sizeof(uint32_t) * 3); 
    mVRDev->UpdateBuffer(transformBuffer, transforms, sizeof(vk::TransformMatrixKHR) * 2);
    
    vr::BLASCreateInfo blasCreateInfo = {};
    blasCreateInfo.Flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
    
    for (uint32_t i = 0; i < 2; i++)
    {
        /* code */
        vr::GeometryData geomData = {};
        geomData.VertexFormat = vk::Format::eR32G32B32Sfloat;
        geomData.Stride = sizeof(float) * 3; // 3 floats per vertex: x, y, z
        geomData.IndexFormat = vk::IndexType::eUint32;
        geomData.PrimitiveCount = 1;
        geomData.DataAddresses.VertexDevAddress = vertBuffer.DevAddress;
        geomData.DataAddresses.IndexDevAddress = indexBuffer.DevAddress;
        geomData.DataAddresses.TransformDevAddress = transformBuffer.DevAddress + sizeof(vk::TransformMatrixKHR) * i;
        blasCreateInfo.Geometries.push_back(geomData);
    }
    


    // [POI]
    // this only creates the BLAS, it does not build it
	// it creates acceleration structure and allocates memory for it and scratch memory
    auto[blasHandle, blasBuildInfo] = mVRDev->CreateBLAS(blasCreateInfo); 

    // Create a scratch buffer for the BLAS build
    auto BLASscratchBuffer = mVRDev->CreateScratchBufferBLAS(blasBuildInfo); 
    // To have avoid allocating scratch memory, every build you can create a big scratch buffer and reuse it for all BLAS builds
    // You can create a big buffer with minimum scratch alignment properties from VulrayDevice::GetAccelerationStructureProperties()
    // and divide it into smaller buffers for each BLAS build according to how much scratch memory each BLAS needs
    // Set the scratch buffer address for a BLAS by setting blasBuildInfo.BuildGeometryInfo.scratchData 
    // or just call VulrayDevice::BindScratchBufferToBuildInfo() to do the same thing

    mBLASHandle = blasHandle;

    // [POI]
    // create a TLAS
    vr::TLASCreateInfo tlasCreateInfo = {};
    tlasCreateInfo.Flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
    tlasCreateInfo.MaxInstanceCount = 1; // Max number of instances in the TLAS, when building the TLAS num of instances may be lower

    auto [tlasHandle, tlasBuildInfo] = mVRDev->CreateTLAS(tlasCreateInfo);

    mTLASHandle = tlasHandle;

    // Create the scratch buffer for TLAS build
    auto TLASScratchBuffer = mVRDev->CreateScratchBufferTLAS(tlasBuildInfo);
    
    // create a buffer for the instance data
    auto InstanceBuffer = mVRDev->CreateInstanceBuffer(1); // 1 instance

    
	//Specify the instance data
    auto inst = vk::AccelerationStructureInstanceKHR()
        .setInstanceCustomIndex(0)
		.setAccelerationStructureReference(mBLASHandle.Buffer.DevAddress)
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

    std::vector<vr::BLASBuildInfo> buildInfos = { blasBuildInfo }; 

    mVRDev->BuildBLAS(buildInfos, buildCmd);

    mVRDev->AddAccelerationBuildBarrier(buildCmd); 


    mVRDev->BuildTLAS(tlasBuildInfo, InstanceBuffer, 1, buildCmd); 

    buildCmd.end();

    auto submitInfo = vk::SubmitInfo()
        .setCommandBufferCount(1)
        .setPCommandBuffers(&buildCmd);

    mQueues.GraphicsQueue.submit(submitInfo, nullptr);
    
    mDevice.waitIdle();

    mVRDev->DestroyBuffer(vertBuffer);
    mVRDev->DestroyBuffer(indexBuffer);
    mVRDev->DestroyBuffer(transformBuffer);

    mVRDev->DestroyBuffer(BLASscratchBuffer); 
    mVRDev->DestroyBuffer(TLASScratchBuffer);

    mVRDev->DestroyBuffer(InstanceBuffer);

    mDevice.freeCommandBuffers(mGraphicsPool, buildCmd);
}


void Callable::CreateRTPipeline()
{
    mResourceBindings = {
        vr::DescriptorItem(0, vk::DescriptorType::eAccelerationStructureKHR, vk::ShaderStageFlagBits::eRaygenKHR, 1, &mTLASHandle.Buffer.DevAddress),
        vr::DescriptorItem(1, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eRaygenKHR, 1, &mUniformBuffer),
        vr::DescriptorItem(2, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eRaygenKHR, 1, &mOutputImage),
    };


    mResourceDescriptorLayout = mVRDev->CreateDescriptorSetLayout(mResourceBindings); 
    
    vr::ShaderCreateInfo shaderCreateInfo = {};

    shaderCreateInfo.SPIRVCode = mShaderCompiler.CompileSPIRVFromFile("Shaders/Callable/Callable.hlsl");
    auto shaderModule = mVRDev->CreateShaderFromSPV(shaderCreateInfo);

    mSBT.RayGenShader = shaderModule;
    mSBT.RayGenShader.EntryPoint = "rgen"; 

    mSBT.MissShaders.push_back(shaderModule);
    mSBT.MissShaders.back().EntryPoint = "miss";

    // [POI]
    // Add callable shaders to the shader binding table
    // look at the shader code to see how callable shaders are called
    // The SBT will be structured as follows:
    //              |Shader IDX: 0   | ShaderIDX: 1  |
    // | rgen | ... | call0         | call1         | ... |
    // ShaderIDX specifies what index to pass to CallShader() in the shader, to call the corresponding callable shader

    mSBT.CallableShaders.push_back(shaderModule);
    mSBT.CallableShaders.back().EntryPoint = "call0"; // at index 0 in the shader binding table is the call0 shader

    mSBT.CallableShaders.push_back(shaderModule);
    mSBT.CallableShaders.back().EntryPoint = "call1"; // at index 1 in the shader binding table is the call1 shader

    

    vr::HitGroup hitGroup = {};
    hitGroup.ClosestHitShader = shaderModule;
    hitGroup.ClosestHitShader.EntryPoint = "chit";
    mSBT.HitGroups.push_back(hitGroup);
    

    // Create a layout for the pipeline
    mPipelineLayout = mVRDev->CreatePipelineLayout(mResourceDescriptorLayout);

    // create the ray tracing pipeline, a vk::Pipeline object
    mRTPipeline = mVRDev->CreateRayTracingPipeline(mPipelineLayout, mSBT, 1);

    mSBTBuffer = mVRDev->CreateSBT(mRTPipeline, mSBT);

    // create a descriptor buffer for the ray tracing pipeline
    mResourceDescBuffer = mVRDev->CreateDescriptorBuffer(mResourceDescriptorLayout, mResourceBindings, vr::DescriptorBufferType::Resource);
}


void Callable::UpdateDescriptorSet()
{
    mCamera.Position = glm::vec3(0.0f, 0.0f, 2.5f);

    mVRDev->UpdateDescriptorBuffer(mResourceDescBuffer, mResourceBindings, vr::DescriptorBufferType::Resource);
}

void Callable::Update(vk::CommandBuffer renderCmd)
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


void Callable::Stop()
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

    mVRDev->DestroyBLAS(mBLASHandle);
    mVRDev->DestroyTLAS(mTLASHandle);
}


int main()
{
    // Create the application, start it, run it and stop it, boierplate code, eg initialising vulkan, glfw, etc
    // that is the same for every application is handled by the Application class
    // it can be found in the Base folder
	Application* app = new Callable();

    app->Start();
    app->Run();
    app->Stop();

    delete app;
}