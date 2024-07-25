# Fixed functions

旧版图形应用程序接口为图形管道的大部分阶段提供了默认状态。 在 Vulkan 中，您必须明确大多数流水线状态，因为这些状态将被嵌入到不可变的流水线状态对象中。 在本章中，我们将填充配置这些固定功能操作的所有结构。

## Dynamic state

虽然大部分流水线状态都需要固定到流水线状态中，但实际上有少量状态可以在绘制时无需重新创建流水线而进行更改。 例如视口大小(size of the viewport)、线宽(line width)和混合常量(blend constants)。 如果你想使用动态状态并不包含这些属性，那么你就必须像这样填写一个 `VkPipelineDynamicStateCreateInfo` 结构。

```c
std::vector<VkDynamicState> dynamicStates = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR
};

VkPipelineDynamicStateCreateInfo dynamicState{};
dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
dynamicState.pDynamicStates = dynamicStates.data();
```

这将导致这些值的配置被忽略，而您可以（并需要）在绘制时指定数据。 这将使设置更加灵活，对于视口和剪刀状态等情况来说也很常见，而在管道状态下，这些设置将变得更加复杂。

## Vertex input

`VkPipelineVertexInputStateCreateInfo` 结构描述了将传递给顶点着色器的顶点数据格式。 其描述方式大致有两种：

- 绑定：数据之间的间距，以及数据是按顶点还是按实例

- 属性描述：传递给顶点着色器的属性类型，从哪个绑定以及在哪个偏移量加载这些属性

由于我们直接在顶点着色器中对顶点数据进行硬编码，因此我们将在此结构中填入顶点数据，以指定暂时没有顶点数据需要加载。 我们将在顶点缓冲章节中继续讨论。

```c
//Vertex state
VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
vertexInputInfo.vertexBindingDescriptionCount = 0;
vertexInputInfo.pVertexBindingDescriptions = nullptr;
vertexInputInfo.vertexAttributeDescriptionCount = 0;
vertexInputInfo.pVertexAttributeDescriptions = nullptr;
```

`pVertexBindingDescriptions`（顶点绑定描述）和 `pVertexAttributeDescriptions`（顶点属性描述）成员指向一个结构数组，该数组用于描述上述加载顶点数据的细节。 将此结构添加到 `createGraphicsPipeline` 函数中，紧跟在 `shaderStages` 数组之后。

## Input assembly

`VkPipelineInputAssemblyStateCreateInfo` 结构描述了两件事：将从顶点绘制何种几何图形，以及是否应启用基元重启。 前者在拓扑结构成员中指定，其值可以是:

- VK_PRIMITIVE_TOPOLOGY_POINT_LIST：来自顶点的点 - 

- VK_PRIMITIVE_TOPOLOGY_LINE_LIST：来自每 2 个顶点的直线，不重复使用 

- VK_PRIMITIVE_TOPOLOGY_LINE_STRIP：每条直线的末端顶点用作下一条直线的起始顶点 

- VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST： 每 3 个顶点的三角形，不重复使用

- VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP: 每个三角形的第二个和第三个顶点都用作下一个三角形的前两个顶点

通常情况下，顶点是按索引顺序从顶点缓冲区(vertex buffer)加载的，但在元素缓冲区(elements buffer)中，你可以自己指定要使用的索引。 这样就可以执行重复使用顶点等优化操作。 如果将 primitiveRestartEnable 成员设置为 VK_TRUE，就可以在 _STRIP 拓扑模式中使用 0xFFFF 或 0xFFFFFFFF 的特殊索引来分割线条和三角形。

```c
// input assembly
VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
// 每 3 个顶点绘制一个三角形，不重复使用三角形
inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
// 在使用索引绘制时，图元重启允许使用特定的索引值来表示一个图元的结束和下一个图元的开始
inputAssembly.primitiveRestartEnable = VK_FALSE;
```

## Viewports and scissors

视口(viewports)基本上描述了输出将渲染到的帧缓冲区域。 视口几乎总是（0，0）到（宽，高），在本教程中也是如此。

```c
VkViewport viewport{};
viewport.x = 0.0f;
viewport.y = 0.0f;
viewport.width = (float) swapChainExtent.width;
viewport.height = (float) swapChainExtent.height;
viewport.minDepth = 0.0f;
viewport.maxDepth = 1.0f;
```

