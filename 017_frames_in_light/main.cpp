#include <stdlib.h>
#include <cstdint>
#include <set>
#include <spdlog/common.h>
#include <stdexcept>
#include <vulkan/vulkan_core.h>
#ifdef USING_GLFW
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#endif

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <optional>

#ifdef USING_XCB
#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xproto.h>
#include <vulkan/vulkan_xcb.h>
#endif /* USING_XCB */

// for log print
#include <spdlog/spdlog.h>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const int MAX_FRAMES_IN_FLIGHT = 2;

const std::vector<const char *> validationLayers = {
    "VK_LAYER_KHRONOS_validation", // debug, logging and validate
    //"VK_LAYER_LUNARG_gfxreconstruct" // recording draw command for replay
    //"VK_LAYER_RENDERDOC_Capture"    // renderdoc added
};

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
                                      VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
                                      const VkAllocationCallbacks *pAllocator,
                                      VkDebugUtilsMessengerEXT *pDebugMessenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else
    {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance,
                                   VkDebugUtilsMessengerEXT debugMessenger,
                                   const VkAllocationCallbacks *pAllocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        spdlog::trace("{} call vkDestroyDebugUtilsMessengerEXT", __func__);
        func(instance, debugMessenger, pAllocator);
    }
}

struct QueueFamilyIndices
{
    std::optional<uint32_t> mGraphicsFamily;
    std::optional<uint32_t> mPresentFamily;

    bool isComplete()
    {
        return mGraphicsFamily.has_value() && mPresentFamily.has_value();
    }
};


struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR mCapabilities;
    std::vector<VkSurfaceFormatKHR> mFormats;
    std::vector<VkPresentModeKHR> mPresentModes;
};

