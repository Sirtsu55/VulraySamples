#include "Vulray/Vulray.h"
#include "Common.h"
#include "Application.h"
#include "FileRead.h"
#include "ShaderCompiler.h"
#include <filesystem>


class HelloTriangle : public Application
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

void HelloTriangle::Start()
{
    //defined in the base application class, creates an output image to render to and a camera uniform buffer
    CreateBaseResources();
    
    CreateAS();
    
    CreateRTPipeline();
    UpdateDescriptorSet();

}

void HelloTriangle::CreateAS()
{
    // vertex and index data for the triangle

    float vertices[] = {
        1.0f, -1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f,
        0.0f,  1.0f, 0.0f
    };
    uint32_t indices[] = { 0, 1, 2 };

    // create a buffer for the vertices and copy the data to it
    mVertexBuffer = mVRDev->CreateBuffer(
        sizeof(float) * 3 * 3, // 3 vertices, 3 floats per vertex
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, // we will be writing to this buffer on the CPU, so we need to set this flag, the buffer is also host visible so it is not fast GPU memory
        vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR); // this buffer will be used as a source for the BLAS
    mIndexBuffer = mVRDev->CreateBuffer(
        sizeof(uint32_t) * 3, // 3 vertices, 3 floats per vertex
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, // same as above
        vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR);


    // upload the vertex data to the buffer, UpdateBuffer(...) will use mapping the buffer and memcpy 
    mVRDev->UpdateBuffer(mVertexBuffer, vertices, sizeof(float) * 3 * 3);
    mVRDev->UpdateBuffer(mIndexBuffer, indices, sizeof(uint32_t) * 3); 
    
	// Create info struct for the BLAS
    vr::BLASCreateInfo blasCreateInfo = {};
    blasCreateInfo.Flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
    
    // [POI]
	// triangle geometry data, BLASCreateInfo can have multiple geometries
    vr::GeometryData geomData = {};

    geomData.VertexFormat = vk::Format::eR32G32B32Sfloat;
    geomData.Stride = sizeof(float) * 3; // 3 floats per vertex: x, y, z
    geomData.IndexFormat = vk::IndexType::eUint32;
    geomData.PrimitiveCount = 1;
    geomData.DataAddresses.VertexDevAddress = mVertexBuffer.DevAddress;
    geomData.DataAddresses.IndexDevAddress = mIndexBuffer.DevAddress;
	
    // add triangle geometry to the BLAS create info
    // NOTE: BLASCreateInfo can have multiple geometries and they are of type VkAccelerationStructureGeometryKHR
    blasCreateInfo.Geometries.push_back(geomData);


    // [POI]
    // this only creates the BLAS, it does not build it
	// it creates acceleration structure and allocates memory for it and scratch memory
    auto[blasHandle, blasBuildInfo] = mVRDev->CreateBLAS(blasCreateInfo); 

    // Create a scratch buffer for the BLAS build
    auto BLASscratchBuffer = mVRDev->CreateScratchBufferBLAS(blasBuildInfo); 

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

    // [POI]
    // upload the instance data to the buffer
    mVRDev->UpdateBuffer(InstanceBuffer, &inst, sizeof(vk::AccelerationStructureInstanceKHR), 0);



    // create a command buffer to build the BLAS and TLAS, mGraphicsPool is a command pool that is created in the Base Application class
    auto buildCmd = mVRDev->CreateCommandBuffer(mGraphicsPool); 

    buildCmd.begin(vk::CommandBufferBeginInfo().setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

    // [POI]

    // build the AS
    std::vector<vr::BLASBuildInfo> buildInfos = { blasBuildInfo }; // We can have multiple BLAS builds at once, but we only have one for now

    mVRDev->BuildBLAS(buildInfos, buildCmd); // Add build commands to command buffer and retrieve scratch buffer for the build 

    mVRDev->AddAccelerationBuildBarrier(buildCmd); // Add a barrier to the command buffer to make sure the BLAS build is finished before the TLAS build starts

    // Add build commands to command buffer and retrieve scratch buffer for the build
    // We can reuse the scratch buffer from here to update the TLAS, but for now we don't update

    mVRDev->BuildTLAS(tlasBuildInfo, InstanceBuffer, 1, buildCmd); 

    buildCmd.end();

    // submit the command buffer and wait for it to finish
    auto submitInfo = vk::SubmitInfo()
        .setCommandBufferCount(1)
        .setPCommandBuffers(&buildCmd);

    mQueues.GraphicsQueue.submit(submitInfo, nullptr);
    
    mDevice.waitIdle();

    // Destroy the scratch buffers, because the build is finished
    // NOTE: We know the build is finished because we waited for the device to be idle, but in a real application we would use a fence or something else
    mVRDev->DestroyBuffer(BLASscratchBuffer); 
    mVRDev->DestroyBuffer(TLASScratchBuffer);

    // We don't need the instance buffer anymore, because the TLAS is built and we don't plan on updating it
    mVRDev->DestroyBuffer(InstanceBuffer);

    // free the command buffer
    mDevice.freeCommandBuffers(mGraphicsPool, buildCmd);
}


void HelloTriangle::CreateRTPipeline()
{



    // [POI]
    // Now we create a descriptor layout for the ray tracing pipeline
    // last parameter is a pointer to the items vector, so we can use it later to create the descriptor set
    // for now we have only one item, so we just pass the address of the first element
    // if we want to update the descriptor set later with another item,
    // we can just reassign the vr::DescriptorItem::pItems with new items and update the descriptor set
    mResourceBindings = {
        vr::DescriptorItem(0, vk::DescriptorType::eAccelerationStructureKHR, vk::ShaderStageFlagBits::eRaygenKHR, 1, &mTLASHandle.TLASBuffer.DevAddress),
        vr::DescriptorItem(1, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eRaygenKHR, 1, &mUniformBuffer),
        vr::DescriptorItem(2, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eRaygenKHR, 10, &mOutputImage, 1)
    };


    // create a descriptor set layout, for the ray tracing pipeline
    mResourceDescriptorLayout = mVRDev->CreateDescriptorSetLayout(mResourceBindings); 
    
    // create shaders for the ray tracing pipeline
    // Spir-V bytecode is required in the info struct


    vr::ShaderCreateInfo shaderCreateInfo = {};

    // Shader compiler class from from Base/ will perform HLSL -> SPIR-V translation.
    // We put eRaygenKHR as stage, but it has to be one of the ray tracing stages, because DXC compiler will use 
    // lib_6_x to compile any of the ray tracing stages eg. eRaygenKHR, eMissKHR, eClosestHitKHR, eAnyHitKHR, eIntersectionKHR -> lib_6_x
    shaderCreateInfo.SPIRVCode = mShaderCompiler.CompileSPIRVFromFile(vk::ShaderStageFlagBits::eRaygenKHR, "Shaders/ColorfulTriangle/ColorfulTriangle.hlsl");
    // since HLSL allows multiple entry points in a single shader, we have all of the ray tracing stages in one shader
    // if compiling from glsl we would have to create a separate shader module for each stage
    auto shaderModule = mVRDev->CreateShaderFromSPV(shaderCreateInfo);

    // add the shader to the shader binding table which stores all the shaders for the pipeline
    mSBT.RayGenShader = shaderModule;
    mSBT.RayGenShader.Stage = vk::ShaderStageFlagBits::eRaygenKHR;
    // entry point for the ray generation shader, if there are multiple entry points in the shader.
    // In this case we are using one shader module with all the required entry points, 
    // but by default it is "main" because most GLSL compilers use "main" as the default entry point
    mSBT.RayGenShader.EntryPoint = "rgen"; 


    mSBT.MissShaders.push_back(shaderModule);
    mSBT.MissShaders.back().Stage = vk::ShaderStageFlagBits::eMissKHR;
    mSBT.MissShaders.back().EntryPoint = "miss";

    // [POI]
    //hit groups can contain multiple shaders, so there is another special struct for it
    vr::HitGroup hitGroup = {};
    hitGroup.ClosestHitShader = shaderModule;
    hitGroup.ClosestHitShader.EntryPoint = "chit";
    hitGroup.ClosestHitShader.Stage = vk::ShaderStageFlagBits::eClosestHitKHR;
    mSBT.HitGroups.push_back(hitGroup);
    
    // create the ray tracing pipeline

    // Create a layout for the pipeline
    mPipelineLayout = mVRDev->CreatePipelineLayout(mResourceDescriptorLayout);

    // create the ray tracing pipeline, a vk::Pipeline object
    mRTPipeline = mVRDev->CreateRayTracingPipeline(mPipelineLayout, mSBT, 1);

    // [POI]
    // Build the shader binding table, it is a buffer that contains the shaders for the pipeline and we can update hit record data if we want
    mSBTBuffer = mVRDev->CreateSBT(mRTPipeline, mSBT);

    // create a descriptor buffer for the ray tracing pipeline

    mResourceDescBuffer = mVRDev->CreateDescriptorBuffer(mResourceDescriptorLayout, mResourceBindings, vr::DescriptorBufferType::Resource);


}


void HelloTriangle::UpdateDescriptorSet()
{
    // Set the camera position
    // movement, rotation and input is handled by the Application Base class and we can modify the camera values as we like
    mCamera.Position = glm::vec3(0.0f, 0.0f, 2.5f);

    // [POI] We already provided each descriptor item with the pointer to a resource back when we created the descriptor set layout
    // so we can just update the resource values here
    // if we want to update the descriptor set with a new item, we can just reassign the vr::DescriptorItem::p*** with new items and update the descriptor set
    mVRDev->UpdateDescriptorBuffer(mResourceDescBuffer, mResourceBindings, vr::DescriptorBufferType::Resource);

}

void HelloTriangle::Update(vk::CommandBuffer renderCmd)
{
    // begin the command buffer
    renderCmd.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));


    mVRDev->BindDescriptorBuffer({ mResourceDescBuffer }, renderCmd);

    // set offsets for the descriptor set

    // if we have an array of buffer indices, we can set the offsets for each set here
    // set + n binds to the nth buffer we bound in BindDescriptorBuffer(...) and the nth offset in the offsets vector
    //  mVRDev->BindDescriptorSet(mPipelineLayout, 1st set, bufferindices vector, offsets vector, renderCmd);
    // look at vkCmdSetDescriptorBufferOffsetsEXT in the vulkan spec for more info
    // right now we only have one set bound at 0, so we just pass 0
    // 0th set binds to the 0th buffer we bound in BindDescriptorBuffer(...) which is the resource descriptor buffer and we don't have any offsets
    mVRDev->BindDescriptorSet(mPipelineLayout, 0, 0, 0, renderCmd);

    //transition the output image to general layout for ray tracing
    mVRDev->TransitionImageLayout(
        mOutputImageBuffer.Image,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eGeneral,
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1),
        renderCmd);

    renderCmd.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, mRTPipeline);

    // [POI]
    // RAYTRACING INITIATING
    mVRDev->DispatchRays(renderCmd, mRTPipeline, mSBTBuffer, mWidth, mHeight);

    //transition the swapchain image to transfer dst optimal
    mVRDev->TransitionImageLayout(mSwapchainStructs.SwapchainImages[mCurrentSwapchainImage],
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eTransferDstOptimal,
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1),
        renderCmd);

    //transition the output image to transfer src optimal
    mVRDev->TransitionImageLayout(mOutputImageBuffer.Image,
        vk::ImageLayout::eGeneral,
        vk::ImageLayout::eTransferSrcOptimal,
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1),
        renderCmd);

    // [POI]
    //blit the output image to the swapchain image
    renderCmd.blitImage(
        mOutputImageBuffer.Image, vk::ImageLayout::eTransferSrcOptimal,
        mSwapchainStructs.SwapchainImages[mCurrentSwapchainImage], vk::ImageLayout::eTransferDstOptimal,
        vk::ImageBlit(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
            { vk::Offset3D(0, 0, 0), vk::Offset3D(mWidth, mHeight, 1) },
            vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
            { vk::Offset3D(0, 0, 0), vk::Offset3D(mWidth, mHeight, 1) }),
        vk::Filter::eNearest);
    
    //transition the output image to general
    mVRDev->TransitionImageLayout(mOutputImageBuffer.Image,
        vk::ImageLayout::eTransferSrcOptimal,
        vk::ImageLayout::eGeneral,
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1),
        renderCmd);

    //transition the swapchain image to present
    mVRDev->TransitionImageLayout(mSwapchainStructs.SwapchainImages[mCurrentSwapchainImage],
        vk::ImageLayout::eTransferDstOptimal,
        vk::ImageLayout::ePresentSrcKHR,
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1),
        renderCmd);

    // end the command buffer
    renderCmd.end();



    //wait for previous frame to finish, also resets the fence and command buffer that we are waiting for
    WaitForRendering();

    //this function will submit the command buffer to the queue and present the image to the screen
    Present(renderCmd);

    // update the camera, UpdateCamera() function handles the uniform buffer for the camera and the camera movement
    UpdateCamera();

}


void HelloTriangle::Stop()
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
	Application* app = new HelloTriangle();

    app->Start();
    app->Run();
    app->Stop();

    delete app;
}