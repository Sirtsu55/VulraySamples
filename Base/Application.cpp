#include "Common.h"
#include "Application.h"



Application::Application()
{
    glfwInit(); //Initializes the GLFW library


    if(glfwVulkanSupported() != GLFW_TRUE)
    {
        VULRAY_LOG_ERROR("Vulkan is not supported on this system");
        throw std::runtime_error("Vulkan is not supported on this system");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); //Sets the client API to GLFW_NO_API, which means that the application will not create an OpenGL context

    mWindow = glfwCreateWindow(mWidth, mHeight, "HelloTriangle", nullptr, nullptr); //Creates a window

    // specify debug callback by passing a pointer to the function if you want to use it
    // vr::LogCallback = logcback;


    vr::VulkanBuilder builder;
#ifndef NDEBUG
    builder.EnableDebug = true;
#else
    builder.EnableDebug = false;
#endif

    // Get the required extensions
    uint32_t count;
    const char** extensions = glfwGetRequiredInstanceExtensions(&count);
    // Add the extensions to the builder
    for(uint32_t i = 0; i < count; i++)
    {
        builder.InstanceExtensions.push_back(extensions[i]);
    }
    // Create the instance
    mInstance = builder.CreateInstance();


    // user can also add extra extensions to the device if they want to use them
    // builder.DeviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME); 

    // user can also enable extra features if they want to use them
    // features from 1.0 to 1.2 are available 
    // builder.PhysicalDeviceFeatures12.bufferDeviceAddress = true;
    
    // Create the surface for the window
    VkSurfaceKHR surface;
    auto r = glfwCreateWindowSurface(mInstance.InstanceHandle, mWindow, nullptr, &surface);

    mSurface = surface;

    // Pick the physical device to use
    mPhysicalDevice = builder.PickPhysicalDevice(mSurface);

    // Create the logical device
    mDevice = builder.CreateDevice();

    // Get the queues for the logical device
    mQueues = builder.GetQueues();

    assert(mQueues.GraphicsQueue && "Graphics queue is null");


    // This code creates a swapchain with a particular format and dimensions.

    auto SwapchainBuilder = vr::SwapchainBuilder(mDevice, mPhysicalDevice, mSurface, mQueues.GraphicsIndex, mQueues.PresentIndex);
    SwapchainBuilder.Height = mWidth;
    SwapchainBuilder.Width = mHeight;
	SwapchainBuilder.BackBufferCount = 2;
    SwapchainBuilder.ImageUsage = vk::ImageUsageFlagBits::eTransferDst;
	SwapchainBuilder.DesiredFormat = vk::Format::eB8G8R8A8Unorm; 
    mSwapchainStructs = SwapchainBuilder.BuildSwapchain();
    
    //Create command pools
	vk::CommandPoolCreateInfo poolInfo = {};
	poolInfo.queueFamilyIndex = mQueues.GraphicsIndex;
	poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer; // release command buffers back to pool
	mGraphicsPool = mDevice.createCommandPool(poolInfo);
    
	mMaxFramesInFlight = static_cast<uint32_t>(mSwapchainStructs.SwapchainImageViews.size());
    
	//create command buffers
	vk::CommandBufferAllocateInfo allocInfo = {};
	allocInfo.commandPool = mGraphicsPool;
	allocInfo.level = vk::CommandBufferLevel::ePrimary;
	allocInfo.commandBufferCount = 1;


    //Create semaphores
    vk::SemaphoreCreateInfo semaphoreInfo = {};
    //create semaphores for present
    mRenderSemaphore = mDevice.createSemaphore(semaphoreInfo);
    mPresentSemaphore = mDevice.createSemaphore(semaphoreInfo);

    //Create fences
    vk::FenceCreateInfo fenceInfo = {};
    fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;
    mRenderFence = mDevice.createFence(fenceInfo);


    mVRDev = new vr::VulrayDevice(mInstance.InstanceHandle, mDevice, mPhysicalDevice);

    mRTRenderCmd = mVRDev->CreateCommandBuffers(mGraphicsPool, 2);
    

}

void Application::Update(vk::CommandBuffer renderCmd)
{

}

void Application::Start()
{
}

void Application::Stop()
{
}

void Application::BeginFrame()
{

    //Acquire the next image
    auto result = mDevice.acquireNextImageKHR(mSwapchainStructs.SwapchainHandle, UINT64_MAX, mRenderSemaphore, nullptr);

    if(result.result == vk::Result::eErrorOutOfDateKHR)
    {
        VULRAY_LOG_ERROR("Swapchain out of date");
        return;
    }
    else if(result.result != vk::Result::eSuccess && result.result != vk::Result::eSuboptimalKHR)
    {
        throw std::runtime_error("Failed to acquire swapchain image");
    }

    mCurrentSwapchainImage = result.value;


}

