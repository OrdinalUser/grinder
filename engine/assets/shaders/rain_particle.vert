#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in vec3 instanceOffset;
layout(location = 3) in vec3 instanceVelocity;
layout(location = 4) in float instanceLife;

layout(location = 0) out vec2 vTexCoord;
layout(location = 1) out vec3 vVelocity;
layout(location = 2) out float vLife;

layout(binding = 0) uniform UniformBuffer {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 cameraPos;
    float time;
} ubuf;

void main() {
    vec3 worldPos = position * 0.02 + instanceOffset + instanceVelocity * ubuf.time * 0.5;
    
    gl_Position = ubuf.proj * ubuf.view * ubuf.model * vec4(worldPos, 1.0);
    
    vTexCoord = texCoord;
    vVelocity = instanceVelocity;
    vLife = instanceLife;
}