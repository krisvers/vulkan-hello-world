#version 450

layout (location = 0) in vec3 vColor;
layout (location = 1) in vec2 vUV;

layout (location = 0) out vec4 oColor;

void main() {
    oColor = vec4(vColor, 1.0);
}