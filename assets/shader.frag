#version 450

layout (location = 0) in vec3 vPos;
layout (location = 1) in vec3 vColor;
layout (location = 2) in vec2 vUV;

layout (location = 0) out vec4 oColor;

layout (binding = 1) uniform sampler2D uTexture;

void main() {
    oColor = texture(uTexture, vUV);
}