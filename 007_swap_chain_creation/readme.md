

# 检查 swap chain 的支持情况

出于各种原因，并非所有显卡都能将图像直接显示在屏幕上，例如，它们是为服务器设计的，没有任何显示输出。 其次，由于图像呈现在很大程度上与窗口系统和与窗口相关的表面息息相关，它实际上并不是 Vulkan 核心的一部分。 您必须在查询其支持情况后启用 VK_KHR_swapchain 设备扩展。

为此，我们将首先扩展 `isDeviceSuitable` 函数，以检查该扩展是否受支持。 我们之前已经了解了如何列出 VkPhysicalDevice 所支持的扩展，因此这样做应该相当简单。 请注意，Vulkan 头文件提供了一个很好的宏 `VK_KHR_SWAPCHAIN_EXTENSION_NAME`，它被定义为 `VK_KHR_swapchain`。 使用这个宏的好处是编译器会捕捉拼写错误。

```c
const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};
```

接下来，创建一个新函数 `checkDeviceExtensionSupport`，从`isDeviceSuitable` 中调用，作为额外检查:

```c
    bool isDeviceSuitable(VkPhysicalDevice device)
    {
        QueueFamilyIndices indices = findQueueFamilies(device);
        bool extensionsSupported = checkDeviceExtensionSupport(device);
        return indices.isComplete() && extensionsSupported;
    }
```

修改函数的主体，以枚举扩展名，并检查其中是否包含所有必需的扩展名

```c
    bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
        
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());
#if 1
        spdlog::trace("{} enumerate {} device extension properties", __func__, extensionCount);
        for (auto& extension : availableExtensions) {
            spdlog::trace("{}    property: {}", __func__, extension.extensionName);
        }
#endif

        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }
```



# 使用 Device Extension

使用交换链需要首先启用 `VK_KHR_swapchain` 扩展。 启用该扩展只需对逻辑设备创建结构稍作修改即可

```c
void createLogicalDevice() {
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

        std::set<uint32_t> uniqueQueueFamilies = {
            indices.graphicsFamily.value(),
            indices.presentFamily.value()
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

        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            spdlog::error("{} create logical device error", __func__);
            throw std::runtime_error("failed to create logical device");
        }

        vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
        vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
    }
```

# 获取 swap chain 支持的详细信息

仅仅检查交换链是否可用是不够的，因为它实际上可能与我们的window surface不兼容。 创建swap chain 还涉及比创建实例和设备更多的设置，因此我们需要查询更多细节后才能继续。

我们需要检查的属性基本上有三种：

- 基础的surface 能力(swap chain 中最大最小的 images 的数量；images 最大和最小的长宽信息)

- Surface 的格式(像素格式，色域支持)

- presentation 的模式

与 `findQueueFamilies` 类似，在查询到这些详细信息后，我们将使用结构体来传递它们。 上述三种类型的属性以下列结构体和结构体列表的形式出现：

```c
struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};
```

现在我们将创建一个新函数 `querySwapChainSupport`，用于填充此结构

```c
SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice pDevice) {
        SwapChainSupportDetails details;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pDevice, surface, &details.capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(pDevice, surface, &formatCount, nullptr);

        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(pDevice, surface, &formatCount, details.formats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(pDevice, surface, &presentModeCount, nullptr);

        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(pDevice, surface, &presentModeCount, details.presentModes.data());
        }

        return details;
    }
```

现在所有细节都在结构体中，因此让我们再次扩展 isDeviceSuitable，利用该函数来验证交换链支持是否充分:

```c
bool isDeviceSuitable(VkPhysicalDevice pDevice)
    {
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
```

# 为 swap chain 选择合适的设置

如果满足 swapChainAdequate 条件，那么支持肯定是足够的，但仍可能存在许多不同的优化模式。 现在，我们将编写几个函数，以找到最佳交换链的正确设置。 有三种类型的设置需要确定：

- surface format(color depth)

- presentation 模式（将图像 "交换 "到屏幕上的条件）

- 交换范围（交换链中图像的分辨率）

## surface format

```c
VkSurfaceFormatKHR chooseSwapSurfaceFormat(
        const std::vector<VkSurfaceFormatKHR>& availableFormats) {
        for (const auto& availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8_SRGB &&
                availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }

        return availableFormats[0];
    }
```

## Presentation mode

 presentation mode可以说是交换链中最重要的设置，因为它代表了向屏幕显示图像的实际条件。 Vulkan 有四种可能的模式：

