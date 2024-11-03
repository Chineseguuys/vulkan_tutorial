#ifndef _SHAPE_DEMO_
#define _SHAPE_DEMO_

#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <vulkan/vulkan_core.h>

#include <spdlog/spdlog.h>
#include "Vertex.h"

namespace ops {

struct Shape {
    enum vertex_type {
        TYPE_POINT = 1,
        TYPE_LINE = 2,
        TYPE_MESH = 3
    };

    std::vector<Vertex> mVertices;
    int mMaterialID;    // only for mesh?

    virtual vertex_type getVertexType();
}; /* end Shape*/


} // end of namespace std


#endif /* _SHAPE_DEMO_ */