# VkImageCreateInfo.arrayLayers

`VkImageCreateInfo` 结构体中的 `arrayLayers` 字段用于指定 Vulkan 中图像（`VkImage`）的**数组层数**，即该图像包含的层的数量。这个字段在创建 2D 图像数组或 3D 图像等多层图像时尤其重要。

### `arrayLayers` 字段的含义：

- **`arrayLayers`** 定义了图像的**层数**，也就是图像的不同切片或层的数量。
- **适用情况**：
  - 当图像类型是 1D 或 2D 且 `arrayLayers` 大于 1 时，它代表一个图像数组，即多个相同尺寸的 1D 或 2D 图像组合成一个数组。
  - 对于 3D 图像，`arrayLayers` 必须始终为 1，因为 3D 图像本身包含多个深度切片（由 `depth` 字段指定），不需要额外的数组层。
  - 对于**立方体贴图**，`arrayLayers` 必须是 6 的倍数，且通常为 6，因为立方体贴图有 6 个面（如果是立方体图像数组，则为 6 的倍数，如 12、18 等）。

### `arrayLayers` 的设置方法：

1. **1D 图像或 2D 图像（非数组）**：
   如果你要创建一个普通的 1D 或 2D 图像，`arrayLayers` 应该设置为 1，表示只有一层。
   
   ```c
   VkImageCreateInfo imageInfo = {};
   imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
   imageInfo.imageType = VK_IMAGE_TYPE_2D;  // 2D 图像
   imageInfo.extent.width = 1024;
   imageInfo.extent.height = 1024;
   imageInfo.extent.depth = 1;
   imageInfo.mipLevels = 1;
   imageInfo.arrayLayers = 1;               // 只有一层
   ```

2. **1D 或 2D 图像数组**：
   如果你要创建一个 1D 或 2D 图像数组，`arrayLayers` 应设置为数组中的图像数量。例如，要创建一个包含 4 张 2D 图像的数组：
   
   ```c
   VkImageCreateInfo imageInfo = {};
   imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
   imageInfo.imageType = VK_IMAGE_TYPE_2D;  // 2D 图像数组
   imageInfo.extent.width = 1024;
   imageInfo.extent.height = 1024;
   imageInfo.extent.depth = 1;
   imageInfo.mipLevels = 1;
   imageInfo.arrayLayers = 4;               // 4 层（图像数组）
   ```

3. **3D 图像**：
   对于 3D 图像，`arrayLayers` 必须设置为 1，因为 3D 图像的深度已经通过 `extent.depth` 来指定多层。
   
   ```c
   VkImageCreateInfo imageInfo = {};
   imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
   imageInfo.imageType = VK_IMAGE_TYPE_3D;  // 3D 图像
   imageInfo.extent.width = 1024;
   imageInfo.extent.height = 1024;
   imageInfo.extent.depth = 16;             // 16 个深度切片
   imageInfo.mipLevels = 1;
   imageInfo.arrayLayers = 1;               // 对于 3D 图像必须为 1
   ```

4. **立方体贴图**：
   当创建立方体贴图时，`arrayLayers` 通常为 6，因为立方体贴图有 6 个面。如果你要创建立方体贴图数组，`arrayLayers` 应为 6 的倍数。
   
   ```c
   VkImageCreateInfo imageInfo = {};
   imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
   imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;  // 立方体贴图
   imageInfo.imageType = VK_IMAGE_TYPE_2D;  // 立方体贴图仍然是 2D 图像
   imageInfo.extent.width = 1024;
   imageInfo.extent.height = 1024;
   imageInfo.extent.depth = 1;
   imageInfo.mipLevels = 1;
   imageInfo.arrayLayers = 6;               // 6 层（立方体贴图的 6 个面）
   ```
   
   如果你要创建**立方体贴图数组**，例如包含 2 个立方体贴图，则 `arrayLayers` 应设置为 12（6 × 2）。
   
   ```c
   imageInfo.arrayLayers = 12;              // 立方体贴图数组：2 个立方体图像，共 12 层
   ```

### 总结：

- `arrayLayers` 指定了图像的层数。
  - 对于 1D 和 2D 图像，可以设置为大于 1 来创建图像数组。
  - 对于 3D 图像，`arrayLayers` 必须为 1。
  - 对于立方体贴图，`arrayLayers` 应设置为 6 的倍数（通常为 6）。

