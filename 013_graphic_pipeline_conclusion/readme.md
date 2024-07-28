# Conclusion

现在，我们可以将前几章中的所有结构和对象组合起来，创建图形管道！ 以下是我们现在拥有的对象类型，简单回顾一下

- Shader stages: the shader modules that define the functionality of the programmable stages of the graphics pipeline

- Fixed-function state: all of the structures that define the fixed-function stages of the pipeline, like input assembly, rasterizer, viewport and color blending

- Pipeline layout: the uniform and push values referenced by the shader that can be updated at draw time

- Render pass: the attachments referenced by the pipeline stages and their usage

> - 着色器阶段：定义了着色器模块用于图形管线哪一可编程阶段
> 
> - 固定功能状态：定义了图形管线的固定功能阶段使用的状态信息，比如输入装配，视口，光栅化，颜色混合
> 
> - 管线布局：定义了被着色器使用，在渲染时可以被动态修改的uniform变量
> 
> - 渲染流程：定义了被管线使用的附着的用途



上面所有这些信息组合起来完整定义了图形管线的功能，现在，我们可以开始填写VkGraphicsPipelineCreateInfo结构体来创建管线对象。我们需要在createGraphicsPipeline函数的尾部，vkDestroyShaderModule函数调用之前添加下面的代码：

```c
// create graphic pipeline
VkGraphicsPipelineCreateInfo pipelineInfo{};
pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
// 我们只定义了顶点着色器和片段着色器这两个阶段
pipelineInfo.stageCount = 2;
pipelineInfo.pStages = shaderStages;
```

我们首先引用 `VkPipelineShaderStageCreateInfo` 结构数组。然后，我们引用描述固定功能(fixed functions)阶段的所有结构。

```c
pipelineInfo.pVertexInputState = &vertexInputInfo;
pipelineInfo.pInputAssemblyState = &inputAssembly;
pipelineInfo.pViewportState = &viewportState;
pipelineInfo.pRasterizationState = &rasterizer;
pipelineInfo.pMultisampleState = &multisampling;
pipelineInfo.pDepthStencilState = nullptr;
pipelineInfo.pColorBlendState = &colorBlending;
pipelineInfo.pDynamicState = &dynamicState;
```

之后是管道布局，它是一个 Vulkan 句柄，而不是结构指针。

```c
pipelineInfo.layout = mPipelineLayout;
```

最后是渲染通道(render pass)的引用，以及将使用此图形管道的子通道(sub pass)的索引。 我们也可以在此管道中使用其他渲染通道，而不是此特定实例，但它们必须与 renderPass 兼容。 这里描述了对兼容性的要求，但我们在本教程中不会使用该功能。

```c
pipelineInfo.renderPass = mRenderPass;
pipelineInfo.subpass = 0;
```

实际上还有两个参数：`basePipelineHandle` 和 `basePipelineIndex`。 Vulkan 允许通过派生现有管道来创建新的图形管道。 管道派生的原理是，当管道与现有管道有很多共同功能时，建立管道的成本会更低，而且在同一父管道之间切换也会更快。 你既可以用 `basePipelineHandle` 指定现有管道的句柄，也可以用 `basePipelineIndex` 引用另一个即将创建的管道索引。 现在只有一个管道，所以我们只需指定一个空句柄和一个无效索引。 只有在 `VkGraphicsPipelineCreateInfo` 的 flags 字段中也指定了 `VK_PIPELINE_CREATE_DERIVATIVE_BIT` 标志时，才会使用这些值。

```c
pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
pipelineInfo.basePipelineIndex = -1;
```

最后创建 graphic pipeline:

```c

if (vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &pipelineInfo,
      nullptr, &mGraphicsPipeline) != VK_SUCCESS) {
      spdlog::error("{} failed to create graphics pipeline", __func__);
      throw std::runtime_error("failed to create graphics pipeline");
}
```

`vkCreateGraphicsPipelines` 函数实际上比 Vulkan 中的常规对象创建函数拥有更多参数。 该函数设计用于接收多个 `VkGraphicsPipelineCreateInfo` 对象，并在一次调用中创建多个 VkPipeline 对象。

第二个参数（我们为其传递了 `VK_NULL_HANDLE` 参数）引用了一个可选的 `VkPipelineCache` 对象。 管道缓存可用于存储与管道创建相关的数据，并在多次调用 `vkCreateGraphicsPipelines` 时重复使用，如果缓存存储在文件中，甚至可以在程序执行时重复使用。 这样就有可能大大加快以后管道创建的速度。 我们将在管道缓存一章中讨论这个问题。
