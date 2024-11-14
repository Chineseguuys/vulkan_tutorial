#ifndef _VERTEX_DEMO_H_
#define _VERTEX_DEMO_H_

#include <vulkan/vulkan_core.h>
#include <glm/glm.hpp>
#include <vector>
#include <array>


namespace ops {

struct Vertex {
    glm::vec3 mPos;
    glm::vec3 mColor;
    glm::vec2 mTexCoord;
    glm::vec3 mNormals;
    uint32_t mMaterialID;

    // 对于希望在 std::unordered_map 中使用的类，需要提供一个 operator==() function 来判断两个实例是否相同
    bool operator==(const Vertex& other) const {
        return mPos == other.mPos && mColor == other.mColor && mTexCoord == other.mTexCoord;
    }

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

    static std::array<VkVertexInputAttributeDescription, 5> getAttributeDescription() {
        std::array<VkVertexInputAttributeDescription, 5> attributeDescriptions{};

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;  // for vec3
        attributeDescriptions[0].offset = offsetof(Vertex, mPos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;   // for vec3
        attributeDescriptions[1].offset = offsetof(Vertex, mColor);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, mTexCoord);

        attributeDescriptions[3].binding = 0;
        attributeDescriptions[3].location = 3;
        attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[3].offset = offsetof(Vertex, mNormals);

        attributeDescriptions[4].binding = 0;
        attributeDescriptions[4].location = 4;
        attributeDescriptions[4].format = VK_FORMAT_R32_SINT;
        attributeDescriptions[4].offset = offsetof(Vertex, mMaterialID);

        return attributeDescriptions;
    }
};

}


#endif