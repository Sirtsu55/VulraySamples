#include "Vulray/Vulray.h"
#include "Common.h"
#include "Application.h"
#include "FileRead.h"
#include "ShaderCompiler.h"

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

    
    vr::ShaderBindingTable mSBT;    // contains the raygen, miss and hit groups
	vr::SBTBuffer mSBTBuffer;       // contains the shader records for the SBT

    vk::DescriptorPool mDescriptorPool = nullptr;
    vr::DescriptorSet mDescriptorSet;
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
        1.0f, 1.0f, 0.0f,
        -1.0f, 1.0f, 0.0f,
        0.0f,  -1.0f, 0.0f
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

    mBLASHandle = blasHandle;

    // [POI]
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

    auto BLASscratchBuffer = mVRDev->BuildBLAS(buildInfos, buildCmd); // Add build commands to command buffer and retrieve scratch buffer for the build 

    mVRDev->AddAccelerationBuildBarrier(buildCmd); // Add a barrier to the command buffer to make sure the BLAS build is finished before the TLAS build starts

    // Add build commands to command buffer and retrieve scratch buffer for the build
    // We can reuse the scratch buffer from here to update the TLAS, but for now we don't update
    auto TLASScratchBuffer = mVRDev->BuildTLAS(tlasBuildInfo, InstanceBuffer, 1, buildCmd); 

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

    // create a descriptor pool
    std::vector<vk::DescriptorPoolSize> poolSizes = {
        vk::DescriptorPoolSize(vk::DescriptorType::eAccelerationStructureKHR, 1),
        vk::DescriptorPoolSize(vk::DescriptorType::eStorageImage, 1),
        vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 1),
    };

    mDescriptorPool = mVRDev->CreateDescriptorPool(poolSizes, vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind | vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 1);


    // [POI]
    // vr::DescriptorSet is a wrapper to encapsulate all Descriptor related structs
    // Now we create a descriptor layout for the ray tracing pipeline
    // last parameter is a pointer to the items vector, so we can use it later to create the descriptor set
    // for now we have only one item, so we just pass the address of the first element
    // if we want to update the descriptor set later with another item,
    // we can just reassign the vr::DescriptorItem::pItems with new items and update the descriptor set
   mDescriptorSet.Items = {
        vr::DescriptorItem(0, vk::DescriptorType::eAccelerationStructureKHR, vk::ShaderStageFlagBits::eRaygenKHR, 1, &mTLASHandle.AccelerationStructure),
        vr::DescriptorItem(1, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eRaygenKHR, 1, &mOutputImageView),
        vr::DescriptorItem(2, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eRaygenKHR, 1, &mCameraUniformBuffer.Buffer),
    };


    mDescriptorSet.Layout = mVRDev->CreateDescriptorSetLayout(mDescriptorSet.Items); // create a descriptor set layout, for the ray tracing pipeline

    
    // create shaders for the ray tracing pipeline
    // Spir-V bytecode is required in the info struct


    vr::ShaderCreateInfo shaderCreateInfo = {};

    VULRAY_LOG_INFO("Compiling Shaders");

    shaderCreateInfo.Stage = vk::ShaderStageFlagBits::eRaygenKHR;
    // Shader compiler class from from Base/ will perform GLSL -> SPIR-V translation, it takes about 1 second to compile the three shaders
    shaderCreateInfo.SPIRVCode = std::move(mShaderCompiler.CompileSPIRVFromFile(shaderCreateInfo.Stage, SHADER_DIR"/HelloTriangle.rgen"));
    // add the shader to the shader binding table which stores all the shaders for the pipeline
    mSBT.RayGenShader = mVRDev->CreateShaderFromSPV(shaderCreateInfo);


    shaderCreateInfo.Stage = vk::ShaderStageFlagBits::eMissKHR;
    shaderCreateInfo.SPIRVCode = std::move(mShaderCompiler.CompileSPIRVFromFile(shaderCreateInfo.Stage, SHADER_DIR"/HelloTriangle.rmiss"));
    mSBT.MissShaders.push_back(mVRDev->CreateShaderFromSPV(shaderCreateInfo));


    shaderCreateInfo.Stage = vk::ShaderStageFlagBits::eClosestHitKHR;
    shaderCreateInfo.SPIRVCode = std::move(mShaderCompiler.CompileSPIRVFromFile(shaderCreateInfo.Stage, SHADER_DIR"/HelloTriangle.rchit"));

    VULRAY_LOG_INFO("Shaders Compiled");

    // [POI]
    //hit groups can contain multiple shaders, so there is another special struct for it
    vr::HitGroup hitGroup = {};

    hitGroup.ClosestHitShader = mVRDev->CreateShaderFromSPV(shaderCreateInfo);

    mSBT.HitGroups.push_back(hitGroup);
    
    // create the ray tracing pipeline

    // Create a layout for the pipeline
    mPipelineLayout = mVRDev->CreatePipelineLayout(mDescriptorSet.Layout);

    // create the ray tracing pipeline, a vk::Pipeline object
    mRTPipeline = mVRDev->CreateRayTracingPipeline(mPipelineLayout, mSBT, 1);

    // [POI]
    // Build the shader binding table, it is a buffer that contains the shaders for the pipeline and we can update hit record data if we want
    mSBTBuffer = mVRDev->CreateSBT(mRTPipeline, mSBT);

    // create a descriptor set for the ray tracing pipeline
    mDescriptorSet.Set = mVRDev->AllocateDescriptorSet(mDescriptorPool, mDescriptorSet.Layout);

}


