#include <stdlib.h>
#include <cstdint>
#include <set>
#include <spdlog/common.h>
#include <stdexcept>
#include <vulkan/vulkan_core.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// for transpose
//  GLM_FORCE_RADIANS定义对于确保像glm::rotate这样的函数使用弧度作为参数, 而不是使用角度作为参数
#define GLM_FORCE_RADIANS
// GLM生成的透视投影矩阵默认使用OpenGL深度范围-1.0到1.0
// 我们需要使用GLM_FORCE_DEPTH_ZERO_TO_ONE定义将其配置为使用0.0到1.0的 Vulkan 范围
#ifndef GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#endif

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <vector>
#include <array>
#include <string>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <cstddef>
#include <optional>
#include <chrono>
#include <unordered_map>
// using std::hash function
#include <functional>

// for log print
#include <spdlog/spdlog.h>

// for matrix and vertex
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
// hash.hpp will provide some struct's hash functional
#include <glm/gtx/hash.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "tiny_obj_loader.cc"

#include "Verterx.h"
#include "Shape.h"

// 对于需要在 std::unordered_map 中使用的类，还需要提供一个 std::hash 的类特化函数用于在 std::unordered_map 中计算 hash 值
namespace std {
    template<> struct hash<ops::Vertex> {
        size_t operator()(ops::Vertex const& vertex) const {
            return (
                (hash<glm::vec3>()(vertex.mPos) ^
                (hash<glm::vec3>()(vertex.mColor) << 1)) >> 1) ^
                (hash<glm::vec2>()(vertex.mTexCoord) << 1);
        }
    };
}

struct UniformBufferObject {
    glm::mat4 mModel;
    glm::mat4 mView;
    glm::mat4 mProj;
};

// 用来更新选择纹理
struct UBOIndex {
    int u_samplerIndex;
};

const uint32_t WIDTH = 1200;
const uint32_t HEIGHT = 900;

// https://skfb.ly/VAKF
const std::string MODEL_PATH = "./models/house.obj";
const std::string MTL_PATH   = "./models/house.mtl";

const int MAX_FRAMES_IN_FLIGHT = 2;

const std::vector<const char *> validationLayers = {
    "VK_LAYER_KHRONOS_validation", // debug, logging and validate
    //"VK_LAYER_LUNARG_gfxreconstruct" // recording draw command for replay
    //"VK_LAYER_RENDERDOC_Capture"    // renderdoc added
};

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#ifdef BUG_FIXES
    VK_KHR_SEPARATE_DEPTH_STENCIL_LAYOUTS_EXTENSION_NAME,
    VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
    VK_KHR_MULTIVIEW_EXTENSION_NAME,
    "VK_KHR_maintenance2",
#endif /* BUG_FIXES */
};

