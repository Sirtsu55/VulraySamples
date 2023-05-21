#pragma once
#include <GLFW/glfw3.h>
#include "SimpleTimer.h"
#include "Camera.h"

class Application
{
public:
	Application();
	virtual ~Application();

	void BlitImage(vk::CommandBuffer renderCmd);

	void Run();

	//Functions to be overriden by the samples
	virtual void Start();

	virtual void Update(vk::CommandBuffer renderCmd);
	
	virtual void Stop();

	void BeginFrame();

	void WaitForRendering();

	void Present(vk::CommandBuffer commandBuffer);

	void CreateBaseResources();

	void UpdateCamera();

private:
	void HandleResize();


protected:

	GLFWwindow* mWindow = nullptr;
	vk::Device mDevice = nullptr;
	vk::SurfaceKHR mSurface = nullptr;
	vr::InstanceWrapper mInstance;
	vk::PhysicalDevice mPhysicalDevice = nullptr;
	vr::CommandQueues mQueues;


	vk::CommandPool mGraphicsPool;

	vr::AllocatedImage mOutputImageBuffer;
    vr::AccessibleImage mOutputImage;

	vr::SwapchainBuilder mSwapchainBuilder;
	vr::SwapchainStructs mSwapchainStructs;
	vk::SwapchainKHR mOldSwapchain = nullptr;
	
	uint32_t mMaxFramesInFlight = 0;

	uint32_t mWindowWidth = 1280;
	uint32_t mWindowHeight = 720;

	uint32_t mRenderWidth = 1280;
	uint32_t mRenderHeight = 720;

	uint32_t mCurrentSwapchainImage = 0;

	vk::Semaphore mRenderSemaphore;
	vk::Semaphore mPresentSemaphore;
	vk::Fence mRenderFence;

	vr::VulrayDevice* mVRDev = nullptr;

    std::vector<vk::CommandBuffer> mRTRenderCmd; // one recording and one pending
    uint32_t mRTRenderCmdIndex = 0;


	vr::AllocatedBuffer mUniformBuffer = {};

	Camera mCamera;

	glm::dvec2 mMousePos = { 0.0f, 0.0f };
	glm::dvec2 mMouseDelta = { 0.0f, 0.0f };

	float DeltaTime = 0.0f;

	uint64_t mFrameCount = 0;
	uint32_t mPassiveFrameCount = 0;

	
	SimpleTimer mFrameTimer;
};
	