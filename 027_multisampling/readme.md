# 创建 framebuffer 时，附件的顺序

在 Vulkan 中，创建 `VkFramebuffer` 时，`VkFramebufferCreateInfo` 结构体中的 `pAttachments` 数组的顺序至关重要。这个顺序需要与所使用的 `VkRenderPass` 的附件顺序相对应，以确保正确地将图像视图绑定到渲染通道的各个附件（如颜色附件、深度附件等）。

## 理解 `VkRenderPass` 与 `VkFramebuffer` 的关系

### 1. `VkRenderPass` 的附件顺序

在创建 `VkRenderPass` 时，通过 `VkRenderPassCreateInfo` 的 `pAttachments` 成员定义了所有可能的附件。每个附件在数组中的位置（即索引）决定了它在渲染通道中的引用方式。例如：

```c
VkAttachmentDescription colorAttachment{};
        // attachment 的格式，例如 VK_FORMAT_B8G8R8A8_SRGB
        colorAttachment.format = mSwapChainImageFormat;
        // 多重采用，这里使用多重采用
        colorAttachment.samples = mMSAASamples;
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
        // 由于多采样的图像不能直接呈现，首先需要转化为常规的图像
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // 我们需要添加一个附加的 attachment
        VkAttachmentDescription colorAttachmentResolve{};
        colorAttachmentResolve.format = mSwapChainImageFormat;
        colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        // depth attachment
        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = findDepthFormat();
        depthAttachment.samples = mMSAASamples;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // subpasses and attachment reference
        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorAttachmentResolveRef{};
        colorAttachmentResolveRef.attachment = 2;
        colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;
        subpass.pResolveAttachments = &colorAttachmentResolveRef;

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
        std::array<VkAttachmentDescription, 3> attachments = {colorAttachment, depthAttachment, colorAttachmentResolve};

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
```

在上述示例中：

- 第0个附件是color附件。
- 第1个附件是depth附件。
- 第二各附件是 color resolve 附件。

### 2. `VkFramebuffer` 的附件顺序

创建 `VkFramebuffer` 时，`VkFramebufferCreateInfo` 的 `pAttachments` 数组必须按照与 `VkRenderPass` 中定义的附件顺序一致的方式提供 `VkImageView` 对象。具体来说，`pAttachments` 中的每个元素对应于 `VkRenderPass` 中 `pAttachments` 数组中相同索引的附件。

```c
void createFrameBuffers() {
        mSwapChainFramebuffers.resize(mSwapChainImageViews.size());
        for (unsigned int i = 0; i < mSwapChainImageViews.size(); ++i) {
            std::array<VkImageView, 3> attachments = {
                // 这里的顺序必须和 render pass 中创建相应的附件的创建顺序保持一致
                mColorImageView, // 对应 renderpass 中的 0
                mDepthImageView, // 对应 renderpass 中的 1
                mSwapChainImageViews[i], // 对应 renderpass 中
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
```

在这个示例中：

- `attachments[0]` 对应于 `VkRenderPass` 的第0个附件（Color）。
- `attachments[1]` 对应于 `VkRenderPass` 的第1个附件（depth）。
- `attachments[2]` 对应于 `VkRenderPass` 的第2个附件（color resolve）。

## 确认附件顺序的步骤

### 步骤一：查看 `VkRenderPass` 中附件的定义顺序

首先，查看 `VkRenderPassCreateInfo` 中 `pAttachments` 数组中每个 `VkAttachmentDescription` 的定义顺序。每个附件在数组中的索引（从0开始）决定了它在 `VkFramebuffer` 中的位置。

### 步骤二：按照相同的索引顺序提供 `VkImageView`

在创建 `VkFramebuffer` 时，确保 `pAttachments` 数组中的 `VkImageView` 顺序与 `VkRenderPass` 中 `pAttachments` 的顺序一致。每个 `VkImageView` 应对应于 `VkRenderPass` 中相同索引的附件。

### 示例代码总结

