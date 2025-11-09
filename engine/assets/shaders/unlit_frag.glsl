#version 330 core

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
} fs_in;

out vec4 FragColor;

struct Material {
    vec3 diffuseColor;
    float opacity;
};

uniform Material uMaterial;

void main() {
    // Just output the material color, no lighting
    FragColor = vec4(uMaterial.diffuseColor, uMaterial.opacity);
}