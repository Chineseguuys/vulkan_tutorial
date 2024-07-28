# Framebuffers

在过去的几章中，我们已经讨论了很多关于帧缓冲区的内容，并且我们已经设置了渲染传递，以期望获得与交换链图像格式相同的单帧缓冲区，但实际上我们还没有创建任何帧缓冲区。

在创建呈现传递时指定的附件会被封装到一个 VkFramebuffer 对象中。 帧缓冲器对象会引用代表附件的所有 VkImageView 对象。 在我们的例子中，只有一个：颜色附件。 然而，我们必须为附件使用的图像取决于交换链在我们获取图像进行展示时返回的图像。 这意味着我们必须为交换链中的所有图像创建一个帧缓冲，并在绘制时使用与检索到的图像相对应的图像。

```c
// Framebuffers
std::vector<VkFramebuffer> mSwapChainFramebuffers;
```

我们将在一个新函数 createFramebuffers 中为该数组创建对象，该函数将在创建图形管道后立即从 initVulkan 中调用：

```c
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
}
```

首先调整容器大小，以容纳所有帧缓冲区：

```c
void createFrameBuffers() {
     mSwapChainFramebuffers.resize(mSwapChainImageViews.size());        
}
```

然后，我们将遍历image view，并从中创建frame buffer：

```c
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
```

如你所见，帧缓冲的创建非常简单。 首先，我们需要指定帧缓冲需要与哪个render pass兼容。 你只能在与之兼容的render pass中使用帧缓冲，这大致意味着它们使用相同数量和类型的attachments。

attachmentCount和pAttachments成员变量用于指定附着个数，以及渲染流程对象用于描述附着信息的pAttachment数组。

width和height成员变量用于指定帧缓冲的大小，layers成员变量用于指定图像层数。我们使用的交换链图像都是单层的，所以将layers成员变量设置为1。

我们需要在应用程序结束前，清除图像视图和渲染流程对象前清除帧缓冲对象：

```c
for (auto& framebuffer : mSwapChainFramebuffers) {
       vkDestroyFramebuffer(mDevice, framebuffer, nullptr);
}
```
