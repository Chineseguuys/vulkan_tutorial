#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormals;

layout(location = 0) out vec4 outColor;

// 纹理数组？
layout(binding = 1) uniform sampler2D texSampler[3];
// 选择哪一个纹理？
layout(binding = 2) uniform UBOIndex {
    int u_samplerIndex;
} selectSampler;

void main() {
    outColor = texture(texSampler[selectSampler.u_samplerIndex], fragTexCoord, 1.0);
}