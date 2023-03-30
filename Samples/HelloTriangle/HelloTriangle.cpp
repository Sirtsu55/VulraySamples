#include "Vulray/Vulray.h"
#include "Common.h"
#include "Application.h"
#include "FileRead.h"

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
    

    vr::AllocatedBuffer mVertexBuffer;
    vr::AllocatedBuffer mIndexBuffer;

    
    vr::ShaderBindingTable mSBT; // contains the raygen, miss and hit groups
	vr::SBTBuffer mSBTBuffer; // contains the shader records for the SBT

    vk::DescriptorPool mDescriptorPool = nullptr;
    vr::DescriptorSet mDescriptorSet;
    vk::Pipeline mRTPipeline = nullptr;
    vk::PipelineLayout mPipelineLayout = nullptr;


    vr::AllocatedBuffer mUniformBuffer;

    vr::BLASHandle mBLASHandle;
    vr::TLASHandle mTLASHandle;
    vr::TLASBuildInfo mTLASBuildInfo;

    vr::AllocatedBuffer mInstanceBuffer;
    vr::AllocatedBuffer mScratchBuffer;


};

void HelloTriangle::CreateAS()
{
    // vertex and index data for the triangle
    float vertices1[] = {
        -1.0f, 1.0f, 0.0f,
        -3.0f, 1.0f, 0.0f,
        -2.0f,  -1.0f, 0.0f
    };
    float vertices2[] = {
        1.0f, 1.0f, 0.0f,
        -1.0f, 1.0f, 0.0f,
        0.0f,  -1.0f, 0.0f
    };
    float vertices3[] = {
        3.0f, 1.0f, 0.0f,
        1.0f, 1.0f, 0.0f,
        2.0f,  -1.0f, 0.0f
    };
    uint32_t indices[] = { 0, 1, 2 };

    // create a buffer for the vertices and copy the data to it
    mVertexBuffer = mVRDev->CreateBuffer(
        sizeof(float) * 3 * 3 * 3, // 3 vertices, 3 floats per vertex, 3 triangles
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, // we will be writing to this buffer on the CPU, so we need to set this flag, the buffer is also host visible so it is not fast GPU memory
        vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR); // this buffer will be used as a source for the BLAS
    mIndexBuffer = mVRDev->CreateBuffer(
        sizeof(uint32_t) * 3, // 3 vertices, 3 floats per vertex
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, // same as above
        vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR);


    // upload the vertex data to the buffer, UpdateBuffer(...) will use mapping the buffer and memcpy 
    mVRDev->UpdateBuffer(mVertexBuffer, vertices1, sizeof(float) * 3 * 3); 
    mVRDev->UpdateBuffer(mIndexBuffer, indices, sizeof(uint32_t) * 3); // same as above

    mVRDev->UpdateBuffer(mVertexBuffer, vertices2, sizeof(float) * 3 * 3, sizeof(float) * 3 * 3); 
    mVRDev->UpdateBuffer(mVertexBuffer, vertices3, sizeof(float) * 3 * 3, sizeof(float) * 3 * 3 * 2); 

	// Create info struct for the BLAS
    vr::BLASCreateInfo blasCreateInfo = {};
    blasCreateInfo.Flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
    
	// triangle geometry data, BLASCreateInfo can have multiple geometries
    vr::GeometryData geomData = {};

    geomData.VertexFormat = vk::Format::eR32G32B32Sfloat;
    geomData.Stride = sizeof(float) * 3; // 3 floats per vertex: x, y, z
    geomData.IndexFormat = vk::IndexType::eUint32;
    geomData.PrimitiveCount = 1;
    geomData.VertexDevAddress = mVertexBuffer.DevAddress;
    geomData.IndexDevAddress = mIndexBuffer.DevAddress;
	//Helper Function: vr::FillBottomAccelGeometry(...) takes vr::GeometryData and converts it to a VkAccelerationStructureGeometryKHR struct that is required
	//by vr::BLASCreateInfo. Vertex, index and optionally transform buffers are  required for this function
	
    // add triangle geometry to the BLAS create info
    // NOTE: BLASCreateInfo can have multiple geometries and they are of type VkAccelerationStructureGeometryKHR
    blasCreateInfo.Geometries.push_back(geomData);

    geomData.VertexDevAddress = mVertexBuffer.DevAddress + sizeof(float) * 3 * 3;
    geomData.IndexDevAddress = mIndexBuffer.DevAddress;

    blasCreateInfo.Geometries.push_back(geomData);

    geomData.VertexDevAddress = mVertexBuffer.DevAddress + sizeof(float) * 3 * 3 * 2;

    blasCreateInfo.Geometries.push_back(geomData);

    // this only creates the BLAS, it does not build it
	// it creates acceleration structure and allocates memory for it and scratch memory
    auto[blasHandle, blasBuildInfo] = mVRDev->CreateBLAS(blasCreateInfo); 

    mBLASHandle = blasHandle;


    // create a TLAS
    vr::TLASCreateInfo tlasCreateInfo = {};
    tlasCreateInfo.Flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
    tlasCreateInfo.MaxInstanceCount = 1;
    auto [tlasHandle, tlasBuildInfo] = mVRDev->CreateTLAS(tlasCreateInfo);

    mTLASHandle = tlasHandle;
    mTLASBuildInfo = tlasBuildInfo;

    // create a buffer for the instance data
    mInstanceBuffer = mVRDev->CreateInstanceBuffer(1);

    
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

    // upload the instance data to the buffer
    mVRDev->UpdateBuffer(mInstanceBuffer, &inst, sizeof(vk::AccelerationStructureInstanceKHR), 0);


    
    // create a command buffer to build the BLAS and TLAS, mGraphicsPool is a command pool that is created in the Base Application class
    auto buildCmd = mVRDev->CreateCommandBuffer(mGraphicsPool); 

    buildCmd.begin(vk::CommandBufferBeginInfo().setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

    // build the AS
    std::vector<vr::BLASBuildInfo> buildInfos = { blasBuildInfo };
    auto BLASscratchBuffer = mVRDev->BuildBLAS(buildInfos, buildCmd);

    mVRDev->AddAccelerationBuildBarrier(buildCmd);
    mScratchBuffer = mVRDev->BuildTLAS(tlasBuildInfo, mInstanceBuffer, 1, buildCmd);

    buildCmd.end();

    // submit the command buffer and wait for it to finish
    auto submitInfo = vk::SubmitInfo()
        .setCommandBufferCount(1)
        .setPCommandBuffers(&buildCmd);

    mQueues.GraphicsQueue.submit(submitInfo, nullptr);
    
    mDevice.waitIdle();


    mVRDev->DestroyBuffer(BLASscratchBuffer);


    // free the command buffer
    mDevice.freeCommandBuffers(mGraphicsPool, buildCmd);
}


void HelloTriangle::CreateRTPipeline()
{
    // create a uniform buffer
    mUniformBuffer = mVRDev->CreateBuffer(
        sizeof(float) * 4 * 4 * 2, // two 4x4 matrix
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, // we will be writing to this buffer on the CPU
        vk::BufferUsageFlagBits::eUniformBuffer); // its a uniform buffer


    // create a descriptor pool
    std::vector<vk::DescriptorPoolSize> poolSizes = {
        vk::DescriptorPoolSize(vk::DescriptorType::eAccelerationStructureKHR, 1),
        vk::DescriptorPoolSize(vk::DescriptorType::eStorageImage, 1),
        vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 1),
    };

    mDescriptorPool = mVRDev->CreateDescriptorPool(poolSizes, vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind | vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 1);

    // create a descriptor layout for the ray tracing pipeline
    // last parameter is a pointer to the items vector, so we can use it later to create the descriptor set
    // for now we have only one item, so we just pass the address of the first element
   mDescriptorSet.Items = {
        vr::DescriptorItem(0, vk::DescriptorType::eAccelerationStructureKHR, vk::ShaderStageFlagBits::eRaygenKHR, 1, &mTLASHandle.AccelerationStructure, vk::DescriptorBindingFlagBits::eUpdateAfterBind),
        vr::DescriptorItem(1, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eRaygenKHR, 1, &mOutputImageView),
        vr::DescriptorItem(2, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eRaygenKHR, 1, &mUniformBuffer.Buffer),
    };

    mDescriptorSet.Layout = mVRDev->CreateDescriptorSetLayout(mDescriptorSet.Items); // create a descriptor set layout, for the ray tracing pipeline
    // create shaders for the ray tracing pipeline
    // user can also supply Spir-V bytecode directly in the info struct
    // by filling vr::ShaderCreateInfo::SPIRVCode vector

    vr::ShaderCreateInfo shaderCreateInfo = {};
    FileRead(SHADER_DIR"/HelloTriangle.rgen.spv", shaderCreateInfo.SPIRVCode); // utility function to read a file into a vector
    shaderCreateInfo.Stage = vk::ShaderStageFlagBits::eRaygenKHR;

    // add the shader to the shader binding table which stores all the shaders for the pipeline
    mSBT.RayGenShader = mVRDev->CreateShaderFromSPV(shaderCreateInfo);

    shaderCreateInfo.Stage = vk::ShaderStageFlagBits::eMissKHR;
    FileRead(SHADER_DIR"/HelloTriangle.rmiss.spv", shaderCreateInfo.SPIRVCode);
    mSBT.MissShaders.push_back(mVRDev->CreateShaderFromSPV(shaderCreateInfo));

    shaderCreateInfo.Stage = vk::ShaderStageFlagBits::eClosestHitKHR;
    FileRead(SHADER_DIR"/HelloTriangle.rchit.spv", shaderCreateInfo.SPIRVCode);

    //hit groups can contain multiple shaders, so there is another struct for it
    vr::HitGroup hitGroup = {};
    hitGroup.ClosestHitShader = mVRDev->CreateShaderFromSPV(shaderCreateInfo);
    mSBT.HitGroups.push_back(hitGroup);

    // create the ray tracing pipeline
    mPipelineLayout = mVRDev->CreatePipelineLayout(mDescriptorSet.Layout);

    mRTPipeline = mVRDev->CreateRayTracingPipeline(mPipelineLayout, mSBT, 1);

    mSBTBuffer = mVRDev->CreateSBT(mRTPipeline, mSBT);

    // create a descriptor set for the ray tracing pipeline
    mDescriptorSet.Set = mVRDev->AllocateDescriptorSet(mDescriptorPool, mDescriptorSet.Layout);

}