void Application::WaitForRendering()
{
    //Wait for the fence to be signaled by the GPU
    auto _ = mDevice.waitForFences(mRenderFence, true, UINT64_MAX);
    //Reset the fence
    mDevice.resetFences(mRenderFence);

    //reset the command buffer
    uint32_t lastCmdIdx = mRTRenderCmdIndex == 0 ? 1 : 0;
    mRTRenderCmd[lastCmdIdx].reset(vk::CommandBufferResetFlagBits::eReleaseResources);
}

void Application::Present(vk::CommandBuffer commandBuffer)
{

    vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;



    auto qSubmitInfo = vk::SubmitInfo()
        .setPWaitDstStageMask(&waitStage)
        .setCommandBufferCount(1)
        .setPCommandBuffers(&commandBuffer)
        .setWaitSemaphoreCount(1)
        .setPWaitSemaphores(&mRenderSemaphore)
        .setSignalSemaphoreCount(1)
        .setPSignalSemaphores(&mPresentSemaphore);

    auto _ = mQueues.GraphicsQueue.submit(1, &qSubmitInfo, mRenderFence);

    auto presentInfo = vk::PresentInfoKHR()
        .setWaitSemaphoreCount(1)
        .setPWaitSemaphores(&mPresentSemaphore)
        .setSwapchainCount(1)
        .setPSwapchains(&mSwapchainStructs.SwapchainHandle)
        .setPImageIndices(&mCurrentSwapchainImage);
    
    auto result = mQueues.GraphicsQueue.presentKHR(presentInfo);
    

    //new image index
    mRTRenderCmdIndex = mRTRenderCmdIndex == 0 ? 1 : 0;

}

void Application::CreateBaseResources()
{
    // Create an image to render to
    auto imageCreateInfo = vk::ImageCreateInfo()
        .setImageType(vk::ImageType::e2D)
        .setFormat(vk::Format::eR8G8B8A8Unorm)
        .setExtent(vk::Extent3D(mSwapchainStructs.SwapchainExtent, 1))
        .setMipLevels(1)
        .setArrayLayers(1)
        .setSamples(vk::SampleCountFlagBits::e1)
        .setTiling(vk::ImageTiling::eOptimal)
        .setUsage(vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc)
        .setSharingMode(vk::SharingMode::eExclusive)
        .setInitialLayout(vk::ImageLayout::eUndefined);
    
    // create the image with dedicated memory
    mOutputImage = mVRDev->CreateImage(imageCreateInfo, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);

    // create a view for the image
    auto viewCreateInfo = vk::ImageViewCreateInfo()
        .setImage(mOutputImage.Image)
        .setViewType(vk::ImageViewType::e2D)
        .setFormat(vk::Format::eR8G8B8A8Unorm)
        .setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

    mOutputImageView = mDevice.createImageView(viewCreateInfo);

    // create a uniform buffer
    mCameraUniformBuffer = mVRDev->CreateBuffer(
        sizeof(float) * 4 * 4 * 2, // two 4x4 matrix
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, // we will be writing to this buffer on the CPU
        vk::BufferUsageFlagBits::eUniformBuffer); // its a uniform buffer
}


void Application::Run()
{
    while (!glfwWindowShouldClose(mWindow))
    {
        BeginFrame();
        Update(mRTRenderCmd[mRTRenderCmdIndex]);
        glfwPollEvents();
    }
}

Application::~Application()
{
    if(mCameraUniformBuffer.Buffer)
        mVRDev->DestroyBuffer(mCameraUniformBuffer);
    if(mOutputImage.Image)
        mVRDev->DestroyImage(mOutputImage);

    //Clean up
    delete mVRDev;

    glfwDestroyWindow(mWindow);
    glfwTerminate();

    mDevice.destroyImageView(mOutputImageView);


    mDevice.destroyFence(mRenderFence);
    mDevice.destroySemaphore(mRenderSemaphore);
    mDevice.destroySemaphore(mPresentSemaphore);
    mDevice.destroyCommandPool(mGraphicsPool);
    
    vr::SwapchainBuilder::DestroySwapchain(mDevice, mSwapchainStructs);
    mDevice.destroy();
    mInstance.InstanceHandle.destroySurfaceKHR(mSurface);

    vr::InstanceWrapper::DestroyInstance(mInstance);
}