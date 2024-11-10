#ifndef _SHAPE_DEMO_H_
#define _SHAPE_DEMO_H_

#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>
#include "Verterx.h"

namespace ops {

struct Shape {
    enum Shape_Type{
        TYPE_POINT,
        TYPE_LINE,
        TYPE_MESH
    };
    std::string mName;
    uint32_t mOffset;
    std::vector<uint32_t> mIndices;
};


struct Shape_Mesh : public Shape {
    uint32_t  mMeterial_ID; // mesh 需要使用的纹理
};

}

#endif