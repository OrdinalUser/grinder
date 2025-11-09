#version 330 core

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
} fs_in;

out vec4 FragColor;

// Material properties (colors only, no textures)
struct Material {
    vec3 diffuseColor;
    vec3 specularColor;
    float shininess;
    float opacity;
};

uniform Material uMaterial;
uniform vec3 uViewPos;

// Placeholder light - we'll expand this later
uniform vec3 uLightPos;
uniform vec3 uLightColor;

void main() {
    // Ambient
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * uLightColor * uMaterial.diffuseColor;
    
    // Diffuse
    vec3 norm = normalize(fs_in.Normal);
    vec3 lightDir = normalize(uLightPos - fs_in.FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * uLightColor * uMaterial.diffuseColor;
    
    // Specular (Blinn-Phong)
    vec3 viewDir = normalize(uViewPos - fs_in.FragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(norm, halfwayDir), 0.0), uMaterial.shininess);
    vec3 specular = spec * uLightColor * uMaterial.specularColor;
    
    vec3 result = ambient + diffuse + specular;
    FragColor = vec4(result, uMaterial.opacity);
}
