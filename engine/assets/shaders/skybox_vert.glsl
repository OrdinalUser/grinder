#version 330 core
layout(location = 0) in vec3 aPos;
out vec3 vTexCoord;

uniform mat4 uProjection;
uniform mat4 uView; // expected to be rotation-only (no translation)

void main() {
    vTexCoord = aPos;
    vec4 pos = uProjection * uView * vec4(aPos, 1.0);
    // Push depth to far plane so skybox doesn't occlude scene geometry
    gl_Position = pos.xyww;
}