请记住，交换链及其图像的大小可能与窗口的宽度和高度不同。 交换链图像稍后将用作帧缓冲区，因此我们应严格控制其大小。

`minDepth` 和 `maxDepth` 值指定了用于帧缓冲的深度值范围。 这些值必须在 $[0.0f, 1.0f] $范围内，但 `minDepth` 可以大于 `maxDepth`。 如果您没有做任何特别的事情，那么您应该坚持使用 0.0f 和 1.0f 的标准值。

视口(viewports)定义了从图像到帧缓冲区的转换，而剪刀矩形(scissors)则定义了实际存储像素的区域。 任何超出剪刀矩形的像素都将被光栅化器丢弃。 它们的功能类似于滤镜而非变换。 区别如下图所示。 请注意，左剪刀矩形只是产生该图像的多种可能性之一，只要它大于视口即可。

![](viewports_scissors.png)

因此，如果我们想要绘制整个帧缓冲区，就需要指定一个完全覆盖帧缓冲区的剪刀矩形：

```c
VkRect2D scissor{};
scissor.offset = {0, 0};
scissor.extent = swapChainExtent;
```

视口(viewports)和剪刀矩形(scissor rectangle)既可以指定为流水线的静态部分，也可以指定为命令缓冲区中的动态状态。 虽然前者与其他状态更为一致，但将视口和剪刀状态设置为动态往往更为方便，因为它能为你提供更多的灵活性。 这种情况非常普遍，所有实现都可以处理这种动态状态，而不会影响性能。

选择动态视口和剪刀矩形时，需要为管道启用相应的动态状态：

```c
std::vector<VkDynamicState> dynamicStates = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR
};

VkPipelineDynamicStateCreateInfo dynamicState{};
dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
dynamicState.pDynamicStates = dynamicStates.data();
```

然后，您只需在创建管道时指定它们的数量：

```c
VkPipelineViewportStateCreateInfo viewportState{};
viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
viewportState.viewportCount = 1;
viewportState.scissorCount = 1;
```

## Rasterizer

光栅化器(rasterizer)从顶点着色器(vertex shader)中获取由顶点塑造的几何体，并将其转化为片段(fragments)，由片段着色器着色(fragment shader)。 它还会执行深度测试、面剔除和剪刀测试，并可配置为输出填充整个多边形或仅填充边缘的片段（线框渲染）。 所有这些都可以通过 `VkPipelineRasterizationStateCreateInfo` 结构进行配置。

```c
// Rasterizer
VkPipelineRasterizationStateCreateInfo rasterizer{};
rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
rasterizer.depthClampEnable = VK_FALSE;
```

如果将 `depthClampEnable` 设置为 `VK_TRUE`，那么超出近平面(near planes)和远平面(far planes)的片段就会被拍到近平面或者远平面上，而不会被丢弃。 这在阴影贴图等特殊情况下非常有用。 使用此功能需要启用 GPU 功能。

`polygonMode` 决定几何体生成碎片的方式。 可用的模式如下： 

- VK_POLYGON_MODE_FILL：用片段填充多边形区域

- VK_POLYGON_MODE_LINE：多边形边缘绘制为线条

- VK_POLYGON_MODE_POINT：多边形顶点绘制为点

```c
rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
```

lineWidth 成员非常简单，它以片段数来描述线条的粗细。 支持的最大线宽取决于硬件，任何粗于 1.0f 的线条都需要启用宽线 GPU 功能。

```c
rasterizer.lineWidth = 1.0f;
```

cullMode 变量决定了要使用的面剔除类型。 您可以禁用剔除、剔除正面、剔除背面或两者兼而有之。 frontFace 变量用于指定将面视为正面的顶点顺序，可以是顺时针或逆时针。

```c
rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
```

## Multisampling

`VkPipelineMultisampleStateCreateInfo` 结构用于配置多重采样，这是执行抗锯齿的方法之一。 它的工作原理是将光栅化为同一像素的多个多边形的片段着色器结果结合起来。 这主要发生在边缘，也是最明显的混叠伪影出现的地方。 如果只有一个多边形映射到一个像素，就不需要多次运行片段着色器，因此它比简单地渲染到更高分辨率然后再降频的成本要低得多。 启用它需要启用 GPU 功能。