const std::vector<const char*> instanceExtensions = {
#ifdef BUG_FIXES
    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
#endif /* BUG_FIXES */
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
    GLFWwindow *mWindow;

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
    // descriptor set layout
    VkDescriptorSetLayout mDescriptorSetLayout;
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

    // vertex buffer
    // load model
    std::vector<ops::Vertex> mVertices;
    std::vector<uint32_t> mIndices;
    // 需要根据形状去解析
    std::vector<ops::Shape_Mesh> mMeshes;
    VkBuffer mVertexBuffer;
    VkDeviceMemory mVertexBufferMemory;
    // index buffer
    VkBuffer mIndexBuffer;
    VkDeviceMemory mIndexBufferMemory;

    // uniform buffer UniformBufferObject
    // Todo: 这个需要改名字
    std::vector<VkBuffer> mUniformBuffers;  // 在 cpu 侧的句柄
    std::vector<VkDeviceMemory> mUniformBuffersMemory;  // 实际要分配的 gpu 内存
    std::vector<void*> mUniformBuffersMapped;   // gpu 内存在cpu 侧的 map

    // uniform buffer UBOIndex
    // 这个用来选择纹理的下标
    std::vector<VkBuffer> mUBOIndexBuffers;
    std::vector<VkDeviceMemory> mUBOIndexBuffersMemory;
    std::vector<void*> mUBOIndexBuffersMapped;

    // descriptor pool
    VkDescriptorPool mDescriptorPool;
    std::vector<VkDescriptorSet> mDescriptorSets;

    // synchronization object
    // use semaphore for gpu sync, and use fence for sync between gpu and cpu
    std::vector<VkSemaphore> mImageAvailableSemaphores;
    std::vector<VkSemaphore> mRenderFinishedSemaphores;
    std::vector<VkFence> mInFlightFences;

    // frames in flight
    uint32_t mCurrentFrame = 0;

    // for glfw window size changed
    bool mFrameBufferResized = false;

    // texture image
    VkImage mTextureImage;
    VkDeviceMemory mTextureImageMemory;
    VkImageView mTextureImageView;
    VkSampler mTextureSampler;

    // 我们要同时使用多个纹理图片
    // 我们需要一个从纹理的名称到其下标的映射
    std::unordered_map<std::string, int> mTexName2IndexMap;
    std::vector<VkImage> mTextureImages;
    std::vector<VkDeviceMemory> mTextureImagesMemory;
    std::vector<VkImageView> mTextureImagesView;
    std::vector<VkDescriptorImageInfo> mTextureImagesInfo;
    // 我们对于多个纹理，可以使用同一个 sampler?
    // Todo: 能否只使用一个 sampler

    // depth image
    VkImage mDepthImage;
    VkDeviceMemory mDepthImageMemory;
    VkImageView mDepthImageView;

    // tiny obj instance
    tinyobj::ObjReader mObjReaderInstance;

    // 按键输入用来控制相机的位置
    const glm::vec3 front = glm::vec3(0.0f, 0.0f, -1.0f);
    const glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 mCameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
    float mCameraDeltaTime = 0.0f;
    float mCameraLastRenderTime = 0.0f;
    float mCameraSpeedFactor = 2.0f;

    void initWindow()
    {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        mWindow = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
        if (mWindow == nullptr) {
            spdlog::error("{} glfwCreateWindow failed", __func__);
            throw std::runtime_error("glfwCreateWindow failed");
        }

        glfwSetWindowUserPointer(mWindow, this);
        glfwSetFramebufferSizeCallback(mWindow, frameBufferResizedCallback);
    }

    static void frameBufferResizedCallback(GLFWwindow* window, int width, int height) {
        spdlog::debug("{}: window changed to [{}x{}]", __func__, width, height);
        auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
        app->mFrameBufferResized = true;
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
        // descriptor
        createDescriptorSetLayout();
        createGraphicPipeline();
        createCommandPool();
        // create depth image and depth image views
        createDepthResources();
        // move create frame buffers after create depth resources
        createFrameBuffers();
        // load model obj
        loadModel();
        // generate the texture image
        createTextureImages();
        // Todo: 这里需要重新修改
        //createTextureImage();
        // Todo: 这里需要重新修改
        //createTextureImageView();
        // Todo: 这里需要重新的修改
        // creata image sampler
        createTextureSampler();
        // create vertex buffer and map it to gpu mem after create Command pool
        createVertexBuffer();
        createIndexBuffer();
        // create uniform buffer and map it to gpu mem
        createUniformBuffers();
        // descriptor pool
        createDescriptorPool();
        createDescriptorSets();
        createCommandBuffers();
        createSyncObjects();
    }

    void createSurface() {
        if (glfwCreateWindowSurface(mInstance, mWindow, nullptr, &mSurface) != VK_SUCCESS) {
            spdlog::error("{} glfwCreateWindowSurface failed", __func__);
            throw  std::runtime_error("failed to create window surface!");
        }
    }

    void mainLoop()
    {
        while (!glfwWindowShouldClose(mWindow))
        {
            glfwPollEvents();
            drawFrame();
        }

        vkDeviceWaitIdle(mDevice);
    }

    void cleanup()
    {
        spdlog::info("{}", __func__);

        cleanupSwapChain();

        // vertex buffer is not dependent on the swap chain, we need clean it by my self
        // the buffer should be available for use in rendering commands until the end of 
        // program
        vkDestroyBuffer(mDevice, mVertexBuffer, nullptr);
        // 就像 C++ 中的动态内存分配一样，内存应该在某个时候被释放
        // 一旦缓冲区不再使用，绑定到缓冲区对象的内存可能会被释放，因此让我们在缓冲区被销毁后释放它
        vkFreeMemory(mDevice, mVertexBufferMemory, nullptr);

        // 回收 uniform buffer 的内存 rotate matrix and project matrix
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            vkDestroyBuffer(mDevice, mUniformBuffers[i], nullptr);
            vkFreeMemory(mDevice, mUniformBuffersMemory[i], nullptr);
        }

        // 回收 uniform buffer 的内存， texture index
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroyBuffer(mDevice, mUBOIndexBuffers[i], nullptr);
            vkFreeMemory(mDevice, mUBOIndexBuffersMemory[i], nullptr);
        }

        // destory descriptor pool
        vkDestroyDescriptorPool(mDevice, mDescriptorPool, nullptr);

        // destroy sampler
        vkDestroySampler(mDevice, mTextureSampler, nullptr);
        // destroy image view
        for (auto& vkImageView : mTextureImagesView) {
            vkDestroyImageView(mDevice, vkImageView, nullptr);
        }

        // clean the texture image
        for (auto& vkImage : mTextureImages) {
            vkDestroyImage(mDevice, vkImage, nullptr);
        }

        for (auto& vkImageMemory : mTextureImagesMemory) {
            vkFreeMemory(mDevice, vkImageMemory, nullptr);
        }

        // destory descriptor set layout
        vkDestroyDescriptorSetLayout(mDevice, mDescriptorSetLayout, nullptr);

        // destory index buffer
        vkDestroyBuffer(mDevice, mIndexBuffer, nullptr);
        vkFreeMemory(mDevice, mIndexBufferMemory, nullptr);

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            vkDestroySemaphore(mDevice, mRenderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(mDevice, mImageAvailableSemaphores[i], nullptr);
            vkDestroyFence(mDevice, mInFlightFences[i], nullptr);
        }

        vkDestroyCommandPool(mDevice, mCommandPool, nullptr);

        vkDestroyPipeline(mDevice, mGraphicsPipeline, nullptr);
        vkDestroyPipelineLayout(mDevice, mPipelineLayout, nullptr);
        vkDestroyRenderPass(mDevice, mRenderPass, nullptr);

        vkDestroyDevice(mDevice, nullptr);

        if (enableValidationLayers)
        {
            DestroyDebugUtilsMessengerEXT(mInstance, mDebugMessenger, nullptr);
        }

        vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
        vkDestroyInstance(mInstance, nullptr);

        glfwDestroyWindow(mWindow);
        glfwTerminate();
    }

    void drawFrame() {
        vkWaitForFences(mDevice, 1, &mInFlightFences[mCurrentFrame], VK_TRUE, UINT64_MAX);
        vkResetFences(mDevice, 1, &mInFlightFences[mCurrentFrame]);

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(
            mDevice,
            mSwapChain,
            UINT64_MAX,
            mImageAvailableSemaphores[mCurrentFrame],
            VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            spdlog::debug("{}: vk error out of data khr", __func__);
            recreateSwapChain();
            return;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            spdlog::error("{}: vkAcquireNextImageKHR", __func__);
            throw std::runtime_error("failed to acquire swap images");
        }

        // update uniform buffer
        updateUniformBuffer(mCurrentFrame);

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

        result = vkQueuePresentKHR(mPresentQueue, &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || mFrameBufferResized) {
            spdlog::debug("{}: recreateSwapChain", __func__);
            mFrameBufferResized = false;
            recreateSwapChain();
        } else if (result != VK_SUCCESS) {
            spdlog::error("{}: vkQeueuPresentKHR error", __func__);
            throw std::runtime_error("failed to present swap chain image!");
        }

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
        appInfo.pApplicationName = "Vertex Input Description";
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
        // 如果要使用各项异性过滤，需要手动的去请求它
        deviceFeatures.samplerAnisotropy = VK_TRUE;
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
            spdlog::trace("{} imageSharingMode = {}", __func__, "VK_SHARING_MODE_CONCURRENT");
            // 图像可在多个队列系列中使用，无需明确的所有权转让。
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            spdlog::trace("{} imageSharingMode = {}", __func__, "VK_SHARING_MODE_EXCLUSIVE");
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
        spdlog::debug("{} swap chain image count = {}", __func__, imageCount);
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
        createDepthResources();
        createFrameBuffers();
    }

    void cleanupSwapChain() {
        spdlog::debug("{}", __func__);

        vkDestroyImageView(mDevice, mDepthImageView, nullptr);
        vkDestroyImage(mDevice, mDepthImage, nullptr);
        vkFreeMemory(mDevice, mDepthImageMemory, nullptr);

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

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &memProperties);

        for (int32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
            if ((typeFilter & (1 << i)) &&
                ((memProperties.memoryTypes[i].propertyFlags & properties) == properties)) {
                spdlog::info("{}: return memory type with index {}", __func__, i);
                return i;
            }
        }

        spdlog::error("{}: failed to find suitable memory type", __func__);
        throw std::runtime_error("failed to find suitable memory type!");
    }

    bool isDeviceSuitable(VkPhysicalDevice pDevice)
    {
#if 1
        // print device properities and features
        VkPhysicalDeviceProperties deviceProperties;
        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceProperties(pDevice, &deviceProperties);
        vkGetPhysicalDeviceFeatures(pDevice, &deviceFeatures);

        spdlog::debug("for device: {}", deviceProperties.deviceName);
        spdlog::debug("    apiVersion: {}.{}.{}",
            VK_VERSION_MAJOR(deviceProperties.apiVersion),
            VK_VERSION_MINOR(deviceProperties.apiVersion),
            VK_VERSION_PATCH(deviceProperties.apiVersion)
        );
        spdlog::debug("    driverVersion: {}.{}.{}",
            VK_VERSION_MAJOR(deviceProperties.driverVersion),
            VK_VERSION_MINOR(deviceProperties.driverVersion),
            VK_VERSION_PATCH(deviceProperties.driverVersion)
        );
        spdlog::debug("    vendorID: {:x}", deviceProperties.vendorID);
        spdlog::debug("    deviceID: {:x}", deviceProperties.deviceID);
#endif
        QueueFamilyIndices indices = findQueueFamilies(pDevice);
        bool extensionsSupported = checkDeviceExtensionSupport(pDevice);

        bool swapChainAdequate = false;
        if (extensionsSupported) {
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(pDevice);
            swapChainAdequate = !swapChainSupport.mFormats.empty() && 
                !swapChainSupport.mPresentModes.empty();
        }

        // 检查显卡的各项异性过滤的支持
        VkPhysicalDeviceFeatures supportedFeatures{};
        vkGetPhysicalDeviceFeatures(pDevice, &supportedFeatures);
        return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
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
        uint32_t glfwExtensionCount = 0;
        const char **glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
#if 1
        for (const char* extension : extensions) {
            spdlog::info("{} glfwExtension: {}", __func__, extension);
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
#ifdef BUG_FIXES
        for (const char* extension : instanceExtensions) {
            spdlog::info("{} add instance extension: \"{}\"", __func__, extension);
            extensions.push_back(extension);
        }
#endif /* BUG_FIXES */
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
                spdlog::debug("{} find available format with RGBA8888 format and" 
                    "sRGB nonlinear color space", __func__);
                return availableFormat;
            }
        }

        return availableFormats[0];
    }

    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
        for (const auto& availablePresentMode : availablePresentModes) {
            // 不会在交换链满的时候，去阻塞应用程序，队列中的图像会直接被替换为程序新提交的图像
            if  (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                spdlog::trace("{} choose {}", __func__, "VK_PRESENT_MODE_MAILBOX_KHR");
                return availablePresentMode;
            }
        }
        spdlog::trace("{} use {} for default", __func__, "VK_PRESENT_MODE_FIFO_KHR");
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    /**
     * 设置交换范围，它是交换链中的图像的分辨率，它几乎总是和我们要显示的窗口大小相同
    */
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            spdlog::debug("{} return extent with size [{}x{}]",
                __func__,
                capabilities.currentExtent.width,
                capabilities.currentExtent.height
            );
            return capabilities.currentExtent;
        } else {
            int width, height;
            glfwGetFramebufferSize(mWindow, &width, &height);
            spdlog::info("{} glfwGetFrameBufferSize [{}x{}]", __func__, width, height);

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
            // move most of the same part to function createImageView
            mSwapChainImageViews[i] = createImageView(
                mSwapChainImages[i],
                mSwapChainImageFormat,
                VK_IMAGE_ASPECT_COLOR_BIT
            );
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

        // depth attachment
        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = findDepthFormat();
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // subpasses and attachment reference
        VkAttachmentReference colorAttachmentRef{};
        // layout(location = 0) out vec4 outColor;
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;

        VkSubpassDependency dependency{};
        // 渲染通道之外
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        // we have color attachment and depth attachment
        std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
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

        //Vertex state 需要加载顶点数据的时候使用
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        VkVertexInputBindingDescription bindDescription = ops::Vertex::getBindingDescription();
        // 4: position, color, texcoord, normal
        std::array<VkVertexInputAttributeDescription, 5> attributeDescription = ops::Vertex::getAttributeDescription();

        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<unsigned int>(attributeDescription.size());
        vertexInputInfo.pVertexBindingDescriptions = &bindDescription;
        // 类似于 opengl 中的 glVertexAttribPointer() 函数的功能？
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescription.data();

        // input assembly
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        // 每 3 个顶点绘制一个三角形，不重复使用三角形
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        // 在使用索引绘制时，图元重启允许使用特定的索引值来表示一个图元的结束和下一个图元的开始
        inputAssembly.primitiveRestartEnable = VK_FALSE;

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
        // 逆时针方向
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
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
        // 通常，在 Vulkan 中，你需要在创建图形管线时预先定义许多渲染状态（如视口、剪裁区域、光栅化设置等），
        // 这些状态一旦设置，除非重新创建管线，否则无法改变。然而，为了避免频繁地重新创建管线，
        // Vulkan 提供了动态状态机制，允许某些状态在命令缓冲区记录期间进行动态修改。
        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,  // 视口状态可以动态修改
            VK_DYNAMIC_STATE_SCISSOR    // 剪裁区域可以动态修改
        };

        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        // depth stencil
        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        // 字段指定为保留或丢弃片段而执行的比较。我们坚持较低深度 = 更接近的约定，因此新片段的深度应该更小。
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        // 保留只在特定范围内的片段
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.minDepthBounds = 0.0f;
        depthStencil.maxDepthBounds = 1.0f;
        depthStencil.stencilTestEnable = VK_FALSE;
        depthStencil.front = {};
        depthStencil.back = {};

        // Pipeline layout
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        // 我们创建了一个 descriptorsetlayout, 绑定了一个 ubo, 在创建 pipeline 的时候，需要进行设置
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &mDescriptorSetLayout;
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
        pipelineInfo.pDepthStencilState = &depthStencil;

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
            std::array<VkImageView, 2> attachments = {
                mSwapChainImageViews[i],
                mDepthImageView
            };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = mRenderPass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments  = attachments.data();
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
        // resize the sync objects size
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
        // resize the command buffers
        mCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = mCommandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = static_cast<uint32_t>(mCommandBuffers.size());

        if (vkAllocateCommandBuffers(mDevice, &allocInfo, mCommandBuffers.data()) != VK_SUCCESS) {
            spdlog::error("{} failed to allocate command buffers", __func__);
            throw std::runtime_error("failed to allocate command buffers");
        }
    }

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properities,
        VkBuffer& buffer,
        VkDeviceMemory& bufferMemory) {
        // VkBuffer 是一个逻辑上的概念，他表示一段连续的内存数据，但是他不实际包含数据，而是描述数据的大小，用途等信息
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        // 和 image chains 中的 image 相同，buffers 也可以被一个特定的 queue 占用，也可以在多个
        // queues 之间进行共享。在这里这个 buffer 只在 graphics queue 中使用
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(mDevice, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            spdlog::error("{}: failed to create buffer!", __func__);
            throw std::runtime_error("failed to create buffer!");
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(mDevice, buffer, &memRequirements);
        spdlog::debug("{}: memRequirements: {}, {}, {}",
            __func__,
            memRequirements.size, 
            memRequirements.alignment,
            memRequirements.memoryTypeBits
        );

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properities);
        // 实际分配gpu缓冲区
        if (vkAllocateMemory(mDevice, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            spdlog::error("{}: failed to allocate buffer memory!", __func__);
            throw std::runtime_error("failed to allocate buffer memory!");
        }
        // 将 VkBuffer 绑定到 DeviceMemory 的一段位置，需要指定偏移量的大小
        // 多个 buffer 可以绑定到同一个 DeviceMemory 上，但是具有不同的偏移地址
        vkBindBufferMemory(mDevice, buffer, bufferMemory, 0);
    }

    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        endSingleTimeCommands(commandBuffer);
    }

    void createVertexBuffer() {
        VkDeviceSize bufferSize = sizeof(mVertices[0]) * mVertices.size();
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingBuffer,
            stagingBufferMemory
        );

        void* data;
        // 将 gpu 专用内存映射到 cpu 可访问内存中来
        vkMapMemory(mDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, mVertices.data(), static_cast<size_t>(bufferSize));
        vkUnmapMemory(mDevice, stagingBufferMemory);

        createBuffer(bufferSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            mVertexBuffer,
            mVertexBufferMemory
        );

        copyBuffer(stagingBuffer, mVertexBuffer, bufferSize);
        vkDestroyBuffer(mDevice, stagingBuffer, nullptr);
        vkFreeMemory(mDevice, stagingBufferMemory, nullptr);
    }

    void  createIndexBuffer() {
        VkDeviceSize bufferSize = sizeof(mIndices[0]) * mIndices.size();

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(bufferSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingBuffer,
            stagingBufferMemory
        );

        void* data;
        vkMapMemory(mDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, mIndices.data(), static_cast<size_t>(bufferSize));
        vkUnmapMemory(mDevice, stagingBufferMemory);

        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mIndexBuffer, mIndexBufferMemory);

        copyBuffer(stagingBuffer, mIndexBuffer, bufferSize);
        vkDestroyBuffer(mDevice, stagingBuffer, nullptr);
        vkFreeMemory(mDevice, stagingBufferMemory, nullptr);
    }

    void createUniformBuffers() {
        // 坐标变换矩阵
        VkDeviceSize bufferSize = sizeof(UniformBufferObject);

        mUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        mUniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
        mUniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                mUniformBuffers[i], mUniformBuffersMemory[i]
            );

            // 我们在创建后立即使用vkMapMemory映射缓冲区，以获取稍后可以写入数据的指针。
            // 在应用程序的整个生命周期中，缓冲区始终映射到该指针。该技术称为“持久映射” ，适用于所有 Vulkan 实现。
            // 不必每次需要更新缓冲区时都映射缓冲区，从而提高性能，因为映射是有开销的
            vkMapMemory(mDevice, mUniformBuffersMemory[i], 0, bufferSize, 0, &mUniformBuffersMapped[i]);
        }

        // 纹理下标选择
        bufferSize = sizeof(UBOIndex);
        mUBOIndexBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        mUBOIndexBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
        mUBOIndexBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                mUBOIndexBuffers[i], mUBOIndexBuffersMemory[i]
            );

            // 映射内存
            vkMapMemory(mDevice, mUBOIndexBuffersMemory[i], 0, bufferSize, 0, &mUBOIndexBuffersMapped[i]);
        }
    }

    void createDescriptorPool() {
        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        // Todo: 这里存在一些问题，我们在创建 Descriptor Pool 的时候，需要告知 vulkan 我们需要多少个 descriptor set
        // 这里我们创建了两个 Uniform buffer object
        poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * 2);
        // 我们还创建了 3 个 texture
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * mTextureImages.size());

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        if (vkCreateDescriptorPool(mDevice, &poolInfo, nullptr, &mDescriptorPool) != VK_SUCCESS) {
            spdlog::error("{}: failed to create descriptor pool!", __func__);
            throw std::runtime_error("failed to create descriptor pool!");
        }
    }

    void createDescriptorSets() {
        // 现在我们有三个 Descriptor 需要做分配
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, mDescriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo{};  
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = mDescriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        allocInfo.pSetLayouts = layouts.data();

        mDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
        if (vkAllocateDescriptorSets(mDevice, &allocInfo, mDescriptorSets.data()) != VK_SUCCESS) {
            spdlog::error("{}: failed to allocate descriptor sets!", __func__);
            throw std::runtime_error("failed to allocate descriptor sets!");
        }

        // 现在描述符集已分配完毕，但其中的描述符仍需要配置。我们现在将添加一个循环来填充每个描述符
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            // 视角
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = mUniformBuffers[i];
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(UniformBufferObject);

            // 纹理选择
            VkDescriptorBufferInfo uboIndexBufferInfo{};
            uboIndexBufferInfo.buffer = mUBOIndexBuffers[i];
            uboIndexBufferInfo.offset = 0;
            uboIndexBufferInfo.range = sizeof(UBOIndex);

            // Todo: 对于创建了纹理数组的情况下，如何设置 Descriptor
            std::vector<VkDescriptorImageInfo> imagesInfo{};
            int imagesNums = mTextureImages.size();
            imagesInfo.resize(imagesNums);
            for (int i = 0; i < imagesNums; i++) {
                imagesInfo[i].imageView = mTextureImagesView[i];
                // Todo: 所有的 texture 是否可以共用一个 sampler
                imagesInfo[i].sampler = mTextureSampler;
                imagesInfo[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }

            std::array<VkWriteDescriptorSet, 3> descriptorWrites{};
            // ubo for rotate matrix and project matrix
            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = mDescriptorSets[i];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pBufferInfo = &bufferInfo;
            descriptorWrites[0].pImageInfo = nullptr;   // optional
            descriptorWrites[0].pTexelBufferView = nullptr; // optional
            // for texture images
            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = mDescriptorSets[i];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[1].descriptorCount = imagesInfo.size();
            descriptorWrites[1].pImageInfo = imagesInfo.data();
            // for texture iamges
            descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[2].dstSet = mDescriptorSets[i];
            descriptorWrites[2].dstBinding = 2;
            descriptorWrites[2].dstArrayElement = 0;
            descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites[2].descriptorCount = 1;
            descriptorWrites[2].pBufferInfo = &uboIndexBufferInfo;
            descriptorWrites[2].pImageInfo = nullptr;
            descriptorWrites[2].pTexelBufferView = nullptr;

            vkUpdateDescriptorSets(mDevice,
                static_cast<uint32_t>(descriptorWrites.size()), 
                descriptorWrites.data(), 0, nullptr
            );
        }
    }

    void updateUniformBuffer(uint32_t currentImage) {
        static auto startTime = std::chrono::high_resolution_clock::now();

        // 更新 camera 位置
        processInput(mWindow);

        UniformBufferObject ubo{};
        ubo.mModel = glm::rotate(glm::mat4(1.0f), glm::radians(70.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        //ubo.mModel = glm::rotate(ubo.mModel, timeDiff * glm::radians(22.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.mView = glm::lookAt(
            mCameraPos, // eyes
            mCameraPos + front, // center
            up  // up
        );
        ubo.mProj = glm::perspective(glm::radians(45.0f),
            mSwapChainExtent.width / static_cast<float>(mSwapChainExtent.height),
            0.1f,
            100.0f
        );
        // GLM最初是为OpenGL设计的，其中剪辑坐标的Y坐标是倒置的。
        // 补偿这一问题的最简单方法是翻转投影矩阵中 Y 轴缩放因子的符号。
        // 如果不这样做，则图像将呈现颠倒状态
        ubo.mProj[1][1] *= -1;  // Y

        // 将更新之后的 ubo 写入到映射的内存中
        memcpy(mUniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
    }

    void updateTextureIndex(uint32_t currentImage ,uint32_t textureIndex) {
        UBOIndex index;
        index.u_samplerIndex = textureIndex;

        memcpy(mUBOIndexBuffersMapped[currentImage], &index, sizeof(UBOIndex));
    }

    void createDescriptorSetLayout() {
        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        // 对应 vertex shader 中的 layout(binding = 0) uniform UniformBufferObject
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.descriptorCount = 1;
        // 在哪个着色器阶段使用
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        uboLayoutBinding.pImmutableSamplers = nullptr; //optional

        // sampler layout binding
        // 对应 fragment shader 中的 layout(binding = 1) uniform sampler2D texSampler[3];
        VkDescriptorSetLayoutBinding samplerLayoutBinding{};
        samplerLayoutBinding.binding = 1;
        samplerLayoutBinding.descriptorCount = 3;   // 现在需要存储三个纹理，这里设置为 3
        samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding.pImmutableSamplers = nullptr;
        samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        // 通过 index 去选择特定的 texture sampler
        VkDescriptorSetLayoutBinding samplerIndexLayoutBinding{};
        samplerIndexLayoutBinding.binding = 2;
        samplerIndexLayoutBinding.descriptorCount = 1;
        samplerIndexLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        samplerIndexLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array<VkDescriptorSetLayoutBinding, 3> bindings = {
            uboLayoutBinding,
            samplerLayoutBinding,
            samplerIndexLayoutBinding
        };
        // 所有的描述符的绑定，都需要组合到一个 VkDescriptorSetLayout 对象上面
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(mDevice, &layoutInfo, nullptr, &mDescriptorSetLayout) != VK_SUCCESS) {
            spdlog::error("{}: failed to create descriptor set layout!", __func__);
            throw std::runtime_error("failed to create descriptor set layout!");
        }
    }

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

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

        std::array<VkClearValue, 2> clearValues{};
#ifndef USE_SELF_DEFINED_CLEAR_COLOR
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
#else
        clearValues[0].color = {{0.102f, 0.102f, 0.102f, 1.0f}};
#endif /* USE_SELF_DEFINED_CLEAR_COLOR */
        clearValues[1].depthStencil = {1.0f, 0};

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

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

        for (auto& mesh : mMeshes) {
            auto& meshIndices = mesh.mIndices;
            // 需要更新 uboIndex
            spdlog::trace("{} update texture index: {}, {}", __func__, imageIndex, mesh.mMeterial_ID);
            // 由于这个 ubo 必须和 mesh 的绘制同步，所以只能通过 command buffer 的方式更新，而不使用内存映射的方式更新
            // 这会带来性能的损失
            // Todo: 但是 vkCmdUpdateBuffer() 操作必须放在 RenderPass 开始之前，所以这里这样使用也不行
            UBOIndex index;
            index.u_samplerIndex = mesh.mMeterial_ID;
            //updateTextureIndex(mCurrentFrame, mesh.mMeterial_ID);
            // vertex buffer
            VkBuffer vertexBuffers[] = {mVertexBuffer};
            VkDeviceSize offsets[] = {0};
            VkDeviceSize indicesOffsets = mesh.mOffset * sizeof(uint32_t);
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(commandBuffer, mIndexBuffer, indicesOffsets, VK_INDEX_TYPE_UINT32);
            vkCmdBindDescriptorSets(commandBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                mPipelineLayout,
                0,
                1,
                &mDescriptorSets[mCurrentFrame],
                0,
                nullptr
            );

            // ready to issue the draw command for the triangle
            // three vertices and draw one triangle
            //vkCmdDraw(commandBuffer, static_cast<uint32_t>(vertices.size()), 1, 0, 0);
            // Todo: 对于每一个 Shape (单独的模型) 我们都会使用不同的纹理，使用不同的 indices.
            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(meshIndices.size()), 1, 0, 0, 0);
        }

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

    // Todo: 根据解析的模型中的materials 来创建所有的纹理
    void createTextureImages() {
        const std::vector<tinyobj::material_t>& materials = mObjReaderInstance.GetMaterials();
        // 先只考虑漫反射的纹理，diffuse
        int index = 0;
        for (auto& material : materials) {
            std::string materialName = material.name;
            std::string diffuseTexName = material.diffuse_texname;
            if (!diffuseTexName.empty()) {
                // Todo: 加载纹理
                VkImage vkImage;
                VkDeviceMemory vkImageMemory;
                VkImageView vkImageView;
                createTextureImage(diffuseTexName, vkImage, vkImageMemory);
                mTextureImages.push_back(vkImage);
                mTextureImagesMemory.push_back(vkImageMemory);
                spdlog::debug("{} texture {} with index {}", __func__, diffuseTexName, index);
                mTexName2IndexMap.insert(std::make_pair<>(diffuseTexName, index++));
                vkImageView = createTextureImageView(vkImage);
                mTextureImagesView.push_back(vkImageView);
            }
        }
    }

    void createTextureImage(const std::string texturePath, VkImage& vkImage, VkDeviceMemory& vkImageMemory) {
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load(texturePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        VkDeviceSize imageSize = texWidth * texHeight * 4;
        spdlog::info("{} load image file {}, dims = [{}x{}], size = {}",
            __func__,
            texturePath.c_str(),
            texWidth, texHeight,
            imageSize
        );

        if (!pixels) {
            spdlog::debug("{} failed to load texture image!", __func__);
            throw std::runtime_error("failed to load texture image!");
        }
        spdlog::info("{} load image with [{}x{}x{}], image size is {}",
            __func__, texWidth, texHeight, texChannels, imageSize);

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        void* data;
        vkMapMemory(mDevice, stagingBufferMemory, 0, imageSize, 0, &data);
        // 将图片的数据拷贝到 VkBuffer 中
        memcpy(data, pixels, static_cast<size_t>(imageSize));
        vkUnmapMemory(mDevice, stagingBufferMemory);

        stbi_image_free(pixels);
#ifndef BUG_FIXES
        createImage(texWidth, texHeight,
            VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vkImage, vkImageMemory
        );
#else
        // VUID-VkImageViewCreateInfo-None-02273
        createImage(texWidth, texHeight,
            VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_LINEAR,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vkImage, vkImageMemory
        );
#endif /* BUG_FIXES */
        // 该图像是使用VK_IMAGE_LAYOUT_UNDEFINED布局创建的，因此在转换textureImage时应将其指定为旧布局。
        // 请记住，我们可以这样做，因为在执行复制操作之前我们不关心其内容
        // 未定义 → 传输目的地
        transitionImageLayout(vkImage,
            VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        );
        // 从临时的 buffer 拷贝到 VkImage 中
        copyBufferToImage(stagingBuffer,
            vkImage,
            static_cast<uint32_t>(texWidth),
            static_cast<uint32_t>(texHeight)
        );
        // 传输目的地→着色器读取
        transitionImageLayout(vkImage,
            VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );
        // 销毁临时的 buffer
        vkDestroyBuffer(mDevice, stagingBuffer, nullptr);
        vkFreeMemory(mDevice, stagingBufferMemory, nullptr);
    }

    VkImageView createTextureImageView(const VkImage& vkImage) {
        return createImageView(vkImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
    }

    void createTextureSampler() {
        // 限制可用于计算最终颜色的纹素样本数量。值越低，性能越好，但质量结果越差。
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(mPhysicalDevice, &properties);
        spdlog::info("{} gpu maxSamplerAnisotropy is {}", __func__, properties.limits.maxSamplerAnisotropy);

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        // axes are called U,V,W instand of X,Y,Z
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        // 各项异性过滤
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
        samplerInfo.borderColor =  VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        // 指定要使用哪个坐标系来寻址图像中的纹素。如果此字段为VK_TRUE，则​​您可以简单地使用[0, texWidth)和[0, texHeight)范围内的坐标。
        // 如果为VK_FALSE ，则使用所有轴上的[0, 1)范围对纹素进行寻址。
        // 现实世界的应用程序几乎总是使用标准化坐标，因为这样就可以使用具有完全相同坐标的不同分辨率的纹理
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        // mipmap 相关，这里目前不使用
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        // 采用器不和任何特定的 Image 绑定，它提供了从纹理中提取颜色的接口，它可以用于任何图像
        if (vkCreateSampler(mDevice, &samplerInfo, nullptr, &mTextureSampler) != VK_SUCCESS) {
            spdlog::error("{} failed to create texture sampler!", __func__);
            throw std::runtime_error("failed to create texture sampler!");
        }
    }

    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = aspectFlags;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        // can left out explicit, the default value is 0(VK_COMPONENT_SWIZZLE_IDENTITY)
        //viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        //viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        //viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        //viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;

        VkImageView imageView;
        if (vkCreateImageView(mDevice, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
            spdlog::error("{} failed to create texture image view!", __func__);
            throw std::runtime_error("failed to create texture image view!");
        }

        return imageView;
    }

    void createImage(uint32_t width, uint32_t height,
        VkFormat format, VkImageTiling tiling,
        VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
        VkImage& image, VkDeviceMemory& imageMemory) {

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = static_cast<uint32_t>(width);
        imageInfo.extent.height = static_cast<uint32_t>(height);
        imageInfo.extent.depth = 1;
#ifdef BUG_FIXES
        // VUID-VkImageCreateInfo-imageCreateMaxMipLevels-02251
        uint32_t maxMipLevels = calculateMaxMipLevels(
                imageInfo.extent.width,
                imageInfo.extent.height,
                imageInfo.extent.depth
        );
        imageInfo.mipLevels = maxMipLevels;
#else
        imageInfo.mipLevels = 1;
#endif /* BUG_FIXES */
        // 图像的层数
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        // 表示该图像将被用作采样器中的纹理，允许它被着色器（shader）读取。
        // 简单来说，当你将一个图像用于着色器中的纹理采样操作时，你需要为该图像设置 VK_IMAGE_USAGE_SAMPLED_BIT 标志。
        imageInfo.usage = usage;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        // 多重采样
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.flags = 0;
        if (vkCreateImage(mDevice, &imageInfo, nullptr, &image) != VK_SUCCESS) {
            spdlog::error("{} failed to create Image!", __func__);
            throw std::runtime_error("failed to create Image!");
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(mDevice, image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);
        if (vkAllocateMemory(mDevice, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
            spdlog::error("{} failed to allocate image memory!", __func__);
            throw std::runtime_error("failed to allocate image memory!");
        }
        vkBindImageMemory(mDevice, image, imageMemory, 0);
    }

    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();
        // 同步对图像资源的访问
        // 在图像的不同的操作之间插入内存屏障
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
#ifndef EXPLICITLY_TRANSITIONNG_DEPTH_IMAGE
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
#else
        if(newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            if (hasStencilComponent(format)) {
                barrier.subresourceRange.aspectMask | VK_IMAGE_ASPECT_STENCIL_BIT;
            }
        } else {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }
#endif /* EXPLICITLY_TRANSITIONNG_DEPTH_IMAGE */
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = 0;

        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags dstinationStage;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
                newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            // 未定义 → 传输目的地：传输不需要等待任何内容的写入
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
                newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            // 传输目的地 → 着色器读取：着色器读取应该等待传输写入，特别是着色器在片段着色器中读取，因为这是我们要使用纹理的地方
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            // 由片段着色器去做读取操作
            dstinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
#ifdef EXPLICITLY_TRANSITIONNG_DEPTH_IMAGE
        } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
                newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
#endif /* EXPLICITLY_TRANSITIONNG_DEPTH_IMAGE */
        } else {
            spdlog::error("{} unsupported layout transition!", __func__);
            throw std::runtime_error("unsupported layout transition!");
        }

        vkCmdPipelineBarrier(
            commandBuffer,
            sourceStage, dstinationStage,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrier
        );

        endSingleTimeCommands(commandBuffer);
    }

    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;

        region.imageOffset = {0, 0, 0};
        region.imageExtent = {
            width,
            height,
            1
        };

        vkCmdCopyBufferToImage(
            commandBuffer,
            buffer,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region
        );

        endSingleTimeCommands(commandBuffer);
    }

    // recording and executing a command buffer
    VkCommandBuffer beginSingleTimeCommands() {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = mCommandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(mDevice, &allocInfo, &commandBuffer);
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        return commandBuffer;
    }
    // end a command buffer
    void endSingleTimeCommands(VkCommandBuffer commandBuffer) {
        vkEndCommandBuffer(commandBuffer);
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(mGraphicsQueue);

        vkFreeCommandBuffers(mDevice, mCommandPool, 1, &commandBuffer);
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

    void createDepthResources() {
        VkFormat depthFormat = findDepthFormat();
        createImage(mSwapChainExtent.width, mSwapChainExtent.height,
            depthFormat,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            mDepthImage,
            mDepthImageMemory
        );
        mDepthImageView = createImageView(mDepthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

#ifdef EXPLICITLY_TRANSITIONNG_DEPTH_IMAGE
        spdlog::info("{} explicitly transitioning depth image", __func__);
        transitionImageLayout(mDepthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        );
#endif /* EXPLICITLY_TRANSITIONNG_DEPTH_IMAGE */
    }

    void loadModel() {
        tinyobj::ObjReaderConfig readerConfig;
        readerConfig.mtl_search_path = "./models/";

        if (!mObjReaderInstance.ParseFromFile(MODEL_PATH, readerConfig)) {
            if (!mObjReaderInstance.Error().empty()) {
                spdlog::error("Error: {}", mObjReaderInstance.Error());
            }
            return;
        }

        if (!mObjReaderInstance.Warning().empty()) {
            spdlog::warn("Warning: {}", mObjReaderInstance.Warning());
        }

        // 存储了每一个vertex 的信息，包括 position, texcoords. normals, vertex color
        const tinyobj::attrib_t &attrib = mObjReaderInstance.GetAttrib();
        const std::vector<tinyobj::shape_t> &shapes = mObjReaderInstance.GetShapes();
        const std::vector<tinyobj::material_t> &materials = mObjReaderInstance.GetMaterials();

        // 我们先把所有的 vertex 都存储一下，以确认下标, 后面在遍历 shape 的时候，再补全 texcoord 和 
        uint64_t vertexNums = attrib.vertices.size() / 3;
        for (int i = 0; i < vertexNums; i++) {
            ops::Vertex tmpVertex{};
            tmpVertex.mPos = {
                attrib.vertices[i * 3 + 0],
                attrib.vertices[i * 3 + 1],
                attrib.vertices[i * 3 + 2]
            };
            mVertices.push_back(tmpVertex);
        }

        for (size_t shapeIndex = 0; shapeIndex < shapes.size(); ++shapeIndex) {
            ops::Shape_Mesh ourMesh;
            ourMesh.mName = shapes[shapeIndex].name;
            ourMesh.mMeterial_ID = 0; // 这个变量现在没有用了
            for (size_t meshIndex = 0; meshIndex < shapes[shapeIndex].mesh.indices.size() / 3; ++meshIndex) {
                tinyobj::index_t idx0 = shapes[shapeIndex].mesh.indices[meshIndex * 3 + 0];
                tinyobj::index_t idx1 = shapes[shapeIndex].mesh.indices[meshIndex * 3 + 1];
                tinyobj::index_t idx2 = shapes[shapeIndex].mesh.indices[meshIndex * 3 + 2];

                ops::Vertex& vertex0 = mVertices[idx0.vertex_index];
                ops::Vertex& vertex1 = mVertices[idx1.vertex_index];
                ops::Vertex& vertex2 = mVertices[idx2.vertex_index];

                int currentMaterialID = shapes[shapeIndex].mesh.material_ids[meshIndex];
                // texcoords
                vertex0.mTexCoord = {
                    attrib.texcoords[2 * idx0.texcoord_index],
                    1.0f - attrib.texcoords[2 * idx0.texcoord_index + 1]
                };
                vertex1.mTexCoord = {
                    attrib.texcoords[2 * idx1.texcoord_index],
                    1.0f - attrib.texcoords[2 * idx1.texcoord_index + 1]
                };
                vertex2.mTexCoord = {
                    attrib.texcoords[2 * idx2.texcoord_index],
                    1.0f - attrib.texcoords[2 * idx2.texcoord_index + 1]
                };

                // normals
                vertex0.mNormals = {
                    attrib.normals[idx0.normal_index * 3],
                    attrib.normals[idx0.normal_index * 3 + 1],
                    attrib.normals[idx0.normal_index * 3 + 2]
                };
                vertex1.mNormals = {
                    attrib.normals[idx1.normal_index * 3],
                    attrib.normals[idx1.normal_index * 3 + 1],
                    attrib.normals[idx1.normal_index * 3 + 2]
                };
                vertex2.mNormals = {
                    attrib.normals[idx2.normal_index * 3],
                    attrib.normals[idx2.normal_index * 3 + 1],
                    attrib.normals[idx2.normal_index * 3 + 2]
                };

                // materialID
                vertex0.mMaterialID = currentMaterialID;
                vertex1.mMaterialID = currentMaterialID;
                vertex2.mMaterialID = currentMaterialID;

                mIndices.push_back(idx0.vertex_index);
                mIndices.push_back(idx1.vertex_index);
                mIndices.push_back(idx2.vertex_index);

                ourMesh.mIndices.push_back(idx0.vertex_index);
                ourMesh.mIndices.push_back(idx1.vertex_index);
                ourMesh.mIndices.push_back(idx2.vertex_index);
            }
            ourMesh.mOffset = mIndices.size() - ourMesh.mIndices.size();
            mMeshes.push_back(ourMesh);
        }
    }

    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates,
        VkImageTiling tiling,
        VkFormatFeatureFlags features) {

        for (auto& format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(mPhysicalDevice, format, &props);
            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
                return format;
            } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
                return format;
            }
        }

        spdlog::error("{} failed to find supported format!", __func__);
        throw std::runtime_error("failed to find supported format!");
    }

    VkFormat findDepthFormat() {
        return findSupportedFormat(
            {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );
    }

    bool hasStencilComponent(VkFormat format) {
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
    }

    // glfw 控制输入
    void processInput(GLFWwindow* window) {
        float currentRenderTime = glfwGetTime();
        mCameraDeltaTime = currentRenderTime - mCameraLastRenderTime;
        mCameraLastRenderTime = currentRenderTime;
        float cameraSpeed = mCameraDeltaTime * mCameraSpeedFactor;

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            mCameraPos += cameraSpeed * front;
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            mCameraPos -= glm::normalize(glm::cross(front, up)) * cameraSpeed;
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
            mCameraPos -= cameraSpeed * front;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
            mCameraPos += glm::normalize(glm::cross(front, up)) * cameraSpeed;
        }
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
            mCameraPos += cameraSpeed * up;
        }
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
            mCameraPos -= cameraSpeed * up;
        }
    }

    static uint32_t calculateMaxMipLevels(uint32_t width, uint32_t height, uint32_t depth) {
        uint32_t maxDimension = width;
        if (height > maxDimension) maxDimension = height;
        if (depth > maxDimension) maxDimension = depth;

        return static_cast<uint32_t>(std::floor(std::log2(maxDimension) + 1));
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
    spdlog::set_level(spdlog::level::debug);
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
