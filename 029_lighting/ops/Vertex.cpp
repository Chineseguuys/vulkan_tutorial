#include "Vertex.h"

namespace ops {

bool Vertex::operator == (const Vertex& other) {
        return mPos == other.mPos && mColor == other.mColor && 
            mTexCoord == other.mTexCoord;
    }

VkVertexInputBindingDescription Vertex::getBindingDescription() {
    VkVertexInputBindingDescription bindDescription{};

    bindDescription.binding = 0;
    bindDescription.stride = sizeof(Vertex);
    bindDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindDescription;
}

std::array<VkVertexInputAttributeDescription, 4> Vertex::getAttributeDescription() {
    std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions{};

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

    return attributeDescriptions;
}

} // end of namespace std