# VkImageCreateInfo.tiling

在 Vulkan 中，`VkImageCreateInfo` 结构体中的 `tiling` 字段用于指定图像在内存中的**布局方式**，即图像数据在 GPU 内存中的排列方式。`tiling` 字段通过 `VkImageTiling` 枚举来表示，主要有两种常见的值：

### `tiling` 字段的选项：

1. **`VK_IMAGE_TILING_OPTIMAL`**：**最佳tiling**布局，表示图像数据的布局由 GPU 和驱动程序决定，优化为设备（GPU）的最佳性能。
2. **`VK_IMAGE_TILING_LINEAR`**：**线性tiling**布局，表示图像数据在内存中是线性存储的，类似于 CPU 可访问的内存布局。这种布局更适合 CPU 的直接访问和读写。

### 两种 `tiling` 布局的区别：

1. **`VK_IMAGE_TILING_OPTIMAL`**：
   
   - 数据在内存中以最适合 GPU 的方式存储，性能最佳。
   - 一般用于需要进行大量 GPU 操作的图像，如纹理、帧缓冲区等。
   - GPU 可以以任意顺序访问像素数据，且通常是跨行、跨块存储的，这有利于提高 GPU 的缓存和并行操作效率。
   - **缺点**：在 `VK_IMAGE_TILING_OPTIMAL` 下，图像数据对 CPU 通常是不可直接访问的，或者访问性能较差。因此，如果你需要使用 CPU 直接读取或写入图像数据，使用这种 tiling 并不合适。

2. **`VK_IMAGE_TILING_LINEAR`**：
   
   - 图像数据在内存中按行逐行线性存储，适合 CPU 直接读取或写入。
   - 一般用于 CPU 频繁访问的图像数据，如需要进行 CPU 端拷贝或处理的图像。
   - **缺点**：`VK_IMAGE_TILING_LINEAR` 的性能通常不如 `VK_IMAGE_TILING_OPTIMAL`，尤其是当 GPU 频繁访问图像时。因为线性布局通常不能很好地利用 GPU 的缓存和并行处理能力。

### 如何使用 `tiling` 字段：

在创建 Vulkan 图像时，需要根据图像的使用场景和性能需求来选择合适的 `tiling`。这个字段是在 `VkImageCreateInfo` 结构体中设置的：

```c
VkImageCreateInfo imageInfo = {};
imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
imageInfo.imageType = VK_IMAGE_TYPE_2D;
imageInfo.extent.width = 1024;
imageInfo.extent.height = 1024;
imageInfo.extent.depth = 1;
imageInfo.mipLevels = 1;
imageInfo.arrayLayers = 1;
imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;  // 设置为最佳tiling
imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
```

### 什么时候使用 `VK_IMAGE_TILING_OPTIMAL`？

- 当图像的主要操作是通过 GPU 进行时，比如纹理、帧缓冲区或者离屏渲染目标。
- `VK_IMAGE_TILING_OPTIMAL` 通常会用于采样（`VK_IMAGE_USAGE_SAMPLED_BIT`）或者作为渲染目标（`VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT` 或 `VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT`）。

### 什么时候使用 `VK_IMAGE_TILING_LINEAR`？

- 当图像需要频繁通过 CPU 访问，或者图像初始化时数据来自 CPU。
- 例如，当你需要用 CPU 生成或修改图像数据，并将其传递给 GPU 时，线性 tiling 更适合。
- 如果你需要直接读取从 GPU 渲染得到的图像数据（比如读取帧缓冲内容），线性 tiling 也更方便 CPU 访问。

```c
VkImageCreateInfo imageInfo = {};
imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
imageInfo.imageType = VK_IMAGE_TYPE_2D;
imageInfo.extent.width = 1024;
imageInfo.extent.height = 1024;
imageInfo.extent.depth = 1;
imageInfo.mipLevels = 1;
imageInfo.arrayLayers = 1;
imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
imageInfo.tiling = VK_IMAGE_TILING_LINEAR;  // 设置为线性tiling
imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;  // 例如用于 CPU 读取操作
```

### 注意事项：

