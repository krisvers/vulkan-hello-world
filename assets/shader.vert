#version 450

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec2 aUV;

layout (location = 0) out vec3 vPos;
layout (location = 1) out vec3 vColor;
layout (location = 2) out vec2 vUV;

layout (binding = 0) uniform UBO {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(aPos, 1.0);
    vPos = vec3(ubo.proj * ubo.view * ubo.model * vec4(aPos, 1.0));
    vColor = aColor;
    vUV = aUV;
}