class HelloTriangleApplication
{
public:
    void run()
    {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
#ifdef USING_GLFW
    GLFWwindow *mWindow;
#endif /* USING_GLFW */

#ifdef USING_XCB
    xcb_connection_t*   mXCBConnection;
    xcb_screen_t*       mXCBScreen;
    xcb_window_t       mXCBWindow;
#endif /* USING_XCB */

    VkInstance mInstance;
    VkDebugUtilsMessengerEXT mDebugMessenger;
    VkSurfaceKHR mSurface;

    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    VkDevice mDevice;
    VkQueue mGraphicsQueue;
    VkQueue mPresentQueue;

    // swap chain
    VkSwapchainKHR mSwapChain;
    std::vector<VkImage> mSwapChainImages;
    VkFormat mSwapChainImageFormat;
    VkExtent2D mSwapChainExtent;

    // image view
    std::vector<VkImageView> mSwapChainImageViews;

    // render pass
    VkRenderPass mRenderPass;
    // Pipeline layout
    VkPipelineLayout mPipelineLayout;

    // graphic pipeline
    VkPipeline mGraphicsPipeline;

    // Framebuffers
    std::vector<VkFramebuffer> mSwapChainFramebuffers;

    // command pools
    VkCommandPool mCommandPool;
    // command buffer allocation
    std::vector<VkCommandBuffer> mCommandBuffers;

    // synchronization object
    // use semaphore for gpu sync, and use fence for sync between gpu and cpu
    std::vector<VkSemaphore> mImageAvailableSemaphores;
    std::vector<VkSemaphore> mRenderFinishedSemaphores;
    std::vector<VkFence> mInFlightFences;

    // 用来记录使用的 frame
    uint32_t mCurrentFrame = 0;

    void initWindow()
    {
#ifdef USING_GLFW
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        mWindow = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
        if (mWindow == nullptr) {
            spdlog::error("{} glfwCreateWindow failed", __func__);
            throw std::runtime_error("glfwCreateWindow failed");
        }
#endif 

#ifdef USING_XCB
        mXCBConnection = xcb_connect(nullptr, nullptr);
        if (xcb_connection_has_error(mXCBConnection)) {
            spdlog::error("Can not connect to X Server!");
            throw std::runtime_error("Can not connect to X Server.");
        }

        const xcb_setup_t* setup = xcb_get_setup(mXCBConnection);
        mXCBScreen = xcb_setup_roots_iterator(setup).data;

        uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
        uint32_t values[2] = {
            mXCBScreen->black_pixel,
            XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS
        };

        mXCBWindow = xcb_generate_id(mXCBConnection);
        xcb_create_window(
            mXCBConnection,
            0,
            mXCBWindow,
            mXCBScreen->root,
            0, 0, WIDTH, HEIGHT,
            0,
            0,
            mXCBScreen->root_visual,
            mask, values
        );

        xcb_change_property(mXCBConnection,
            XCB_PROP_MODE_REPLACE, mXCBWindow,
            XCB_ATOM_WM_NAME, XCB_ATOM_STRING,
            8, strlen("Vulkan Demo"), "Vulkan Demo"
        );

        xcb_map_window(mXCBConnection, mXCBWindow);
        xcb_flush(mXCBConnection);
#endif /* USING_XCB */
    }

    void initVulkan()
    {
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createRenderPass();
        createGraphicPipeline();
        createFrameBuffers();
        createCommandPool();
        createCommandBuffers();
        createSyncObjects();
    }

    void createSurface() {
#ifdef USING_GLFW
        if (glfwCreateWindowSurface(mInstance, mWindow, nullptr, &mSurface) != VK_SUCCESS) {
            spdlog::error("{} glfwCreateWindowSurface failed", __func__);
            throw  std::runtime_error("failed to create window surface!");
        }
#endif /* USING_GLFW */

#ifdef USING_XCB
        VkXcbSurfaceCreateInfoKHR createInfo = {
            .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
            .connection = mXCBConnection,
            .window = mXCBWindow
        };

        VkResult err = vkCreateXcbSurfaceKHR(mInstance, &createInfo, nullptr, &mSurface);
        if (err) {
            spdlog::error("Can not create VkSurface!");
            std::runtime_error("Can not create VkSurface!");
        }
#endif  /* USING_XCB */
    }

    void mainLoop()
    {
#ifdef USING_GLFW
        while (!glfwWindowShouldClose(mWindow))
        {
            glfwPollEvents();
            drawFrame();
        }
#endif /* USING_GLFW */

#ifdef USING_XCB
        bool isRunning = true;
        xcb_generic_event_t* event;
        while (isRunning) {
            while ((event = xcb_poll_for_event(mXCBConnection))) {
                switch(event->response_type & ~0x80) {
                    case XCB_EXPOSE:
                        drawFrame();
                        break;

                    case XCB_KEY_PRESS:
                        xcb_key_press_event_t* key = (xcb_key_press_event_t*) event;
                        if (key->detail == 9) {
                            isRunning = false;
                        }
                        break;
                }
                free(event);
            }

            drawFrame();
        }
#endif /* USING_XCB */

        vkDeviceWaitIdle(mDevice);
    }

    void cleanup()
    {
        spdlog::info("{}", __func__);

        cleanupSwapChain();

        // destory all the sync objects
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(mDevice, mRenderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(mDevice, mImageAvailableSemaphores[i], nullptr);
            vkDestroyFence(mDevice, mInFlightFences[i], nullptr);
        }

        vkDestroyCommandPool(mDevice, mCommandPool, nullptr);

        //for (auto& framebuffer : mSwapChainFramebuffers) {
        //    vkDestroyFramebuffer(mDevice, framebuffer, nullptr);
        //}

        vkDestroyPipeline(mDevice, mGraphicsPipeline, nullptr);
        vkDestroyPipelineLayout(mDevice, mPipelineLayout, nullptr);
        vkDestroyRenderPass(mDevice, mRenderPass, nullptr);

        //for (auto& imageView : mSwapChainImageViews) {
        //    vkDestroyImageView(mDevice, imageView, nullptr);
        //}

        //vkDestroySwapchainKHR(mDevice, mSwapChain, nullptr);
        
        vkDestroyDevice(mDevice, nullptr);

        if (enableValidationLayers)
        {
            DestroyDebugUtilsMessengerEXT(mInstance, mDebugMessenger, nullptr);
        }

        vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
        vkDestroyInstance(mInstance, nullptr);
#ifdef USING_GLFW
        glfwDestroyWindow(mWindow);
        glfwTerminate();
#endif /* USING_GLFW */

#ifdef USING_XCB
        xcb_destroy_window(mXCBConnection, mXCBWindow);
        xcb_disconnect(mXCBConnection);
#endif /* USING_XCB */
    }

    void drawFrame() {
        vkWaitForFences(mDevice, 1, &mInFlightFences[mCurrentFrame], VK_TRUE, UINT64_MAX);
        vkResetFences(mDevice, 1, &mInFlightFences[mCurrentFrame]);

        uint32_t imageIndex;
        vkAcquireNextImageKHR(
            mDevice,
            mSwapChain,
            UINT64_MAX,
            mImageAvailableSemaphores[mCurrentFrame],
            VK_NULL_HANDLE, &imageIndex);
        vkResetCommandBuffer(mCommandBuffers[mCurrentFrame], 0);
        recordCommandBuffer(mCommandBuffers[mCurrentFrame], imageIndex);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = {mImageAvailableSemaphores[mCurrentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &mCommandBuffers[mCurrentFrame];

        VkSemaphore signalSemaphores[] = {mRenderFinishedSemaphores[mCurrentFrame]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;
        if (vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, mInFlightFences[mCurrentFrame]) != VK_SUCCESS) {
            spdlog::error("{} failed to submit draw command buffer", __func__);
            throw std::runtime_error("failed to submit draw command buffer");
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = {mSwapChain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;

        presentInfo.pImageIndices = &imageIndex;

        vkQueuePresentKHR(mPresentQueue, &presentInfo);

        // change current frame
        mCurrentFrame = (mCurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void createInstance()
    {
        if (enableValidationLayers && !checkValidationLayerSupport())
        {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Frame In Light";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        std::vector<const char*> extensions = getRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (enableValidationLayers)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();

            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT *)&debugCreateInfo;
        }
        else
        {
            createInfo.enabledLayerCount = 0;
            createInfo.pNext = nullptr;
        }

        if (vkCreateInstance(&createInfo, nullptr, &mInstance) != VK_SUCCESS)
        {
            spdlog::error("{} failed to create vulkan instance", __func__);
            throw std::runtime_error("failed to create instance!");
        }
        spdlog::trace("{}: create vulkan instance successful", __func__);
    }

    void createLogicalDevice() {
        QueueFamilyIndices indices = findQueueFamilies(mPhysicalDevice);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

        std::set<uint32_t> uniqueQueueFamilies = {
            indices.mGraphicsFamily.value(),
            indices.mPresentFamily.value()
        };
        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures{};
        VkDeviceCreateInfo createInfo{};    // Logical Device Create Info

        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();

        createInfo.pEnabledFeatures = &deviceFeatures;

        // extension
        // 使用交换链需要首先启用 VK_KHR_swapchain 扩展
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        } else {
            createInfo.enabledLayerCount = 0;
        }

        if (vkCreateDevice(mPhysicalDevice, &createInfo, nullptr, &mDevice) != VK_SUCCESS) {
            spdlog::error("{} create logical device error", __func__);
            throw std::runtime_error("failed to create logical device");
        }

        vkGetDeviceQueue(mDevice, indices.mGraphicsFamily.value(), 0, &mGraphicsQueue);
        vkGetDeviceQueue(mDevice, indices.mPresentFamily.value(), 0, &mPresentQueue);
    }

    void createSwapChain() {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(mPhysicalDevice);

        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.mFormats);
        // 呈现模式，它决定了如何将画面送往屏幕
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.mPresentModes);
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.mCapabilities);

        // 我们必须决定交换链中需要多少图像。 实现过程中会指定其运行所需的最小数量
        // 但是，如果仅仅坚持这一最小值，就意味着我们有时可能需要等待驱动程序完成内部操作，
        // 然后才能获取另一张图像进行渲染。 因此，我们建议至少比最小值多请求一个图像
        uint32_t imageCount = swapChainSupport.mCapabilities.minImageCount + 1;
        if (swapChainSupport.mCapabilities.maxImageCount > 0 &&
            imageCount > swapChainSupport.mCapabilities.maxImageCount) {
            // 我们还应该确保在执行此操作时，图像数量不超过最大值，其中 0 是一个特殊值，表示没有最大值。
            imageCount = swapChainSupport.mCapabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = mSurface;

        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        // imageArrayLayers 指定了每个图像的层数。 除非开发的是立体 3D 应用程序，否则该值始终为 1
        createInfo.imageArrayLayers = 1;
        // imageUsage 位字段指定我们将交换链中的图像用于何种操作
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        QueueFamilyIndices indices = findQueueFamilies(mPhysicalDevice);
        uint32_t queueFamilyIndices[] = {
            indices.mGraphicsFamily.value(),
            indices.mPresentFamily.value()
        };
        // Next, we need to specify how to handle swap chain images that will be 
        // used across multiple queue families. That will be the case in our application 
        // if the graphics queue family is different from the presentation queue.
        // We'll be drawing on the images in the swap chain from the graphics queue
        // and then submitting them on the presentation queue.
        // There are two ways to handle images that are accessed from multiple queues:
        if (indices.mGraphicsFamily != indices.mPresentFamily) {
            spdlog::trace("{} iamgeSharingMode = {}", __func__, "VK_SHARING_MODE_CONCURRENT");
            // 图像可在多个队列系列中使用，无需明确的所有权转让。
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            spdlog::trace("{} iamgeSharingMode = {}", __func__, "VK_SHARING_MODE_EXCLUSIVE");
            // 图像每次由一个队列族拥有，在另一个队列族中使用之前必须明确转移所有权。 此选项可提供最佳性能。
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        // 如果交换链中的图像支持某种变换，我们可以指定将该变换应用于交换链中的图像，例如顺时针旋转 90 度或水平翻转
        // 要指定不需要任何变换，只需指定当前变换即可。
        createInfo.preTransform = swapChainSupport.mCapabilities.currentTransform;
        // compositeAlpha 字段指定是否使用 alpha 通道与窗口系统中的其他窗口进行混合
        // 您几乎总是希望忽略 alpha 通道，因此需要使用 VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;

        createInfo.oldSwapchain = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(mDevice, &createInfo, nullptr, &mSwapChain) != VK_SUCCESS) {
            spdlog::error("{} failed to create swap chain", __func__);
            throw std::runtime_error("failed to create swap chain");
        }
        vkGetSwapchainImagesKHR(mDevice, mSwapChain, &imageCount, nullptr);
        spdlog::trace("{} swap chain image count = {}", __func__, imageCount);
        mSwapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(mDevice, mSwapChain, &imageCount, mSwapChainImages.data());

        mSwapChainImageFormat = surfaceFormat.format;
        mSwapChainExtent = extent;
    }

    void recreateSwapChain() {
        //我们首先调用 vkDeviceWaitIdle，因为就像上一章一样，我们不应该触碰可能仍在使用的资源。
        //显然，我们必须重新创建交换链本身。 图像视图需要重新创建，因为它们直接基于交换链图像。
        //最后，帧缓冲区直接依赖于交换链图像，因此也必须重新创建。
        vkDeviceWaitIdle(mDevice);

        cleanupSwapChain();

        createSwapChain();
        createImageViews();
        createFrameBuffers();
    }

    void cleanupSwapChain() {
        for (int i = 0; i < mSwapChainFramebuffers.size(); i++) {
            vkDestroyFramebuffer(mDevice, mSwapChainFramebuffers[i], nullptr);
        }

        for (int i = 0; i < mSwapChainImageViews.size(); i++) {
            vkDestroyImageView(mDevice, mSwapChainImageViews[i], nullptr);
        }

        vkDestroySwapchainKHR(mDevice, mSwapChain, nullptr);
    }

    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo)
    {
#if 1
        spdlog::trace("{}", __func__);
#endif
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;
    }

    void setupDebugMessenger()
    {
        if (!enableValidationLayers)
            return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        populateDebugMessengerCreateInfo(createInfo);

        if (CreateDebugUtilsMessengerEXT(mInstance, &createInfo, nullptr, &mDebugMessenger) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }

    void pickPhysicalDevice()
    {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(mInstance, &deviceCount, nullptr);
        if (deviceCount == 0)
        {
            spdlog::error("{} does not find any physical gpu devices under pc", __func__);
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }

        spdlog::trace("{}: find {} physical devices", __func__, deviceCount);

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(mInstance, &deviceCount, devices.data());

        for (const auto &device : devices)
        {
            if (isDeviceSuitable(device))
            {
                mPhysicalDevice = device;
                break;
            }
        }

        if (mPhysicalDevice == VK_NULL_HANDLE)
        {
            spdlog::error("{} can not find a gpu device can do graphic work", __func__);
            throw std::runtime_error("failed to find a suitable GPU!");
        }
    }

    bool isDeviceSuitable(VkPhysicalDevice pDevice)
    {
#if 0
        // print device properities and features
        VkPhysicalDeviceProperties deviceProperties;
        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceProperties(pDevice, &deviceProperties);
        vkGetPhysicalDeviceFeatures(pDevice, &deviceFeatures);

        spdlog::trace("for device: {}", deviceProperties.deviceName);
        spdlog::trace("    apiVersion: {}.{}.{}",
            VK_VERSION_MAJOR(deviceProperties.apiVersion),
            VK_VERSION_MINOR(deviceProperties.apiVersion),
            VK_VERSION_PATCH(deviceProperties.apiVersion)
        );
        spdlog::trace("    driverVersion: {}.{}.{}",
            VK_VERSION_MAJOR(deviceProperties.driverVersion),
            VK_VERSION_MINOR(deviceProperties.driverVersion),
            VK_VERSION_PATCH(deviceProperties.driverVersion)
        );
        spdlog::trace("    vendorID: {:x}", deviceProperties.vendorID);
        spdlog::trace("    deviceID: {:x}", deviceProperties.deviceID);
#endif
        QueueFamilyIndices indices = findQueueFamilies(pDevice);
        bool extensionsSupported = checkDeviceExtensionSupport(pDevice);

        bool swapChainAdequate = false;
        if (extensionsSupported) {
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(pDevice);
            swapChainAdequate = !swapChainSupport.mFormats.empty() && 
                !swapChainSupport.mPresentModes.empty();
        }
        return indices.isComplete() && extensionsSupported && swapChainAdequate;
    }

    bool checkDeviceExtensionSupport(VkPhysicalDevice pDevice) {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(pDevice, nullptr, &extensionCount, nullptr);
        
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(pDevice, nullptr, &extensionCount, availableExtensions.data());
#if 0
        spdlog::trace("{} enumerate {} device extension properties", __func__, extensionCount);
        for (auto& extension : availableExtensions) {
            spdlog::trace("{}    property: {}", __func__, extension.extensionName);
        }
#endif

        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        for (const auto& extension : availableExtensions) {
            spdlog::trace("{}: device support extension {}", __func__, extension.extensionName);
            requiredExtensions.erase(extension.extensionName);
        }
        // 如果我们希望的 device extension 当前的设备都支持的话，那么这里就会返回空
        return requiredExtensions.empty();
    }

    /**
    前面已经简单介绍过，Vulkan 中的几乎所有操作，从绘图到上传纹理，都需要向队列提交命令。
    不同类型的队列来自不同的队列系列，每个队列系列只允许一部分命令。
    例如，可能有一个队列系列只允许处理计算命令，或者有一个队列系列只允许处理与内存传输相关的命令。
    */
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice pDevice)
    {
        QueueFamilyIndices indices;
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(pDevice, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(pDevice, &queueFamilyCount, queueFamilies.data());
        spdlog::trace("{} get device queue family counts {}", __func__, queueFamilyCount);

        int i = 0;
        for (const auto &queueFamily : queueFamilies)
        {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                // spdlog formatter output format: {parameter index:number format}
                // 即 {参数位置:进制}
                spdlog::trace("{} queueFlags: {:x}", __func__, queueFamily.queueFlags);
                indices.mGraphicsFamily = i;
            }

            VkBool32 presentSupport = false;
            // 接下来，我们将修改 findQueueFamilies 函数，以查找能够显示窗口表面的队列系列。
            // 用于检查的函数是 vkGetPhysicalDeviceSurfaceSupportKHR，它需要物理设备、队列族索引和表面作为参数。 
            // 在与 VK_QUEUE_GRAPHICS_BIT 相同的循环中添加对它的调用
            vkGetPhysicalDeviceSurfaceSupportKHR(pDevice, i, mSurface, &presentSupport);

            if (presentSupport) {
                indices.mPresentFamily = i;
            }

            if (indices.isComplete()) {
                break;
            }
            i++;
        }
        return indices;
    }

    std::vector<const char *> getRequiredExtensions()
    {
#ifdef USING_GLFW
        uint32_t glfwExtensionCount = 0;
        const char **glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
#endif /* USING_GLFW */

#ifdef USING_XCB
        std::vector<const char *> extensions {
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_KHR_XCB_SURFACE_EXTENSION_NAME
        };
#endif /* USING_XCB */

#if 1
        for (const char* extension : extensions) {
            spdlog::trace("{} glfwExtension: {}", __func__, extension);
        }
#endif
        if (enableValidationLayers)
        {
            // 要在程序中设置一个回调来处理消息和相关细节，我们必须使用 VK_EXT_debug_utils 扩展设置一个带有回调的调试
            // GLFW 指定的扩展总是必需的，但调试信使扩展是有条件添加的。
            // 请注意，我在这里使用了 VK_EXT_DEBUG_UTILS_EXTENSION_NAME 宏，
            // 它等于字面字符串 "VK_EXT_debug_utils"。 使用这个宏可以避免输入错误。
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        return extensions;
    }

    bool checkValidationLayerSupport()
    {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
#if 1
        // print layers name and it;s description
        // should be the same as vulkaninfo's output
        for (int i = 0; i < layerCount; ++i) {
            spdlog::debug("{} layers[{}]: {}", __func__, i, availableLayers[i].layerName);
            spdlog::debug("{}       description:{}", __func__, availableLayers[i].description);
        }
#endif
        for (const char *layerName : validationLayers)
        {
            bool layerFound = false;
            for (const auto &layerProperties : availableLayers)
            {
                if (strcmp(layerName, layerProperties.layerName) == 0)
                {
                    spdlog::trace("{} find layer [{}]", __func__, layerName);
                    layerFound = true;
                    break;
                }
            }
            if (!layerFound)
            {   
                spdlog::warn("{} can not find layer [{}]", __func__, layerName);
                return false;
            }
        }
        return true;
    }

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice pDevice) {
        SwapChainSupportDetails details;
        //This function takes the specified VkPhysicalDevice and VkSurfaceKHR window surface into 
        // account when determining the supported capabilities. 
        // All of the support querying functions have these two as first parameters because they 
        // are the core components of the swap chain.
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pDevice, mSurface, &details.mCapabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(pDevice, mSurface, &formatCount, nullptr);

        spdlog::trace("{} physical device format count {}", __func__, formatCount);
        if (formatCount != 0) {
            details.mFormats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(pDevice, 
                mSurface,
                &formatCount, 
                details.mFormats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(pDevice, 
            mSurface,
            &presentModeCount,
            nullptr);

        spdlog::trace("{} physical device present mode count {}", __func__, presentModeCount);
        if (presentModeCount != 0) {
            details.mPresentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(pDevice,
                mSurface,
                &presentModeCount,
                details.mPresentModes.data());
        }

        return details;
    }

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(
        const std::vector<VkSurfaceFormatKHR>& availableFormats) {
        for (const auto& availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
                availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                spdlog::trace("{}: find available format with RGBA8888 format and" 
                    " sRGB nonlinear color space", __func__);
                return availableFormat;
            }
        }

        return availableFormats[0];
    }

    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
        for (const auto& availablePresentMode : availablePresentModes) {
            // 不会在交换链满的时候，去阻塞应用程序，队列中的图像会直接被替换为程序新提交的图像
            if  (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                spdlog::trace("{}: choose {}", __func__, "VK_PRESENT_MODE_MAILBOX_KHR");
                return availablePresentMode;
            }
        }
        spdlog::trace("{}: use {} for default", __func__, "VK_PRESENT_MODE_FIFO_KHR");
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    /**
     * 设置交换范围，它是交换链中的图像的分辨率，它几乎总是和我们要显示的窗口大小相同
    */
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        } else {
            int width, height;
#ifdef USING_GLFW
            glfwGetFramebufferSize(mWindow, &width, &height);
            spdlog::trace("{} glfwGetFrameBufferSize [{}x{}]", __func__, width, height);
#endif /* USING_GLFW */

#ifdef USING_XCB
            xcb_get_geometry_cookie_t cookie = xcb_get_geometry(mXCBConnection, mXCBWindow);
            xcb_get_geometry_reply_t *reply = xcb_get_geometry_reply(mXCBConnection, cookie, NULL);
            if (reply) {
                width = reply->width;   // 窗口宽度
                height = reply->height; // 窗口高度
                spdlog::debug("XCB Window size: {} x {}", width, height);
                free(reply);
            }
#endif /* USING_XCB */

            VkExtent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };

            actualExtent.width = std::clamp(
                actualExtent.width, 
                capabilities.minImageExtent.width, 
                capabilities.maxImageExtent.width
            );
            actualExtent.height = std::clamp(
                actualExtent.height, 
                capabilities.minImageExtent.height, 
                capabilities.maxImageExtent.height
            );

            return actualExtent;
        }
    }