- `VK_PRESENT_MODE_IMMEDIATE_KHR`: 应用程序提交的图像会立即传输到屏幕上，这可能会导致撕裂
- `VK_PRESENT_MODE_FIFO_KHR`: 交换链是一个队列，当刷新显示屏时，显示屏会从队列前部获取图像，而程序会将渲染好的图像插入队列后部。 如果队列已满，程序就必须等待。 这与现代游戏中的垂直同步最为相似。 刷新显示屏的时刻称为 "vertical blank"
- `VK_PRESENT_MODE_FIFO_RELAXED_KHR`: 该模式与前一种模式的区别仅在于应用程序较晚且队列在上一次vertical blank时为空。 图像不会等待下一个vertical blank，而是在最终到达时立即传输。 这可能会导致明显的撕裂
- `VK_PRESENT_MODE_MAILBOX_KHR`: 这是第二种模式的另一种变体。 当队列已满时，不会阻塞应用程序，而是直接用较新的图像替换已在队列中的图像。 这种模式可用于尽可能快地渲染帧，同时还能避免撕裂，与标准垂直同步相比，延迟问题更少。 这种模式通常被称为 "三重缓冲"，但仅仅存在三个缓冲区并不一定意味着帧速率已解锁

只有 `VK_PRESENT_MODE_FIFO_KHR` 模式能保证可用，因此我们必须再次编写一个函数来查找可用的最佳模式：

```cpp
VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
        return VK_PRESENT_MODE_FIFO_KHR;
    }
```

我个人认为，如果不考虑能耗问题，`VK_PRESENT_MODE_MAILBOX_KHR` 是一个非常不错的权衡方案。 它允许我们在避免撕裂的同时，通过渲染尽可能最新的新图像直到垂直空白，保持相当低的延迟。 在移动设备上，由于能耗更为重要，您可能需要使用 `VK_PRESENT_MODE_FIFO_KHR`。 现在，让我们查看一下列表，看看 `VK_PRESENT_MODE_MAILBOX_KHR` 是否可用：

```c
VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
        for (const auto& availablePresentMode : availablePresentModes) {
            if  (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                spdlog::trace("{} choose {}", __func__, "VK_PRESENT_MODE_MAILBOX_KHR");
                return availablePresentMode;
            }
        }
        spdlog::trace("{} use {} for default", __func__, "VK_PRESENT_MODE_FIFO_KHR");
        return VK_PRESENT_MODE_FIFO_KHR;
    }
```

## Swap extent

swap extent 是交换链图像的分辨率，它几乎总是完全等于我们正在绘制的窗口的像素分辨率（稍后详述）。 `VkSurfaceCapabilitiesKHR` 结构定义了可能的分辨率范围。 Vulkan 通过在 currentExtent 成员中设置宽度和高度，告诉我们要与窗口的分辨率相匹配。 不过，有些窗口管理器允许我们在这里有所区别，这表现为将 currentExtent 中的宽度和高度设置为一个特殊值：uint32_t 的最大值。 在这种情况下，我们将在 minImageExtent 和 maxImageExtent 范围内选择与窗口最匹配的分辨率。 但我们必须以正确的单位指定分辨率。

GLFW 在测量尺寸时使用两种单位：像素和屏幕坐标。 例如，我们之前在创建窗口时指定的分辨率 {WIDTH, HEIGHT} 是以屏幕坐标为单位的。 但 Vulkan 使用的是像素，因此交换链的范围也必须以像素为单位指定。 遗憾的是，如果使用的是高 DPI 显示器（如苹果的 Retina 显示器），屏幕坐标与像素并不对应。 相反，由于像素密度较高，窗口的像素分辨率将大于屏幕坐标分辨率。 因此，如果 Vulkan 不为我们固定交换范围，我们就不能只使用原来的 {WIDTH, HEIGHT}。 相反，我们必须使用 `glfwGetFramebufferSize` 以像素为单位查询窗口的分辨率，然后再将其与图像的最小和最大范围相匹配。

```c
VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        } else {
            int width, height;
            glfwGetFramebufferSize(mWindow, &width, &height);

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
```

## Creating the swap chain

现在，有了这些辅助函数来帮助我们在运行时做出选择，我们终于掌握了创建工作交换链所需的全部信息。



# 一些 debug 信息

独立显卡的 swap chain 的一些信息

```
(lldb) print swapChainSupport.mCapabilities 
(VkSurfaceCapabilitiesKHR) {
  minImageCount = 4
  maxImageCount = 0
  currentExtent = (width = 4294967295, height = 4294967295)
  minImageExtent = (width = 1, height = 1)
  maxImageExtent = (width = 16384, height = 16384)
  maxImageArrayLayers = 1
  supportedTransforms = 1
  currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR
  supportedCompositeAlpha = 3
  supportedUsageFlags = 524447
```

