# Command buffers

Vulkan下的指令，比如绘制指令和内存传输指令并不是直接通过函数调用执行的。我们需要将所有要执行的操作记录在一个指令缓冲对象，然后提交给可以执行这些操作的队列才能执行。这使得我们可以在程序初始化时就准备好所有要指定的指令序列，在渲染时直接提交执行。也使得多线程提交指令变得更加容易。我们只需要在需要指定执行的使用，将指令缓冲对象提交给Vulkan处理接口。



# Command pools

在创建指令缓冲对象之前，我们需要先创建指令池对象。指令池对象用于管理指令缓冲对象使用的内存，并负责指令缓冲对象的分配。我们添加了一个VkCommandPool成员变量到类中：

```c
// command pools
VkCommandPool mCommandPool;
```

添加一个叫做createCommandPool的函数，并在initVulkan函数中帧缓冲对象创建之后调用它：

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
        createCommandPool();
    }
```

指令池对象的创建只需要填写两个参数：

```c
QueueFamilyIndices queueFamilyIndices = findQueueFamilies(mPhysicalDevice);
VkCommandPoolCreateInfo poolInfo{};
poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
poolInfo.queueFamilyIndex = queueFamilyIndices.mGraphicsFamily.value();
```

有下面两种用于指令池对象创建的标记，可以提供有用的信息给Vulkan的驱动程序进行一定优化处理：

- `VK_COMMAND_POOL_CREATE_TRANSIENT_BIT`: 使用它分配的指令缓冲对象被频繁用来记录新的指令(使用这一标记可能会改变帧缓冲对象的内存分配策略)
- `VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT`: 指令缓冲对象之间相互独立，不会被一起重置。不使用这一标记，指令缓冲对象会被放在一起重置。



我们将在每一帧录制一个命令缓冲区，因此我们希望能够重置并重新录制。 因此，我们需要为命令池设置 `VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT` 标志位。

执行命令缓冲区的方法是将其提交到某个设备队列，如我们检索到的图形和演示队列。 每个命令池只能分配在一种队列上提交的命令缓冲区。 我们将记录绘图命令，因此选择了图形队列系列。

```c
if (vkCreateCommandPool(mDevice, &poolInfo, nullptr, &mCommandPool) != VK_SUCCESS) {
      spdlog::error("{} failed to create command pool", __func__);
      throw std::runtime_error("failed to create command pool");
}
```

使用 vkCreateCommandPool 函数完成命令池的创建。 该函数没有任何特殊参数。 整个程序都将使用命令在屏幕上绘制图形，因此只有在程序结束时才应销毁池：

```c
vkDestroyCommandPool(mDevice, mCommandPool, nullptr);
```

# Command buffer allocation

现在，我们可以开始分配指令缓冲对象，使用它记录绘制指令。由于绘制操作是在帧缓冲上进行的，我们需要为交换链中的每一个图像分配一个指令缓冲对象。为此，我们添加了一个数组作为成员变量来存储创建的VkCommandBuffer对象。指令缓冲对象会在指令池对象被清除时自动被清除，不需要我们自己显式地清除它。

```c
VkCommandBuffer mCommandBuffer;
```

level成员变量用于指定分配的指令缓冲对象是主要指令缓冲对象还是辅助指令缓冲对象：

- `VK_COMMAND_BUFFER_LEVEL_PRIMARY`:可以被提交到队列进行执行，但不能被其它指令缓冲对象调用
- `VK_COMMAND_BUFFER_LEVEL_SECONDARY`: 不能直接被提交到队列进行执行，但可以被主要指令缓冲对象调用执行

在这里，我们没有使用辅助指令缓冲对象，但辅助指令给缓冲对象的好处是显而易见的，我们可以把一些常用的指令存储在辅助指令缓冲对象，然后在主要指令缓冲对象中调用执行。

# Command buffer recording

现在，我们开始使用 `recordCommandBuffer` 函数，该函数用于将我们要执行的命令写入命令缓冲区。 所使用的 `VkCommandBuffer` 将作为参数传递，我们要写入的当前 `swapchain` 镜像的索引也将作为参数传递。

```c
void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
}
```

```c
VkCommandBufferBeginInfo beginInfo{};
beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
beginInfo.flags = 0;
beginInfo.pInheritanceInfo = nullptr;