    void createImageViews() {
        mSwapChainImageViews.resize(mSwapChainImages.size());
        for(int i = 0; i < mSwapChainImages.size(); ++i) {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = mSwapChainImages[i];
            // The viewType and format fields specify how the image data
            // should be interpreted. The viewType parameter allows you to 
            // treat images as 1D textures, 2D textures, 3D textures and cube maps
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = mSwapChainImageFormat;

            // 通过 "component "字段，您可以随意改变颜色通道。
            // 例如，您可以将单色纹理的所有通道映射到红色通道。
            // 您也可以将 0 和 1 的常值映射到一个通道。 
            // 在本例中，我们将使用默认映射。
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            // subresourceRange 字段描述了图像的用途以及应访问图像的哪个部分。
            // 我们的图像将作为色彩目标使用，不需要任何映射层或多图层
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            // 如果您正在开发一个立体 3D 应用程序，那么您将创建一个包含多个图层的交换链。
            // 然后，您可以通过访问不同的图层，为每个图像创建多个图像视图，分别代表左眼和右眼的视图
            if (vkCreateImageView(mDevice, &createInfo, nullptr, &mSwapChainImageViews[i]) 
                != VK_SUCCESS) {
                spdlog::error("{} failed to create image view for VkImage with index {}",
                     __func__, i);
                throw std::runtime_error("failed to create image view");
            }
        }
    }

