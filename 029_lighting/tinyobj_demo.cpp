#include "tiny_obj_loader.cc"
#include <vector>
#include <string>
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include "stb_image.h"

#include <spdlog/spdlog.h>


struct Vertex {
    glm::vec3 mPos;
    glm::vec3 mColor;
    glm::vec2 mTexCoord;
    glm::vec3 mNormals;
};


const std::string obj_path = "./models/final.obj";
const std::string mtl_path = "./models/final.mtl";


int main(int argc, char* argv[]) {
    tinyobj::ObjReaderConfig readerConfig;
    readerConfig.mtl_search_path = "./models/";

    tinyobj::ObjReader objReaderInstance;
    if (!objReaderInstance.ParseFromFile(obj_path, readerConfig)) {
        if (!objReaderInstance.Error().empty()) {
            spdlog::error("Error: {}", objReaderInstance.Error());
        }
        return 1;
    }

    if (!objReaderInstance.Warning().empty()) {
        spdlog::warn("Warning: {}", objReaderInstance.Warning());
    }

    const tinyobj::attrib_t &attrib = objReaderInstance.GetAttrib();
    const std::vector<tinyobj::shape_t> &shapes = objReaderInstance.GetShapes();
    const std::vector<tinyobj::material_t> &materials = objReaderInstance.GetMaterials();

    for (const auto& material : materials) {
        spdlog::info("Material name: {}", material.name);
        spdlog::info("  ambient texname: {}", material.ambient_texname);
        spdlog::info("  diffuse texname: {}", material.diffuse_texname);
        spdlog::info("  specular texname: {}", material.specular_texname);
        spdlog::info("  specular highlight texname: {}", material.specular_highlight_texname);
        spdlog::info("  bump texname: {}", material.bump_texname);
        spdlog::info("  displacement texname: {}", material.displacement_texname);
        spdlog::info("  aplha texname: {}", material.alpha_texname);
        spdlog::info("  reflection texname: {}", material.reflection_texname);
    }

    for (const auto& shape : shapes) {
        spdlog::info("Shape name: {}", shape.name);
    }

    return 0;
}