if (vkBeginCommandBuffer(mCommandBuffer, &beginInfo) != VK_SUCCESS) {
      spdlog::error("{} failed to begin recording command buffer");
      throw std::runtime_error("failed to begin recording command buffer");
}
```

flags成员变量用于指定我们将要怎样使用指令缓冲。它的值可以是下面这些：

- `VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT`: The command buffer will be rerecorded right after executing it once.
- `VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT`: This is a secondary command buffer that will be entirely within a single render pass.
- `VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT`: The command buffer can be resubmitted while it is also already pending execution.

这些flags现在都不适合我们。

pInheritanceInfo 参数仅适用于二级命令缓冲区(secondary command buffers)。 它指定从调用的主命令缓冲区继承哪种状态。

如果命令缓冲区已被记录过一次，那么调用 vkBeginCommandBuffer 将隐式重置它。 以后不可能再向缓冲区添加命令。

# Starting a render pass

调用`vkCmdBeginRenderPass`函数可以开始一个渲染流程。这一函数需要我们使用`VkRenderPassBeginInfo`结构体来指定使用的渲染流程对象：

第一个参数是渲染传递本身和要绑定的附件。 我们为每个交换链图像创建了一个帧缓冲，并将其指定为颜色附件。 因此，我们需要为要绘制的交换链图像绑定帧缓冲。 利用传入的 imageIndex 参数，我们可以为当前的 swapchain 图像选择合适的帧缓冲。

```c
VkRenderPassBeginInfo renderPassInfo{};
renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
renderPassInfo.renderPass = mRenderPass;
renderPassInfo.framebuffer = mSwapChainFramebuffers[imageIndex];
```

接下来的两个参数定义了渲染区域的大小。 渲染区域定义了着色器加载和存储的位置。 该区域以外的像素将具有未定义的值。 为达到最佳性能，该区域应与attachments的大小相匹配。

```c
renderPassInfo.renderArea.offset = {0, 0};
renderPassInfo.renderArea.extent = mSwapChainExtent;
```

最后两个参数定义了用于 `VK_ATTACHMENT_LOAD_OP_CLEAR` 的透明值，我们将其用作颜色附件的加载操作。 我将透明色定义为不透明度为 100% 的黑色。

```c
VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
renderPassInfo.clearValueCount = 1;
renderPassInfo.pClearValues = &clearColor;
```

现在可以开始渲染了。 所有记录命令的函数都可以通过其 vkCmd 前缀识别出来。 它们都返回 void，因此在完成记录之前不会有错误处理。

每个命令的第一个参数都是要记录命令的命令缓冲区。 第二个参数指定我们刚刚提供的渲染传递的细节。 最后一个参数控制如何提供渲染传递中的绘图命令。 它可以有两个值：

- `VK_SUBPASS_CONTENTS_INLINE`: The render pass commands will be embedded in the primary command buffer itself and no secondary command buffers will be executed.
- `VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS`: The render pass commands will be executed from secondary command buffers.

我们将不使用二级命令缓冲区，因此采用第一种方案。

```c
vkCmdBeginRenderPass(mCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
```

# Basic drawing commands

现在我们可以绑定图形管道了：

```c
vkCmdBindPipeline(mCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mGraphicsPipeline);
```

第二个参数指定管道对象是图形管道还是计算管道。 现在我们已经告诉了 Vulkan 哪些操作要在图形流水线中执行，哪些附件要在片段着色器中使用。

如 fixed functions 一章所述，我们已经指定了视口和剪刀状态，以使该流水线成为动态流水线。 因此，我们需要在发出绘制命令前在命令缓冲区中设置它们：

```c
VkViewport viewport{};
viewport.x = 0.0f;
viewport.y = 0.0f;
viewport.width = static_cast<float>(mSwapChainExtent.width);
viewport.height = static_cast<float>(mSwapChainExtent.height);
viewport.minDepth = 0.0f;
viewport.maxDepth = 1.0f;
vkCmdSetViewport(mCommandBuffer, 0, 1, &viewport);

VkRect2D scissor{};
scissor.offset = {0, 0};
scissor.extent = mSwapChainExtent;
vkCmdSetScissor(mCommandBuffer, 0, 1, &scissor);
```

现在，我们可以发出绘制三角形的命令了：

```c
vkCmdDraw(mCommandBuffer, 3, 1, 0, 0);
```

- `vertexCount`: Even though we don't have a vertex buffer, we technically still have 3 vertices to draw.
- `instanceCount`: Used for instanced rendering, use `1` if you're not doing that.
- `firstVertex`: Used as an offset into the vertex buffer, defines the lowest value of `gl_VertexIndex`.
- `firstInstance`: Used as an offset for instanced rendering, defines the lowest value of `gl_InstanceIndex`.

# Finishing up

现在可以结束渲染传递：

```c
vkCmdEndRenderPass(mCommandBuffer);
```

我们已经完成了命令缓冲区的录制：

```c
if (vkEndCommandBuffer(mCommandBuffer) != VK_SUCCESS) {
       spdlog::error("{} failed to record command buffer", __func__);
       throw std::runtime_error("failed to record command buffer");
}
```