```
(lldb) print swapChainSupport.mFormats 
(std::vector<VkSurfaceFormatKHR>) size=9 {
  [0] = (format = VK_FORMAT_A2R10G10B10_UNORM_PACK32, colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
  [1] = (format = VK_FORMAT_A2B10G10R10_UNORM_PACK32, colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
  [2] = (format = VK_FORMAT_B8G8R8A8_SRGB, colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
  [3] = (format = VK_FORMAT_B8G8R8A8_UNORM, colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
  [4] = (format = VK_FORMAT_R8G8B8A8_SRGB, colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
  [5] = (format = VK_FORMAT_R8G8B8A8_UNORM, colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
  [6] = (format = VK_FORMAT_R16G16B16A16_UNORM, colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
  [7] = (format = VK_FORMAT_R16G16B16A16_SFLOAT, colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
  [8] = (format = VK_FORMAT_R5G6B5_UNORM_PACK16, colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
}
```

```
(lldb) print swapChainSupport.mPresentModes 
(std::vector<VkPresentModeKHR>) size=3 {
  [0] = VK_PRESENT_MODE_MAILBOX_KHR
  [1] = VK_PRESENT_MODE_FIFO_KHR
  [2] = VK_PRESENT_MODE_IMMEDIATE_KHR
}
```

### 创建 swap chain 时候的 debug 信息

```
Process 98501 stopped
* thread #1, name = 'swap_chain', stop reason = breakpoint 2.1
    frame #0: 0x00005555555685b1 swap_chain`HelloTriangleApplication::createSwapChain(this=0x00007fffffffcad0) at main.cpp:327:33
   324 
   325          createInfo.oldSwapchain = VK_NULL_HANDLE;
   326 
-> 327          if (vkCreateSwapchainKHR(mDevice, &createInfo, nullptr, &mSwapChain) != VK_SUCCESS) {
   328              spdlog::error("{} failed to create swap chain", __func__);
   329              throw std::runtime_error("failed to create swap chain");
   330          }
(lldb) print createInfo 
(VkSwapchainCreateInfoKHR) {
  sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR
  pNext = 0x0000000000000000
  flags = 0
  surface = 0xfab64d0000000002
  minImageCount = 5
  imageFormat = VK_FORMAT_A2R10G10B10_UNORM_PACK32
  imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
  imageExtent = (width = 800, height = 600)
  imageArrayLayers = 1
  imageUsage = 16
  imageSharingMode = VK_SHARING_MODE_EXCLUSIVE
  queueFamilyIndexCount = 0
  pQueueFamilyIndices = 0x0000000000000000
  preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR
  compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
  presentMode = VK_PRESENT_MODE_MAILBOX_KHR
  clipped = 1
  oldSwapchain = nullptr
}
(lldb) print surfaceFormat 
(VkSurfaceFormatKHR)  (format = VK_FORMAT_A2R10G10B10_UNORM_PACK32, colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)

```



我在第一次调试的时候，发现这里 surfaceFormat 得到的结果是 `VK_FORMAT_A2R10G10B10_UNORM_PACK32`。这让我感到非常的奇怪。仔细看了一下代码，发现是下面的这段代码写错了：

```c
VkSurfaceFormatKHR chooseSwapSurfaceFormat(
        const std::vector<VkSurfaceFormatKHR>& availableFormats) {
        for (const auto& availableFormat : availableFormats) {
            // 这里错误的写成了VK_FORMAT_B8G8R8_SRGB，应该写成 VK_FORMAT_B8G8R8A8_SRGB
            if (availableFormat.format == VK_FORMAT_B8G8R8_SRGB &&
                availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                spdlog::trace("{} find available format with RGBA8888 format and" 
                    "sRGB nonlinear color space", __func__);
                return availableFormat;
            }
        }

        return availableFormats[0];
    }
```

在完成上面的错误修正之后：

```
(VkSwapchainCreateInfoKHR) {
  sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR
  pNext = 0x0000000000000000
  flags = 0
  surface = 0xfab64d0000000002
  minImageCount = 5
  imageFormat = VK_FORMAT_B8G8R8A8_SRGB
  imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
  imageExtent = (width = 800, height = 600)
  imageArrayLayers = 1
  imageUsage = 16
  imageSharingMode = VK_SHARING_MODE_EXCLUSIVE
  queueFamilyIndexCount = 0
  pQueueFamilyIndices = 0x0000000000000000
  preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR
  compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
  presentMode = VK_PRESENT_MODE_MAILBOX_KHR
  clipped = 1
  oldSwapchain = nullptr
}
```

前面有说过，窗口的长宽可能和实际上的 pixel 长宽并不一致。部分高分屏中，可能 pixel 的长宽要大于实际窗口的长宽数值，但在我的电脑上是一致的 。
