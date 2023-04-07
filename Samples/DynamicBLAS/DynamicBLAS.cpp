#include "Vulray/Vulray.h"
#include "Common.h"
#include "Application.h"
#include "FileRead.h"



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
    

    vr::AllocatedBuffer mVertexBuffer;
    vr::AllocatedBuffer mIndexBuffer;

    
    vr::ShaderBindingTable mSBT;    
	vr::SBTBuffer mSBTBuffer;      

    vk::DescriptorPool mDescriptorPool = nullptr;
    vr::DescriptorSet mDescriptorSet;
    vk::Pipeline mRTPipeline = nullptr;
    vk::PipelineLayout mPipelineLayout = nullptr;


    vr::AllocatedBuffer mUniformBuffer;

    vr::BLASHandle mBLASHandle;

    // Save the build info for the BLAS so we can update it later
    vr::BLASBuildInfo mBLASBuildInfo;

    vr::AllocatedBuffer mUpdateScratchBuffer;

    vr::TLASHandle mTLASHandle;


};

void DynamicBLAS::Start()
{
    CreateStoreImage();
    
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
    // modify the top middle vertex of the triangle
    float new_vertex_pos[3] = {0.0f, -1.0f, sinf(glfwGetTime())};
    // float new_vertex_pos[3] = {0.0f, 1.0f, sinf(glfwGetTime())};

    // [POI] Additional Info
    // Vulkan requires the whole buffer with same size and the same number of primitives as the source BLAS, so if you want to update only one primitive, 
    // you still have to give vulkan the whole buffer, not parts that you want to update

    //offset the vertex buffer by 2 triangles and write new vertices, because we only want to update the top middle vertex, which is triangle 3
    mVRDev->UpdateBuffer(mVertexBuffer, new_vertex_pos, sizeof(float) * 3, sizeof(float) * 3 * 2);

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
    // create a uniform buffer
    mUniformBuffer = mVRDev->CreateBuffer(
        sizeof(float) * 4 * 4 * 2,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        vk::BufferUsageFlagBits::eUniformBuffer); 


    // create a descriptor pool
    std::vector<vk::DescriptorPoolSize> poolSizes = {
        vk::DescriptorPoolSize(vk::DescriptorType::eAccelerationStructureKHR, 1),
        vk::DescriptorPoolSize(vk::DescriptorType::eStorageImage, 1),
        vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 1),
    };

    mDescriptorPool = mVRDev->CreateDescriptorPool(poolSizes, vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind | vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 1);


   mDescriptorSet.Items = {
        vr::DescriptorItem(0, vk::DescriptorType::eAccelerationStructureKHR, vk::ShaderStageFlagBits::eRaygenKHR, 1, &mTLASHandle.AccelerationStructure),
        vr::DescriptorItem(1, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eRaygenKHR, 1, &mOutputImageView),
        vr::DescriptorItem(2, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eRaygenKHR, 1, &mUniformBuffer.Buffer),
    };


    mDescriptorSet.Layout = mVRDev->CreateDescriptorSetLayout(mDescriptorSet.Items); // create a descriptor set layout, for the ray tracing pipeline

    
    // create shaders for the ray tracing pipeline

    vr::ShaderCreateInfo shaderCreateInfo = {};
    FileRead(SHADER_DIR"/HelloTriangle.rgen.spv", shaderCreateInfo.SPIRVCode);
    shaderCreateInfo.Stage = vk::ShaderStageFlagBits::eRaygenKHR;

    mSBT.RayGenShader = mVRDev->CreateShaderFromSPV(shaderCreateInfo);

    shaderCreateInfo.Stage = vk::ShaderStageFlagBits::eMissKHR;
    FileRead(SHADER_DIR"/HelloTriangle.rmiss.spv", shaderCreateInfo.SPIRVCode);
    mSBT.MissShaders.push_back(mVRDev->CreateShaderFromSPV(shaderCreateInfo)); 

    shaderCreateInfo.Stage = vk::ShaderStageFlagBits::eClosestHitKHR;
    FileRead(SHADER_DIR"/HelloTriangle.rchit.spv", shaderCreateInfo.SPIRVCode);

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