- **内存类型选择**：图像的 tiling 方式会影响 Vulkan 需要分配的内存类型。通常，`VK_IMAGE_TILING_LINEAR` 需要用可以被 CPU 访问的内存，而 `VK_IMAGE_TILING_OPTIMAL` 则需要专门为 GPU 优化的内存类型。
- **查询支持的 tiling**：你可以通过 `vkGetPhysicalDeviceImageFormatProperties` 查询某种格式在某种 tiling 下是否被支持。例如，有些格式可能只支持 `VK_IMAGE_TILING_OPTIMAL`。

```c
VkImageFormatProperties properties;
VkResult result = vkGetPhysicalDeviceImageFormatProperties(
    physicalDevice, format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_LINEAR,
    VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 0, &properties);
```

### 总结：

- `tiling` 字段指定 Vulkan 图像的内存布局方式。
- `VK_IMAGE_TILING_OPTIMAL` 适用于高效的 GPU 访问，性能最佳，但不利于 CPU 直接访问。
- `VK_IMAGE_TILING_LINEAR` 适用于 CPU 直接读取或写入，但 GPU 性能不如最佳布局。
- 根据图像的使用场景（CPU 或 GPU 访问）选择合适的 tiling，以获得最佳性能和功能。

# VK_IMAGE_USAGE_SAMPLED_BIT

`VK_IMAGE_USAGE_SAMPLED_BIT` 是 Vulkan 中 `VkImageUsageFlagBits` 枚举的一部分，用于指定一个图像（`VkImage`）在创建时的使用场景。

### 含义：

`VK_IMAGE_USAGE_SAMPLED_BIT` 表示该图像将被用作**采样器**中的纹理，允许它被着色器（shader）读取。简单来说，当你将一个图像用于着色器中的**纹理采样**操作时，你需要为该图像设置 `VK_IMAGE_USAGE_SAMPLED_BIT` 标志。

在 Vulkan 中，图像有多种用途，如渲染目标、传输目标、纹理等。`VK_IMAGE_USAGE_SAMPLED_BIT` 特别适用于将图像作为**采样器**（`VkSampler`）的一部分，通过采样器从图像中读取像素数据。

### 何时使用 `VK_IMAGE_USAGE_SAMPLED_BIT`：

- 当你希望在着色器中使用**纹理采样**来获取图像数据时，需要为图像设置 `VK_IMAGE_USAGE_SAMPLED_BIT`。
- 例如，渲染场景中的纹理贴图（如物体表面的颜色、法线贴图等）通常会被采样器读取，这些图像都需要启用 `VK_IMAGE_USAGE_SAMPLED_BIT`。

### 使用示例：

在创建图像时，指定 `VK_IMAGE_USAGE_SAMPLED_BIT`，表明该图像可以在着色器中作为采样器的一部分被读取。

```c
VkImageCreateInfo imageInfo = {};
imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
imageInfo.imageType = VK_IMAGE_TYPE_2D;       // 2D 图像
imageInfo.extent.width = 1024;
imageInfo.extent.height = 1024;
imageInfo.extent.depth = 1;
imageInfo.mipLevels = 1;                      // 单一 mipmap 级别
imageInfo.arrayLayers = 1;                    // 单层
imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;  // 格式
imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT; // 作为纹理被采样
```

### 使用场景：

- **纹理采样**：如果图像用于存储场景中的纹理（如颜色贴图、法线贴图等），`VK_IMAGE_USAGE_SAMPLED_BIT` 是必要的。
- **环境贴图**：对于立方体贴图或反射、折射等环境映射场景，图像也需要被着色器采样，因此需要设置该标志。
- **自定义渲染效果**：如果你需要从图像中读取像素数据来实现某种自定义效果，如后处理效果（如模糊、阴影映射等），也需要用到 `VK_IMAGE_USAGE_SAMPLED_BIT`。

### 与其他标志的组合：

`VK_IMAGE_USAGE_SAMPLED_BIT` 可以与其他图像使用标志组合。例如，你可能需要将图像用作渲染目标，同时也用作着色器中的采样器。在这种情况下，你可以组合 `VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT` 和 `VK_IMAGE_USAGE_SAMPLED_BIT`：

```c
imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
```

### 总结：

- **`VK_IMAGE_USAGE_SAMPLED_BIT`** 用于指示图像可以被着色器中的采样器读取（即作为纹理使用）。
- 当你在 Vulkan 中使用图像作为纹理时，必须设置此标志。
- 它通常与其他使用标志组合，特别是在需要将图像既作为渲染目标又作为纹理时。