以下是一个完整的示例，展示了如何确保 `VkFramebuffer` 的 `pAttachments` 顺序与 `VkRenderPass` 中的附件顺序一致：

```c
// 假设已创建 Render Pass 和 Image Views

// Render Pass 的附件顺序
VkAttachmentDescription attachments[] = { /* 定义的附件 */ };
VkAttachmentReference colorAttachmentRef = { .attachment = 0, /* 其他成员 */ };
VkAttachmentReference depthAttachmentRef = { .attachment = 1, /* 其他成员 */ };
VkSubpassDescription subpass = { /* 定义子通道，引用附件 */ };

// 创建 Render Pass
VkRenderPassCreateInfo renderPassInfo = { /* 设置附件和子通道 */ };
VkRenderPass renderPass;
vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass);

// 创建 Framebuffer
VkImageView framebufferAttachments[] = {
    colorImageView, // 对应 Render Pass 的第0个附件（颜色）
    depthImageView  // 对应 Render Pass 的第1个附件（深度）
};

VkFramebufferCreateInfo framebufferInfo = {};
framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
framebufferInfo.renderPass = renderPass;
framebufferInfo.attachmentCount = 2;
framebufferInfo.pAttachments = framebufferAttachments;
framebufferInfo.width = swapChainExtent.width;
framebufferInfo.height = swapChainExtent.height;
framebufferInfo.layers = 1;

VkFramebuffer framebuffer;
vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffer);
```

## 注意事项

1. **索引对应**：`VkRenderPass` 中的附件索引必须与 `VkFramebuffer` 中的 `pAttachments` 索引严格对应。如果顺序不一致，将导致渲染结果异常或错误。

2. **附件数量**：`VkFramebufferCreateInfo` 的 `attachmentCount` 必须与 `VkRenderPass` 中的附件数量一致。

3. **Image View 类型**：确保 `VkImageView` 的类型和格式与 `VkRenderPass` 中定义的附件描述相匹配。例如，颜色附件的 `VkImageView` 应该具有与颜色附件相同的格式。

4. **多子通道**：如果 `VkRenderPass` 包含多个子通道（subpass），所有子通道中引用的附件索引都必须在 `VkFramebuffer` 的 `pAttachments` 中正确映射。

## 进一步优化

### 动态 Framebuffer 大小

如果您的应用需要支持动态调整窗口大小，可以在每次窗口大小变化时重新创建与新的大小匹配的 `VkFramebuffer`。

### 使用多重采样

在使用多重采样时，可能需要额外的临时附件（如多重采样颜色附件），确保在 `VkFramebuffer` 的 `pAttachments` 数组中正确包含这些附件，并与 `VkRenderPass` 的定义一致。

### 处理多个渲染目标

如果您的渲染通道使用多个颜色附件（例如用于延迟渲染或多渲染目标），在 `VkFramebuffer` 中的 `pAttachments` 数组中按相同顺序提供所有相关的 `VkImageView`。

## 总结

在 Vulkan 中，创建 `VkFramebuffer` 时，`pAttachments` 数组的顺序必须与 `VkRenderPass` 中 `pAttachments` 数组的顺序严格对应。确保每个 `VkImageView` 在 `VkFramebuffer` 中的位置与 `VkRenderPass` 中的附件索引一致，是实现正确渲染的关键。通过仔细管理附件顺序和索引，您可以避免渲染错误并确保图像资源的正确绑定和使用。

# Resolve



> "Once a multisampled buffer is created, it has to be resolved to the default framebuffer (which stores only a single sample per pixel)."

其中的 **"resolved"**（解析或解决）一词在图形编程，尤其是在 Vulkan 或 OpenGL 中，有特定的含义。下面详细解释 **"resolved"** 在此上下文中的含义及其代表的操作。

## **"Resolved" 的含义**

