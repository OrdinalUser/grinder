#version 450 core
out vec4 FragColor;

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
} fs_in;

struct Material {
    vec3 diffuseColor;
    vec3 specularColor;
    float shininess;
    sampler2D diffuseMap;
    sampler2D specularMap;
    sampler2D normalMap;
};

uniform Material material;
uniform vec3 uLightDir;   // normalized, world-space
uniform vec3 uLightColor;
uniform vec3 uViewPos;

void main() {
    /// Visualize normals - debug leftover
    // vec3 N = normalize(fs_in.Normal); // just normal, ignore map
    // vec3 normalTex = texture(material.normalMap, fs_in.TexCoord).rgb * 2.0 - 1.0;
    // vec3 color = N * 0.5 + 0.5;       // visualize normals
    // FragColor = vec4(normalTex, 1.0);

    vec3 N = normalize(fs_in.Normal);
    vec3 L = normalize(-uLightDir);
    vec3 V = normalize(uViewPos - fs_in.FragPos);
    vec3 H = normalize(L + V);

    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0), material.shininess);

    vec3 diffuseTex = texture(material.diffuseMap, fs_in.TexCoord).rgb;
    vec3 specularTex = texture(material.specularMap, fs_in.TexCoord).rgb;

    vec3 diffuse = material.diffuseColor * diffuseTex * diff;
    vec3 specular = material.specularColor * specularTex * spec;
    vec3 ambient = 0.05 * material.diffuseColor;

    vec3 color = (ambient + diffuse + specular) * uLightColor;
    FragColor = vec4(diffuse, 1.0);
}

// #version 450 core

// out vec4 FragColor;

// in VS_OUT {
//     vec3 FragPos;
//     vec2 TexCoord;
//     mat3 TBN;
// } fs_in;

// struct Material {
//     vec3 diffuseColor;
//     vec3 specularColor;
//     float shininess;
//     sampler2D diffuseMap;
//     sampler2D specularMap;
//     sampler2D normalMap;
// };

// uniform Material material;

// // Lighting
// uniform vec3 uLightDir;   // normalized, world-space
// uniform vec3 uLightColor;

// uniform vec3 uViewPos;

// void main()
// {
//     /// Visualize normals - debug leftover
//     // vec3 N = normalize(fs_in.TBN[2]); // just normal, ignore map
//     // vec3 color = N * 0.5 + 0.5;       // visualize normals
//     // FragColor = vec4(color, 1.0);

//     // Sample maps
//     // vec3 diffuseTex = texture(material.diffuseMap, fs_in.TexCoord).rgb;
//     // FragColor = vec4(diffuseTex, 1.0);

//     vec3 diffuseTex = texture(material.diffuseMap, fs_in.TexCoord).rgb;
//     vec3 specularTex = texture(material.specularMap, fs_in.TexCoord).rgb;
//     // vec3 normalTex = texture(material.normalMap, fs_in.TexCoord).rgb * 2.0 - 1.0;

//     // Construct normal
//     vec3 N = normalize(fs_in.TBN * normalTex);
//     vec3 L = normalize(-uLightDir);
//     vec3 V = normalize(uViewPos - fs_in.FragPos);
//     vec3 H = normalize(L + V);

//     // vec3 N = normalize(fs_in.TBN[2]);  // just vertex normal

//     // Blinn–Phong shading
//     float diff = max(dot(N, L), 0.0);
//     float spec = pow(max(dot(N, H), 0.0), material.shininess);

//     vec3 diffuse = (material.diffuseColor * diffuseTex) * diff;
//     vec3 specular = (material.specularColor * specularTex) * spec;
//     vec3 ambient = 0.05 * material.diffuseColor;

//     vec3 color = (ambient + diffuse + specular) * uLightColor;
//     FragColor = vec4(color, 1.0);
// }
