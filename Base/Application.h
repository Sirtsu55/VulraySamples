#pragma once
#include <GLFW/glfw3.h>
class Application
{
public:
	Application();
	virtual ~Application();

	void Run();

	//Functions to be overriden by the samples
	virtual void Start();

	virtual void Update(vk::CommandBuffer renderCmd);
	
	virtual void Stop();

	void BeginFrame();

	void WaitForRendering();

	void Present(vk::CommandBuffer commandBuffer);

	void CreateStoreImage();

protected:

	GLFWwindow* mWindow = nullptr;
	vk::Device mDevice = nullptr;
	vk::SurfaceKHR mSurface = nullptr;
	vr::InstanceWrapper mInstance;
	vk::PhysicalDevice mPhysicalDevice = nullptr;
	vr::CommandQueues mQueues;

	vk::CommandPool mGraphicsPool;

	vr::ImageAllocation mOutputImage;
    vk::ImageView mOutputImageView = nullptr;

	vr::SwapchainStructs mSwapchainStructs;
	uint32_t mMaxFramesInFlight = 0;

	uint32_t mWidth = 1280;
	uint32_t mHeight = 720;

	uint32_t mCurrentSwapchainImage = 0;

	vk::Semaphore mRenderSemaphore;
	vk::Semaphore mPresentSemaphore;
	vk::Fence mRenderFence;

	vr::VulrayDevice* mVRDev = nullptr;

    std::vector<vk::CommandBuffer> mRTRenderCmd; // one recording and one pending
    uint32_t mRTRenderCmdIndex = 0;
};
	