    void createRenderPass() {
        VkAttachmentDescription colorAttachment{};
        // attachment 的格式，例如 VK_FORMAT_B8G8R8A8_SRGB
        colorAttachment.format = mSwapChainImageFormat;
        // 多重采用，这里表示不使用多重采样
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        // 在 render pass 开始的时候，如何处理 attachment 的内容
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        // 在 render pass 结束的时候，如何处理 attachment 的内容
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        // 在 render pass 开始的时候，如何处理模板缓冲的内容
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        // 在 render pass 结束的时候，如何处理模板缓冲的内容
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        // attachments 在 render pass 开始之前的 layout
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        // attachments 在 render pass 结束后的 layout
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        // subpasses and attachment reference
        VkAttachmentReference colorAttachmentRef{};
        // layout(location = 0) out vec4 outColor;
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
 
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        VkSubpassDependency dependency{};
        // 渲染通道之外
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = VK_PIPELINE_STAGE_NONE;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(mDevice, &renderPassInfo, nullptr, &mRenderPass) != VK_SUCCESS) {
            spdlog::error("{} failed to create render pass", __func__);
            throw std::runtime_error("failed to create render pass");
        }
    }

    void createGraphicPipeline() {
        auto vertShaderCode = readFile("shader/vert.spv");
        auto fragShaderCode = readFile("shader/frag.spv");

        VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
        VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

        // Vertex Shader setting
        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";

        // Fragment Shader setting
        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = {
            vertShaderStageInfo, 
            fragShaderStageInfo
        };

        //Vertex state 需要加载顶点数据的时候，使用，这里由于所有的顶点数据写在了 glsl 中，无需指定
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.pVertexBindingDescriptions = nullptr;
        vertexInputInfo.vertexAttributeDescriptionCount = 0;
        // 类似于 opengl 中的 glVertexAttribPointer() 函数的功能？
        vertexInputInfo.pVertexAttributeDescriptions = nullptr;

        // input assembly
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        // 每 3 个顶点绘制一个三角形，不重复使用三角形
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        // 在使用索引绘制时，图元重启允许使用特定的索引值来表示一个图元的结束和下一个图元的开始
        inputAssembly.primitiveRestartEnable = VK_FALSE;
#if 0
        // viewport and scissor
        VkViewport viewPort{};
        viewPort.x = 0.0f;
        viewPort.y = 0.0f;
        viewPort.width = static_cast<float>(mSwapChainExtent.width);
        viewPort.height = static_cast<float>(mSwapChainExtent.height);
        viewPort.minDepth = 0.0f;
        viewPort.maxDepth = 0.0f;

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = mSwapChainExtent;
#endif

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        //viewportState.pViewports = &viewPort;
        viewportState.scissorCount = 1;
        //viewportState.pScissors = &scissor;

        // Rasterizer
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        // 完全填充多边形
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        // 线宽
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        // 顺时针方向排列
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        // 是否开启深度偏移，对片段的深度值应用一个偏移值
        rasterizer.depthBiasEnable = VK_FALSE;
        // 剔除所有的光栅化的结果
        //rasterizer.rasterizerDiscardEnable = VK_TRUE;
        //rasterizer.depthBiasConstantFactor = 0.0f; // Optional
        //rasterizer.depthBiasClamp = 0.0f; // Optional
        //rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

        // Multisampling
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        //multisampling.minSampleShading = 1.0f;
        //multisampling.pSampleMask = nullptr;
        //multisampling.alphaToCoverageEnable = VK_FALSE;
        //multisampling.alphaToOneEnable = VK_FALSE;

        // Color blending
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
            VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;
        //colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        //colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;    // Optional
        //colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;    // Optional
        //colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        //colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;    // Optional
        //colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;    // Optional

        // color blending
        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f;
        colorBlending.blendConstants[1] = 0.0f;
        colorBlending.blendConstants[2] = 0.0f;
        colorBlending.blendConstants[3] = 0.0f;

        // dynamic states
        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        // Pipeline layout
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0;
        pipelineLayoutInfo.pSetLayouts = nullptr;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;

        if (vkCreatePipelineLayout(mDevice, &pipelineLayoutInfo, nullptr, &mPipelineLayout)
            != VK_SUCCESS) {
            spdlog::error("{} failed to create pipeline layout", __func__);
            throw std::runtime_error("failed to create pipeline layout");
        }

        // create graphic pipeline
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        // 我们只定义了顶点着色器和片段着色器这两个阶段
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        //  这里给出顶点着色器输入的一些信息
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = nullptr;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = mPipelineLayout;
        pipelineInfo.renderPass = mRenderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = -1;

        if (vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &pipelineInfo,
            nullptr, &mGraphicsPipeline) != VK_SUCCESS) {
            spdlog::error("{} failed to create graphics pipeline", __func__);
            throw std::runtime_error("failed to create graphics pipeline");
        }

        // destroy the shader module, we do not need to keep it
        vkDestroyShaderModule(mDevice, fragShaderModule, nullptr);
        vkDestroyShaderModule(mDevice, vertShaderModule, nullptr);
    }

    void createFrameBuffers() {
        mSwapChainFramebuffers.resize(mSwapChainImageViews.size());
        for (unsigned int i = 0; i < mSwapChainImageViews.size(); ++i) {
            VkImageView attachments[] = {
                mSwapChainImageViews[i]
            };
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = mRenderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments  = attachments;
            framebufferInfo.width = mSwapChainExtent.width;
            framebufferInfo.height = mSwapChainExtent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(mDevice, &framebufferInfo, nullptr,
                &mSwapChainFramebuffers[i]) != VK_SUCCESS) {
                spdlog::error("{} failed to create framebuffer for imageView "
                    "with index {}", __func__, i);
                throw std::runtime_error("failed to create framebuffer");
            }
        }
    }

    void createSyncObjects() {
        // resize vector size
        mImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        mRenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        mInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            if (vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mImageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mRenderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(mDevice, &fenceInfo, nullptr, &mInFlightFences[i]) != VK_SUCCESS
            ) {
                spdlog::error("{}:{} failed to create synchronization objects", __func__, i);
                throw std::runtime_error("failed to create synchronization objects!");
            }
        }
    }

    void createCommandPool() {
        QueueFamilyIndices queueFamilyIndices = findQueueFamilies(mPhysicalDevice);
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndices.mGraphicsFamily.value();

        if (vkCreateCommandPool(mDevice, &poolInfo, nullptr, &mCommandPool) != VK_SUCCESS) {
            spdlog::error("{} failed to create command pool", __func__);
            throw std::runtime_error("failed to create command pool");
        }
    }

    void createCommandBuffers() {
        mCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = mCommandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = static_cast<uint32_t>(mCommandBuffers.size());

        if (vkAllocateCommandBuffers(mDevice, &allocInfo, mCommandBuffers.data()) != VK_SUCCESS) {
            spdlog::error("{} failed to allocate command buffers!", __func__);
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        //beginInfo.flags = 0;
        //beginInfo.pInheritanceInfo = nullptr;

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            spdlog::error("{} failed to begin recording command buffer");
            throw std::runtime_error("failed to begin recording command buffer");
        }

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = mRenderPass;
        renderPassInfo.framebuffer = mSwapChainFramebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = mSwapChainExtent;

#ifndef USE_SELF_DEFINED_CLEAR_COLOR
        VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
#else
        VkClearValue clearColor = {{{0.82f, 0.82f, 0.82f, 1.0f}}};
#endif
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mGraphicsPipeline);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(mSwapChainExtent.width);
        viewport.height = static_cast<float>(mSwapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = {mSwapChainExtent.width, mSwapChainExtent.height};
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        // ready to issue the draw command for the triangle
        // three vertices and draw one triangle
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);

        vkCmdEndRenderPass(commandBuffer);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            spdlog::error("{} failed to record command buffer", __func__);
            throw std::runtime_error("failed to record command buffer");
        }
    }

    // 二进制的 SPIR-V 代码需要转化为 VkShaderModule 对象
    VkShaderModule createShaderModule(const std::vector<char>& code) {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
        createInfo.pNext = nullptr;

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(mDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            spdlog::error("{} failed to create shader module", __func__);
            throw std::runtime_error("failed to create shader module");
        }

        return shaderModule;
    }

    static std::vector<char> readFile(const std::string& fileName) {
        std::ifstream file(fileName, std::ios::ate | std::ios::binary);
        
        if (!file.is_open()) {
            spdlog::error("{} failed to open file {}", __func__, fileName);
            throw std::runtime_error("failed to open file");
        }

        uint32_t fileSize = file.tellg();
        std::vector<char> buffer(fileSize);
        spdlog::trace("{} read {} bytes from file {}", __func__, fileSize, fileName);
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();

        return buffer;
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                        VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                                        void *pUserData)
    {
#if 0
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
#else
        if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
            spdlog::error("[{}]: layer: {}", __func__, pCallbackData->pMessage);
        } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
            spdlog::warn("[{}]: layer: {}", __func__, pCallbackData->pMessage);
        } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
            spdlog::info("[{}]: layer: {}", __func__, pCallbackData->pMessage);
        } else {
            spdlog::trace("[{}]: layer: {}", __func__, pCallbackData->pMessage);
        }
#endif
        return VK_FALSE;
    }
};

int main(int argc, char *argv[])
{
#ifdef LOG_DEBUG
    // set log print level
    spdlog::set_level(spdlog::level::trace);
#else
    spdlog::set_level(spdlog::level::info);
#endif
    HelloTriangleApplication app;
    try
    {
        app.run();
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
