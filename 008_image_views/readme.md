# debug 信息

```
(lldb) print createInfo 
(VkImageViewCreateInfo) {
  sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO
  pNext = 0x0000000000000000
  flags = 0
  image = 0xe7f79a0000000005
  viewType = VK_IMAGE_VIEW_TYPE_2D
  format = VK_FORMAT_B8G8R8A8_SRGB
  components = (r = VK_COMPONENT_SWIZZLE_IDENTITY, g = VK_COMPONENT_SWIZZLE_IDENTITY, b = VK_COMPONENT_SWIZZLE_IDENTITY, a = VK_COMPONENT_SWIZZLE_IDENTITY)
  subresourceRange = {
    aspectMask = 1
    baseMipLevel = 0
    levelCount = 1
    baseArrayLayer = 0
    layerCount = 1
  }
}
```
