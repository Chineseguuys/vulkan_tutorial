# binding

```cpp
static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};

        // The binding parameter specifies the index of the binding in the array of bindings
        // 你可能有多组 vertex 数据分别绘制不同的物品(例如 A 用来绘制三角形， B 用来绘制圆形等等)
        // gpu 在绘制的时候，可以同时绑定多组 vertex 数据，每一组的数据需要一个 binding 点，值从 0 开始
        // gpu 支持的同时绑定的 vertex 组的最大数量可以通过 vkGetPhysicalDeviceProperties() api 来进行查询
        bindingDescription.binding = 0;
        // 数据的 stride
        // The stride parameter specifies the number of bytes from one entry to the next
        bindingDescription.stride = sizeof(Vertex);
        // 查找下一个数据的时机，在遍历每一个节点的时候，就查找下一个数据
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }
```

在 Vulkan 中，可以通过查询物理设备的属性来获取 GPU 支持的最大顶点输入绑定数。具体来说，你需要查询 `VkPhysicalDeviceLimits` 结构体中的 `maxVertexInputBindings` 成员变量。这个值表示 GPU 支持的最大顶点输入绑定数，即顶点输入缓冲区（`VkVertexInputBindingDescription`）的最大数量。

以下是查询该值的步骤：

1. 首先，获取物理设备的属性。
2. 从属性中获取 `VkPhysicalDeviceLimits`，再从中获取 `maxVertexInputBindings`。

在我的计算机上，可以通过调试查到这个值为 32

```bash
(lldb) print deviceProperties.limits.maxVertexInputBinding
Available completions:
        deviceProperties.limits.maxVertexInputBindings     
        deviceProperties.limits.maxVertexInputBindingStride
(lldb) print deviceProperties.limits.maxVertexInputBindings
(uint32_t) 32
```

# memRequirements

实际 DeviceMemory 需要创建的大小和结构体的大小不一定一致。

```cpp
VkMemoryRequirements memRequirements;
vkGetBufferMemoryRequirements(mDevice, mVertexBuffer, &memRequirements);

VkMemoryAllocateInfo allocInfo{};
allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
allocInfo.allocationSize = memRequirements.size;
```

```c
(lldb) print memRequirements 
(VkMemoryRequirements)  (size = 64, alignment = 16, memoryTypeBits = 1965)
(lldb) print bufferInfo 
(VkBufferCreateInfo) {
  sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO
  pNext = 0x0000000000000000
  flags = 0
  size = 60
  usage = 128
  sharingMode = VK_SHARING_MODE_EXCLUSIVE
  queueFamilyIndexCount = 0
  pQueueFamilyIndices = 0x0000000000000000
}
```


