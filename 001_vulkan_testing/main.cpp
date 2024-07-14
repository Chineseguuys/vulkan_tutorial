#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <spdlog/spdlog.h>
#include <vector>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

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
    VkSurfaceKHR mSurface;

    void initWindow()
    {
        // need call init first
        glfwInit();
        if (glfwVulkanSupported()) {
            // vulkan is available, at least for compute
            spdlog::info("{}: vulkan is available, at least for compute", __FUNCTION__);
        } else {
            spdlog::error("{}: vulkan not available", __FUNCTION__);
            throw std::runtime_error("vulkan not available");
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        // 关于在 wayland 窗口系统下使用 glfw + vulkan, 下面这一点需要注意
        // https://github.com/glfw/glfw/issues/1398
        mWindow = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
        if (mWindow == nullptr) {
            spdlog::error("{}: can not create glfw window", __FUNCTION__);
            throw std::runtime_error("can not create glfw window");
        }
    }

    void initVulkan()
    {
        uint32_t count{0};
        const char** extensions = glfwGetRequiredInstanceExtensions(&count);
        if (count == 0) {
            spdlog::warn("{}: glfwGetRequiredInstanceExtensions returns 0 extension", __FUNCTION__);
        }
        for (int i = 0; i < count; ++i) {
            spdlog::debug("{}:{}", __FUNCTION__, extensions[i]);
        }

        this->listVulkanLayers();

        VkInstanceCreateInfo createInfo;
        memset(&createInfo, 0, sizeof(VkInstanceCreateInfo));
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pNext = NULL;
        createInfo.flags = 0;
        createInfo.enabledExtensionCount = count;
        createInfo.ppEnabledExtensionNames = extensions;

        VkResult result = vkCreateInstance(&createInfo, NULL, &mInstance);
        if (result != VK_SUCCESS) {
            spdlog::error("{}: Vulkan instance creation failed", __FUNCTION__);
            throw std::runtime_error("Vulkan instance creation failed");
        } else {
            spdlog::info("{}: Vulkan instance creation success", __FUNCTION__);
        }

        result = glfwCreateWindowSurface(mInstance, mWindow, nullptr, &mSurface);
        if (result != VK_SUCCESS) {
            spdlog::error("{}: GLFW: window surface create failed", __FUNCTION__);
            throw std::runtime_error("window surface create failed");
        }

        this->listVulkanDevices(mInstance);
    }

    void listVulkanLayers() {
        uint32_t layerCount{0};
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
        for (VkLayerProperties& layer : availableLayers) {
            spdlog::debug("Layer Name: {}", layer.layerName);
            spdlog::debug("  Layer Description: {}", layer.description);
            spdlog::debug("  Spec Version: {}.{}.{}", 
                VK_VERSION_MAJOR(layer.specVersion), 
                VK_VERSION_MINOR(layer.specVersion),
                VK_VERSION_PATCH(layer.specVersion)
            );
            spdlog::debug("  Implementation Version: {}", layer.implementationVersion);
        }
    }

    void listVulkanDevices(VkInstance& instance) {
        uint32_t gpuCount{0};
        vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr);
        std::vector<VkPhysicalDevice> gpuList(gpuCount);
        // get all devices from the pc
        vkEnumeratePhysicalDevices(instance, &gpuCount, gpuList.data());
        spdlog::debug("--------- GPU Device ---------");
        for (VkPhysicalDevice& device : gpuList) {
            VkPhysicalDeviceProperties devicesProperties;
            vkGetPhysicalDeviceProperties(device, &devicesProperties);
            spdlog::debug("{}:", devicesProperties.deviceName);
            spdlog::debug("\t apiVersion\t = {}.{}.{}", 
                VK_VERSION_MAJOR(devicesProperties.apiVersion),
                VK_VERSION_MINOR(devicesProperties.apiVersion),
                VK_VERSION_PATCH(devicesProperties.apiVersion)
            );
            spdlog::debug("\t driverVersion\t = {}.{}.{}", 
                VK_VERSION_MAJOR(devicesProperties.driverVersion),
                VK_VERSION_MINOR(devicesProperties.driverVersion),
                VK_VERSION_PATCH(devicesProperties.driverVersion)
            );
            spdlog::debug("\t vendorID\t = 0x{0:x}", devicesProperties.vendorID);
            spdlog::debug("\t deviceID\t = 0x{0:x}", devicesProperties.deviceID);
        }
    }

    void mainLoop()
    {
        while (!glfwWindowShouldClose(mWindow))
        {
            glfwPollEvents();

            glfwPostEmptyEvent();
        }
    }

    void cleanup()
    {
        vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
        vkDestroyInstance(mInstance, nullptr);

        glfwDestroyWindow(mWindow);

        glfwTerminate();
    }
};

int main()
{
    spdlog::set_level(spdlog::level::debug);

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