void HelloTriangle::UpdateDescriptorSet()
{
    // Set the camera position
    // movement, rotation and input is handled by the Application Base class and we can modify the camera values as we like
    mCamera.Pos = glm::vec3(0.0f, 0.0f, 2.5f);


    std::vector<vk::WriteDescriptorSet> descUpdate; // 3 descriptors to update
    descUpdate.reserve(3);

    //acceleration structure at binding 0, Look at Pipeline layout created earlier
    descUpdate.push_back(mDescriptorSet.GetWriteDescriptorSets(vk::DescriptorType::eAccelerationStructureKHR, 0));

    // [POI]
    // Get*Info(...) returns an Info struct for the write descriptor struct with the respective vulkan object pointer stored in vr::DescriptorItem::pItems
    // the first parameter in Get*Info(...) means the n:th item in the DescriptorItem::pItems pointer
    // if the pointer is null for the item, the pointer of the item in the struct will be null
    // NOTE: we already set the pointer to the vulkan objects before in CreateRTPipeline() | line 207, so we don't need to set it again
    // if we were to update the descriptor with another object we would have to set a new pointer for vr::DescriptorItem::pItems
    auto accelInfo = mDescriptorSet.Items[0].GetAccelerationStructureInfo(0); // get the acceleration structure info (vk::WriteDescriptorSetAccelerationStructureKHR)
    descUpdate[0].setPNext(&accelInfo); // set the next pointer to the acceleration structure info
    descUpdate[0].setDescriptorCount(1); // we are updating only one descriptor
    
    //storage image at binding 1
    descUpdate.push_back(mDescriptorSet.GetWriteDescriptorSets(vk::DescriptorType::eStorageImage, 1));
    auto imageInfo = mDescriptorSet.Items[1].GetImageInfo(0); 
    descUpdate[1].setPImageInfo(&imageInfo); // set the image info
    descUpdate[1].setDescriptorCount(1);

    //uniform buffer at binding 2, This is the camera buffer and it gets updated every frame in the base class
    descUpdate.push_back(mDescriptorSet.GetWriteDescriptorSets(vk::DescriptorType::eUniformBuffer, 2));
    auto bufferInfo = mDescriptorSet.Items[2].GetBufferInfo(0); 
    descUpdate[2].setPBufferInfo(&bufferInfo); // set the buffer info
    descUpdate[2].setDescriptorCount(1);
    
    // update the descriptor set
    mVRDev->UpdateDescriptorSet(descUpdate);
}

void HelloTriangle::Update(vk::CommandBuffer renderCmd)
{
    // begin the command buffer
    renderCmd.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));


    //transition the output image to general layout for ray tracing
    mVRDev->TransitionImageLayout(
        mOutputImage.Image,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eGeneral,
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1),
        renderCmd);

    // bind the descriptor set for ray tracing
    renderCmd.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, mPipelineLayout, 0, 1, &mDescriptorSet.Set, 0, nullptr);

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
    mVRDev->TransitionImageLayout(mOutputImage.Image,
        vk::ImageLayout::eGeneral,
        vk::ImageLayout::eTransferSrcOptimal,
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1),
        renderCmd);

    // [POI]
    //blit the output image to the swapchain image
    renderCmd.blitImage(
        mOutputImage.Image, vk::ImageLayout::eTransferSrcOptimal,
        mSwapchainStructs.SwapchainImages[mCurrentSwapchainImage], vk::ImageLayout::eTransferDstOptimal,
        vk::ImageBlit(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
            { vk::Offset3D(0, 0, 0), vk::Offset3D(mWidth, mHeight, 1) },
            vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
            { vk::Offset3D(0, 0, 0), vk::Offset3D(mWidth, mHeight, 1) }),
        vk::Filter::eNearest);
    
    //transition the output image to general
    mVRDev->TransitionImageLayout(mOutputImage.Image,
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
    mVRDev->DestroyShader(mSBT.RayGenShader);
    mVRDev->DestroyShader(mSBT.MissShaders[0]);
    mVRDev->DestroyShader(mSBT.HitGroups[0].ClosestHitShader);

    mDevice.destroyPipeline(mRTPipeline);
    mDevice.destroyPipelineLayout(mPipelineLayout);
    mDevice.destroyDescriptorSetLayout(mDescriptorSet.Layout);
    mDevice.freeDescriptorSets(mDescriptorPool, mDescriptorSet.Set);
    mDevice.destroyDescriptorPool(mDescriptorPool);

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