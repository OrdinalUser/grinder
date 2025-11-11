#version 430 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec3 aTangent;

layout(std430, binding = 0) readonly buffer InstanceBuffer {
    mat4 iModel[];
};

out VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
    vec3 Tangent;
} vs_out;

uniform bool uUseInstancing;
uniform mat4 uModel;
uniform mat4 uProjView;

void main() {
    mat4 model = uUseInstancing ? iModel[gl_InstanceID] : uModel;
    vec4 worldPos = model * vec4(aPosition, 1.0);
    
    mat3 normalMatrix = mat3(transpose(inverse(model)));
    vs_out.FragPos = worldPos.xyz;
    vs_out.Tangent = normalMatrix * aTangent;
    vs_out.Normal = normalMatrix * aNormal;
    vs_out.TexCoord = aUV;
    
    gl_Position = uProjView * worldPos;
}