**"Resolved"** 在图形渲染中，特别是在多重采样抗锯齿（Multisample Anti-Aliasing, MSAA）的过程中，指的是将多重采样缓冲区（multisampled buffer）转换为单一采样的缓冲区（single-sampled buffer）的过程。具体来说，这个过程涉及将每个像素的多个采样点（samples）合并为一个最终的颜色值，以生成能够显示在默认帧缓冲区（default framebuffer）中的图像。

### **多重采样与单一采样**

- **多重采样缓冲区（Multisampled Buffer）**：
  
  - 每个像素包含多个采样点（例如，4x MSAA 表示每个像素有4个采样点）。
  - 主要用于抗锯齿，通过在多个采样点上计算颜色来平滑边缘，减少锯齿状边缘（aliasing）。

- **单一采样缓冲区（Single-sampled Buffer）**：
  
  - 每个像素只有一个采样点。
  - 默认帧缓冲区通常是单一采样的，因为显示设备（如屏幕）每个像素只能显示一个颜色值。

### **解析（Resolve）的过程**

解析操作将多重采样缓冲区中的多个采样点合并为一个单一的颜色值。这通常通过以下几种方式实现：

1. **平均化（Averaging）**：
   
   - 对每个像素的所有采样点的颜色值进行平均，得到一个最终的颜色值。

2. **加权平均（Weighted Averaging）**：
   
   - 根据采样点的位置或其他权重，对颜色值进行加权平均。

3. **其他合并方法**：
   
   - 例如，最大值、最小值或特定算法决定如何合并采样点。

### **在 Vulkan 中的实现**

在 Vulkan 中，解析操作通常通过 `vkCmdResolveImage` 函数来实现。以下是一个简单的示例，展示如何使用 `vkCmdResolveImage` 将多重采样图像解析到单一采样图像：

```c
// 假设已经创建了 multisampledImage 和 singleSampledImage
VkImageResolve resolveRegion = {};
resolveRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
resolveRegion.srcSubresource.mipLevel = 0;
resolveRegion.srcSubresource.baseArrayLayer = 0;
resolveRegion.srcSubresource.layerCount = 1;
resolveRegion.srcOffset = {0, 0, 0};
resolveRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
resolveRegion.dstSubresource.mipLevel = 0;
resolveRegion.dstSubresource.baseArrayLayer = 0;
resolveRegion.dstSubresource.layerCount = 1;
resolveRegion.dstOffset = {0, 0, 0};
resolveRegion.extent = { width, height, 1 };

// 在命令缓冲区中记录解析命令
vkCmdResolveImage(
    commandBuffer,
    multisampledImage,
    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    singleSampledImage,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    1,
    &resolveRegion
);
```

### **操作步骤说明**

1. **创建多重采样图像和单一采样图像**：
   
   - 多重采样图像用于渲染，具有多个采样点。
   - 单一采样图像用于存储解析后的最终图像。

2. **定义解析区域（VkImageResolve）**：
   
   - 指定源图像（多重采样图像）和目标图像（单一采样图像）的子资源。
   - 设置解析的区域大小和偏移。

3. **记录解析命令**：
   
   - 使用 `vkCmdResolveImage` 将多重采样图像解析到单一采样图像。

4. **提交命令缓冲区**：
   
   - 提交并执行命令缓冲区，完成解析操作。

## **为什么需要解析操作**

多重采样主要用于提高图像质量，减少锯齿现象。然而，最终显示的图像设备（如显示器）只能处理单一采样的图像。因此，在渲染完成后，需要将多重采样图像解析为单一采样图像，以便显示在屏幕上。这一步骤不仅是必要的，还能确保图像质量和性能的优化。

## **总结**

在 Vulkan 中，**"resolved"** 指的是将多重采样缓冲区中的多个采样点合并为单一采样的过程。这一操作通过解析（resolve）实现，使得多重采样渲染的高质量图像能够正确显示在默认的单一采样帧缓冲区中。解析操作不仅是多重采样渲染流程中的关键步骤，也是确保最终图像质量与显示设备兼容的重要环节。