void HelloTriangle::Start()
{
    //defined in the base application class, creates an output image to render to
    CreateStoreImage();
    
    CreateAS();
    
    CreateRTPipeline();
    UpdateDescriptorSet();

}
void HelloTriangle::UpdateDescriptorSet()
{

    glm::vec3 loc = glm::vec3(0.0f, 0.0f, -2.5f);
    glm::vec3 forward = glm::vec3(0.0f, 0.0f, 1.0f) + loc;

    glm::mat4 view = glm::lookAt(loc, forward, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), (float)mWidth / (float)(mHeight), 0.1f, 512.0f);

    //uniform buffer contains the inverse view and projection matrices
    glm::mat4 mats[2] = { glm::inverse(view), glm::inverse(proj) };
    auto size = sizeof(glm::mat4) * 2;
    mVRDev->UpdateBuffer(mUniformBuffer, mats, size); // upload the data to the uniform buffer

    std::vector<vk::WriteDescriptorSet> descUpdate; // 3 descriptors to update
    //acceleration structure at binding 0
    descUpdate.push_back(mDescriptorSet.GetWriteDescriptorSets(vk::DescriptorType::eAccelerationStructureKHR, 0));
    auto accelInfo = mDescriptorSet.Items[0].GetAccelerationStructureInfo(0);
    descUpdate[0].setPNext(&accelInfo); // set the next pointer to the acceleration structure info
    descUpdate[0].setDescriptorCount(1);
    
    //storage image at binding 1
    descUpdate.push_back(mDescriptorSet.GetWriteDescriptorSets(vk::DescriptorType::eStorageImage, 1));
    // items[0] is the storage image, it is the 0th element from the pointer above when we created the descriptor item
    auto imageInfo = mDescriptorSet.Items[1].GetImageInfo(0);
    descUpdate[1].setPImageInfo(&imageInfo);
    descUpdate[1].setDescriptorCount(1);

    //uniform buffer at binding 2
    descUpdate.push_back(mDescriptorSet.GetWriteDescriptorSets(vk::DescriptorType::eUniformBuffer, 2));
    auto bufferInfo = mDescriptorSet.Items[2].GetBufferInfo(0);
    descUpdate[2].setPBufferInfo(&bufferInfo);
    descUpdate[2].setDescriptorCount(1);
    

    // update the descriptor set
    mVRDev->UpdateDescriptorSet(descUpdate);
}

