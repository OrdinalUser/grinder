#version 330 core

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
    vec3 Tangent;
} fs_in;

out vec4 FragColor;

// Material properties
struct Material {
    float shininess;
    sampler2D diffuseMap;
    sampler2D specularMap;
    sampler2D normalMap;
};

uniform Material uMaterial;
uniform vec3 uViewPos;

// Placeholder light - you'll expand this later
uniform vec3 uLightPos;
uniform vec3 uLightColor;

void main() {
    // Sample textures
    vec4 texDiffuse = texture(uMaterial.diffuseMap, fs_in.TexCoord);
    vec3 texSpecular = texture(uMaterial.specularMap, fs_in.TexCoord).rgb;
    vec3 texNormal = texture(uMaterial.normalMap, fs_in.TexCoord).rgb;
    
    // Normal mapping (convert from [0,1] to [-1,1])
    texNormal = normalize(texNormal * 2.0 - 1.0);
    
    // TBN matrix for normal mapping
    vec3 N = normalize(fs_in.Normal);
    vec3 T = normalize(fs_in.Tangent);
    T = normalize(T - dot(T, N) * N); // Gram-Schmidt orthogonalization
    vec3 B = cross(N, T);
    mat3 TBN = mat3(T, B, N);
    vec3 normal = normalize(TBN * texNormal);
    
    // Ambient
    float ambientStrength = 0.05;
    vec3 ambient = ambientStrength * uLightColor * texDiffuse.rgb;
    
    // Diffuse
    vec3 lightDir = normalize(uLightPos - fs_in.FragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * uLightColor * texDiffuse.rgb;
    
    // Specular (Blinn-Phong)
    vec3 viewDir = normalize(uViewPos - fs_in.FragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), uMaterial.shininess);
    vec3 specular = spec * uLightColor * texSpecular;
    
    vec3 result = ambient + diffuse + specular;
    FragColor = vec4(result, texDiffuse.a);
}