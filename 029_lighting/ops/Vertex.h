#ifndef _VERTEX_DEMO_
#define _VERTEX_DEMO_

#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <vulkan/vulkan_core.h>

namespace ops {

struct Vertex {
    glm::vec3 mPos;
    glm::vec3 mColor;
    glm::vec2 mTexCoord;
    glm::vec3 mNormals;

    bool operator == (const Vertex& other);

    static VkVertexInputBindingDescription getBindingDescription();

    static std::array<VkVertexInputAttributeDescription, 4> getAttributeDescription();
};

}   /* end namespace ops */

#endif /* _VERTEX_DEMO_ */