```c
VkPipelineMultisampleStateCreateInfo multisampling{};
multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
multisampling.sampleShadingEnable = VK_FALSE;
multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
multisampling.minSampleShading = 1.0f; // Optional
multisampling.pSampleMask = nullptr; // Optional
multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
multisampling.alphaToOneEnable = VK_FALSE; // Optional
```

## Color blending

片段着色器返回颜色后，需要将其与帧缓冲器中已有的颜色相结合。 这种转换被称为颜色混合，有两种方法可以实现： 

- 混合新旧值生成最终颜色 

- 使用位操作合并新旧值

有两种类型的结构用于配置色彩混合。 第一种结构（VkPipelineColorBlendAttachmentState）包含每个附加帧缓冲的配置，第二种结构（VkPipelineColorBlendStateCreateInfo）包含全局色彩混合设置。 在本例中，我们只有一个帧缓冲器：

```c
VkPipelineColorBlendAttachmentState colorBlendAttachment{};
colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
colorBlendAttachment.blendEnable = VK_FALSE;
colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional
```

通过此每帧缓冲区结构，您可以配置第一种颜色混合方式。 下面的伪代码可以很好地演示将要执行的操作：

```c
if (blendEnable) {
    finalColor.rgb = (srcColorBlendFactor * newColor.rgb) <colorBlendOp> (dstColorBlendFactor * oldColor.rgb);
    finalColor.a = (srcAlphaBlendFactor * newColor.a) <alphaBlendOp> (dstAlphaBlendFactor * oldColor.a);
} else {
    finalColor = newColor;
}

finalColor = finalColor & colorWriteMask;
```

如果 `blendEnable` 设置为 `VK_FALSE`，那么来自片段着色器的新颜色将不加修改地通过。 否则，将执行两次混合操作来计算新颜色。 生成的颜色会与 `colorWriteMask` 进行 `AND` 运算，以确定哪些通道会实际通过。



第二个结构引用所有帧缓冲区的结构数组，并允许设置混合常数，在上述计算中用作混合因子。

```c
VkPipelineColorBlendStateCreateInfo colorBlending{};
colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
colorBlending.logicOpEnable = VK_FALSE;
colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
colorBlending.attachmentCount = 1;
colorBlending.pAttachments = &colorBlendAttachment;
colorBlending.blendConstants[0] = 0.0f; // Optional
colorBlending.blendConstants[1] = 0.0f; // Optional
colorBlending.blendConstants[2] = 0.0f; // Optional
colorBlending.blendConstants[3] = 0.0f; // Optional
```

如果要使用第二种混合方法（位运算组合），则应将 `logicOpEnable` 设置为 `VK_TRUE`。 然后可以在 `logicOp` 字段中指定位操作。 请注意，这将自动禁用第一个方法，就好像您将每个附加帧缓冲的 `blendEnable` 设置为 `VK_FALSE`！ 在此模式下，还将使用 `colorWriteMask` 来确定帧缓冲中哪些通道将受到实际影响。 也可以禁用这两种模式，就像我们在这里所做的那样，在这种情况下，片段颜色将不加修改地写入帧缓冲器。



## Pipeline layout

You can use `uniform` values in shaders, which are globals similar to dynamic state variables that can be changed at drawing time to alter the behavior of your shaders without having to recreate them. They are commonly used to pass the transformation matrix to the vertex shader, or to create texture samplers in the fragment shader.

These uniform values need to be specified during pipeline creation by creating a [`VkPipelineLayout`](https://www.khronos.org/registry/vulkan/specs/1.0/man/html/VkPipelineLayout.html) object. Even though we won't be using them until a future chapter, we are still required to create an empty pipeline layout.

Create a class member to hold this object, because we'll refer to it from other functions at a later point in time:

当前的教程，所有的数据都写在 glsl 当中，所以不需要使用 uniform 去传递顶点和颜色数据

```c
VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
pipelineLayoutInfo.setLayoutCount = 0; // Optional
pipelineLayoutInfo.pSetLayouts = nullptr; // Optional
pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
    throw std::runtime_error("failed to create pipeline layout!");
}
```