void HelloTriangle::Update(vk::CommandBuffer renderCmd)
{

    renderCmd.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));


    //transition the output image to general layout for ray tracing
    mVRDev->TransitionImageLayout(
        mOutputImage.Image,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eGeneral,
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1),
        renderCmd);

    renderCmd.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, mPipelineLayout, 0, 1, &mDescriptorSet.Set, 0, nullptr);
    // ray tracing dispatch
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

    //copy the output image to the swapchain image
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

    renderCmd.end();



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
            0.0f, 0.0f, 1.0f, sinf(glfwGetTime())
    };

    // upload the instance data to the buffer
    mVRDev->UpdateBuffer(mInstanceBuffer, &inst, sizeof(vk::AccelerationStructureInstanceKHR), 0);

    auto buildCmd = mVRDev->CreateCommandBuffer(mGraphicsPool); 

    buildCmd.begin(vk::CommandBufferBeginInfo().setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

    // build the AS
    auto[tlasHandle, buildInfo] = mVRDev->UpdateTLAS(mTLASHandle, mTLASBuildInfo);

    mTLASHandle = tlasHandle;
    mTLASBuildInfo = buildInfo;

    // returns the same scratch buffer
    mScratchBuffer = mVRDev->BuildTLAS(buildInfo, mInstanceBuffer, 1, buildCmd, &mScratchBuffer);

    buildCmd.end();

    // submit the command buffer and wait for it to finish
    auto submitInfo = vk::SubmitInfo()
        .setCommandBufferCount(1)
        .setPCommandBuffers(&buildCmd);

    mQueues.GraphicsQueue.submit(submitInfo, nullptr);
    
    mDevice.waitIdle();

    std::vector<vk::WriteDescriptorSet> descUpdate; // 3 descriptors to update
    //acceleration structure at binding 0
    descUpdate.push_back(mDescriptorSet.GetWriteDescriptorSets(vk::DescriptorType::eAccelerationStructureKHR, 0));
    mDescriptorSet.Items[0].pItems = &mTLASHandle.AccelerationStructure;
    auto accelInfo = mDescriptorSet.Items[0].GetAccelerationStructureInfo(0);

    descUpdate[0].setPNext(&accelInfo); // set the next pointer to the acceleration structure info
    descUpdate[0].setDescriptorCount(1);
    mVRDev->UpdateDescriptorSet(descUpdate);

    //wait for previous frame to finish, also resets the fence and command buffer that we are waiting for
    WaitForRendering();

    //this function will submit the command buffer to the queue and present the image to the screen
    Present(renderCmd);



}


void HelloTriangle::Stop()
{
    auto _ = mDevice.waitForFences(mRenderFence, VK_TRUE, UINT64_MAX);
    // destroy all the resources we created

    mVRDev->DestroyBuffer(mInstanceBuffer);
    mVRDev->DestroyBuffer(mScratchBuffer);

    mVRDev->DestroyBuffer(mUniformBuffer);
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