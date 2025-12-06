#version 450 core

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
} fs_in;

out vec4 FragColor;

// Material properties (colors only, no textures)
struct MaterialProperties {
    sampler2D diffuseMap;
    sampler2D emmisiveMap;
    float opacity;
    float emmisiveIntensity;
    vec3 emmisiveColor;
};

uniform MaterialProperties uMaterial;

void main() {
    vec4 texDiffuse = texture(uMaterial.diffuseMap, fs_in.TexCoord);
    vec3 emmisiveColorTex = texture(uMaterial.emmisiveMap, fs_in.TexCoord).rgb;
    vec3 emmision = (emmisiveColorTex * uMaterial.emmisiveIntensity) + (texDiffuse.rgb * uMaterial.emmisiveColor * uMaterial.emmisiveIntensity);
    FragColor = vec4(emmision, uMaterial.opacity);
}
