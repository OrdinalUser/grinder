#version 430 core

layout(location = 0) in vec3 aPosition;

layout(std430, binding = 0) readonly buffer InstanceBuffer {
    mat4 iModel[];
};

uniform bool uUseInstancing;
uniform mat4 uModel;
uniform mat4 uProjView;

void main() {
    mat4 model = uUseInstancing ? iModel[gl_InstanceID] : uModel;
    gl_Position = uProjView * model * vec4(aPosition, 1.0);
}