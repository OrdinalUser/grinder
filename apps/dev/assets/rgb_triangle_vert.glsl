#version 330 core

layout (location = 0) in vec3 aPos;    // hard-coded triangle position
layout (location = 1) in vec3 aColor;  // hard-coded color per vertex

out vec3 vColor;

void main() {
    gl_Position = vec4(aPos, 1.0);
    vColor = aColor;
}
