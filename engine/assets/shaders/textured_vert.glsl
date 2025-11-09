#version 330 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec3 aTangent;

out VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
    vec3 Tangent;
} vs_out;

uniform mat4 uModel;
uniform mat4 uProjView;

void main() {
    vec4 worldPos = uModel * vec4(aPosition, 1.0);
    vs_out.FragPos = worldPos.xyz;
    
    mat3 normalMatrix = mat3(transpose(inverse(uModel)));
    vs_out.Normal = normalMatrix * aNormal;
    vs_out.Tangent = normalMatrix * aTangent;
    vs_out.TexCoord = aUV;
    
    gl_Position = uProjView * worldPos;
}