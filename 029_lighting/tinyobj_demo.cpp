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
        spdlog::info("      sharpness: {}", material.diffuse_texopt.sharpness);
        spdlog::info("      brightness: {}", material.diffuse_texopt.brightness);
        spdlog::info("      contrast: {}", material.diffuse_texopt.contrast);
        spdlog::info("      origin offset: [{}, {}, {}]",
            material.diffuse_texopt.origin_offset[0],
            material.diffuse_texopt.origin_offset[1],
            material.diffuse_texopt.origin_offset[2]
        );
        spdlog::info("      scale: [{}, {}, {}]", material.diffuse_texopt.scale[0],
            material.diffuse_texopt.scale[1],
            material.diffuse_texopt.scale[2]
        );
        spdlog::info("      turbulence: [{}, {}, {}]", material.diffuse_texopt.turbulence[0],
            material.diffuse_texopt.turbulence[0],
            material.diffuse_texopt.turbulence[0]
        );
        spdlog::info("  specular texname: {}", material.specular_texname);
        spdlog::info("  specular highlight texname: {}", material.specular_highlight_texname);
        spdlog::info("  bump texname: {}", material.bump_texname);
        spdlog::info("  displacement texname: {}", material.displacement_texname);
        spdlog::info("  aplha texname: {}", material.alpha_texname);
        spdlog::info("  reflection texname: {}", material.reflection_texname);
    }

    for (const auto& shape : shapes) {
        spdlog::info("Shape name: {}", shape.name);
        spdlog::info("  for mesh: ");
        spdlog::info("      vertices indices size: {}", shape.mesh.indices.size());
        spdlog::info("      face vertices: {} [{}, {} ...]", shape.mesh.num_face_vertices.size(),
            shape.mesh.num_face_vertices[0], shape.mesh.num_face_vertices[1]
        );
        spdlog::info("      merticals id size: {} [{}, {} ...]", shape.mesh.material_ids.size(),
            shape.mesh.material_ids[0],
            shape.mesh.material_ids[1]
        );

        spdlog::info("  for line: ");
        spdlog::info("      vertices indices size: {}", shape.lines.indices.size());
        spdlog::info("      points vertices: {}", shape.lines.num_line_vertices.size());

        spdlog::info("  for point: ");
        spdlog::info("      vertices indices size: {}", shape.points.indices.size());
    }

    spdlog::info("Attribute: ");
    spdlog::info("  vertices size: {}", attrib.vertices.size());
    spdlog::info("  vertices weights size: {}", attrib.vertex_weights.size());
    spdlog::info("  normals size: {}", attrib.normals.size());
    spdlog::info("  texcoords size: {}", attrib.texcoords.size());
    spdlog::info("  texcoord_ws size: {}", attrib.texcoord_ws.size());
    spdlog::info("  colors size: {}", attrib.colors.size());
    spdlog::info("  skin_weights size: {}", attrib.skin_weights.size());

    return 0;
}