void DynamicBLAS::UpdateDescriptorSet()
{

    // Configure the camera for the scene 
    glm::vec3 loc = glm::vec3(0.0f, 0.0f, -2.5f);
    glm::vec3 forward = glm::vec3(0.0f, 0.0f, 1.0f) + loc;

    glm::mat4 view = glm::lookAt(loc, forward, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)mWidth / (float)(mHeight), 0.1f, 512.0f);

    //uniform buffer contains the inverse view and projection matrices
    glm::mat4 mats[2] = { glm::inverse(view), glm::inverse(proj) };
    auto size = sizeof(glm::mat4) * 2;
    mVRDev->UpdateBuffer(mUniformBuffer, mats, size); 

    std::vector<vk::WriteDescriptorSet> descUpdate; // 3 descriptors to update
    descUpdate.reserve(3);

    descUpdate.push_back(mDescriptorSet.GetWriteDescriptorSets(vk::DescriptorType::eAccelerationStructureKHR, 0));

    auto accelInfo = mDescriptorSet.Items[0].GetAccelerationStructureInfo(0); 
    descUpdate[0].setPNext(&accelInfo); // set the next pointer to the acceleration structure info
    descUpdate[0].setDescriptorCount(1); 
    
    //storage image at binding 1
    descUpdate.push_back(mDescriptorSet.GetWriteDescriptorSets(vk::DescriptorType::eStorageImage, 1));
    auto imageInfo = mDescriptorSet.Items[1].GetImageInfo(0); 
    descUpdate[1].setPImageInfo(&imageInfo); // set the image info
    descUpdate[1].setDescriptorCount(1);

    //uniform buffer at binding 2
    descUpdate.push_back(mDescriptorSet.GetWriteDescriptorSets(vk::DescriptorType::eUniformBuffer, 2));
    auto bufferInfo = mDescriptorSet.Items[2].GetBufferInfo(0); 
    descUpdate[2].setPBufferInfo(&bufferInfo); // set the buffer info
    descUpdate[2].setDescriptorCount(1);
    
    // update the descriptor set
    mVRDev->UpdateDescriptorSet(descUpdate);
}

void DynamicBLAS::Update(vk::CommandBuffer renderCmd)
{
    renderCmd.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

    UpdateBLAS(renderCmd);

    mVRDev->TransitionImageLayout(
        mOutputImage.Image,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eGeneral,
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1),
        renderCmd);

    renderCmd.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, mPipelineLayout, 0, 1, &mDescriptorSet.Set, 0, nullptr);

    mVRDev->DispatchRays(renderCmd, mRTPipeline, mSBTBuffer, mWidth, mHeight);

    mVRDev->TransitionImageLayout(mSwapchainStructs.SwapchainImages[mCurrentSwapchainImage],
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eTransferDstOptimal,
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1),
        renderCmd);

    mVRDev->TransitionImageLayout(mOutputImage.Image,
        vk::ImageLayout::eGeneral,
        vk::ImageLayout::eTransferSrcOptimal,
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1),
        renderCmd);

    renderCmd.blitImage(
        mOutputImage.Image, vk::ImageLayout::eTransferSrcOptimal,
        mSwapchainStructs.SwapchainImages[mCurrentSwapchainImage], vk::ImageLayout::eTransferDstOptimal,
        vk::ImageBlit(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
            { vk::Offset3D(0, 0, 0), vk::Offset3D(mWidth, mHeight, 1) },
            vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
            { vk::Offset3D(0, 0, 0), vk::Offset3D(mWidth, mHeight, 1) }),
        vk::Filter::eNearest);
    
    mVRDev->TransitionImageLayout(mOutputImage.Image,
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





}


void DynamicBLAS::Stop()
{
    auto _ = mDevice.waitForFences(mRenderFence, VK_TRUE, UINT64_MAX);
    
    mVRDev->DestroyBuffer(mUpdateScratchBuffer);

    // destroy all the resources we created
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
	Application* app = new DynamicBLAS();

    app->Start();
    app->Run();
    app->Stop();

    delete app;
}