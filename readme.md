# 参考链接

https://vulkan-tutorial.com/Introduction

[GitHub - Overv/VulkanTutorial: Tutorial for the Vulkan graphics and compute API](https://github.com/Overv/VulkanTutorial/tree/main)

[GitHub - GavinKG/ILearnVulkanFromScratch-CN: Gitbook repo hosting](https://github.com/GavinKG/ILearnVulkanFromScratch-CN/tree/master)

[Layer : gfxreconstruct](https://github.com/LunarG/gfxreconstruct)

# 操作系统信息

```bash
OS: Manjaro Linux x86_64
Kernel: Linux 6.6.34-1-MANJARO
Uptime: 1 day, 1 hour, 17 mins
DE: KDE Plasma 6.0.5
WM: KWin (Wayland)
CPU: AMD Ryzen 9 5900X (24) @ 3.70 GHz
GPU: AMD Radeon RX 6650 XT @ 0.05 GHz [Discrete]
```

# [gfxreconstruct](https://github.com/LunarG/gfxreconstruct)

可以使用 gfxrecon-convert 将 gfxr 文件转为 json 文件，从 json 文件中可以看到每一次的 vulkan 的 api 的调用和参数信息

```bash
gfxrecon-convert - A tool to convert the contents of GFXReconstruct capture files to JSON.

Usage:
  gfxrecon-convert [-h | --help] [--version] <file>

Required arguments:
  <file>                Path to the GFXReconstruct capture file to be converted
                        to text.

Optional arguments:
  -h                    Print usage information and exit (same as --help).
  --version             Print version information and exit.
  --output file         'stdout' or a path to a file to write JSON output
                        to. Default is the input filepath with "gfxr" replaced by "json".
  --format <format>     JSON format to write.
           json         Standard JSON format (indented)
           jsonl        JSON lines format (every object in a single line)
  --include-binaries    Dump binaries from Vulkan traces in a separate file with an unique name. The main JSON file
                        will include a reference with the file name. The binary files are dumped in a subdirectory
  --expand-flags        Print flags values from Vulkan traces with its correspondent symbolic representation. Otherwise,
                        the flags are printed as hexadecimal value.
  --file-per-frame      Creates a new file for every frame processed. Frame number is added as a suffix
                        to the output file name.
```

# VkBuffer & VkDeviceMemory

在 Vulkan 中，`VkBuffer` 和 `VkDeviceMemory` 代表的是两个不同但紧密相关的概念，分别是数据存储的**容器**和**内存分配**，它们共同作用来存储和管理 GPU 访问的数据。

### 1. **VkBuffer**（缓冲区）

`VkBuffer` 是一种抽象的对象，用于表示一段连续的字节数据，它并不直接包含实际的数据，而是描述了缓冲区的大小、用途（如顶点数据、索引数据或是传输数据的中转等）以及如何与 GPU 进行交互。

- `VkBuffer` 本质上是一个**逻辑对象**，它并不涉及具体的内存分配，而是定义了如何使用该缓冲区。
- `VkBuffer` 可以用于存储各种类型的数据，如顶点、索引、统一缓冲（Uniform Buffer）等。

### 2. **VkDeviceMemory**（设备内存）

`VkDeviceMemory` 是 Vulkan 中用于管理内存分配的对象，代表了实际分配的 GPU 设备内存。你需要将内存分配给缓冲区或者其他 Vulkan 对象（如图像）以使其生效。

- `VkDeviceMemory` 负责**管理内存的分配和释放**，可以分配设备内存并将其绑定到特定的缓冲区或图像。
- `VkDeviceMemory` 可以被多个 `VkBuffer` 或 `VkImage` 对象共享，但这些对象只能访问自己绑定的内存区域。

### 二者的关系

1. **VkBuffer** 是描述数据的逻辑结构，而 **VkDeviceMemory** 是实际存储数据的内存。
2. 创建一个 `VkBuffer` 后，还需要为其分配实际的内存（`VkDeviceMemory`），这可以通过 `vkAllocateMemory` 分配内存，然后使用 `vkBindBufferMemory` 将内存绑定到缓冲区上。
3. 多个 `VkBuffer` 可以绑定到同一个 `VkDeviceMemory` 对象中，只要它们使用不同的内存偏移量。

### 代码示例

```c
// 创建一个 VkBuffer
VkBufferCreateInfo bufferInfo = {};
bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
bufferInfo.size = bufferSize;  // 缓冲区大小
bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT; // 缓冲区用途
bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;  // 独占模式

VkBuffer buffer;
vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);

// 获取缓冲区的内存需求
VkMemoryRequirements memRequirements;
vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

// 分配 VkDeviceMemory
VkMemoryAllocateInfo allocInfo = {};
allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
allocInfo.allocationSize = memRequirements.size;
allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

VkDeviceMemory bufferMemory;
vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory);

// 将分配的内存绑定到缓冲区
vkBindBufferMemory(device, buffer, bufferMemory, 0);
```

### 总结

- `VkBuffer` 是一种描述数据的结构，不涉及实际内存。
- `VkDeviceMemory` 则是实际的设备内存分配，通过绑定操作，`VkBuffer` 使用 `VkDeviceMemory` 中的内存。

两者结合后，GPU 才能访问 `VkBuffer` 中的数据。
