#version 450 core

// ==== Manual #include because OpenGL is being a little bitch
// lighting.glsl - Reusable Blinn-Phong lighting calculations

#ifndef LIGHTING_GLSL
#define LIGHTING_GLSL

// Light type constants
#define LIGHT_TYPE_DIRECTIONAL 0.0
#define LIGHT_TYPE_POINT 1.0
#define LIGHT_TYPE_SPOT 2.0

// GPU Light Data structure (must match your application)
struct GPU_LightData {
    vec4 positionAndType;      // xyz: position, w: type
    vec4 directionAndRange;    // xyz: direction, w: range
    vec4 colorAndIntensity;    // xyz: color, w: intensity
    vec4 spotAnglesRadians;    // x: inner angle, y: outer angle
};

// Material structure for lighting calculations
struct Material {
    vec3 diffuseColor;
    vec3 specularColor;
    float shininess;
};

// Calculate attenuation for point and spot lights
float calculateAttenuation(float distance, float range) {
    if (range <= 0.0) return 1.0; // No attenuation
    
    // Quadratic attenuation with smooth falloff
    float attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);
    
    // Fade out near range limit
    float rangeFactor = 1.0 - smoothstep(range * 0.75, range, distance);
    return attenuation * rangeFactor;
}

// Calculate spot light cone attenuation
float calculateSpotEffect(vec3 lightToFragDir, vec3 spotDirection, float innerAngleCos, float outerAngleCos) {
    // lightToFragDir: direction FROM light TO fragment (normalized)
    // spotDirection: direction the spotlight is pointing (should be normalized)
    // We want the angle between the spot direction and the light-to-fragment direction
    float theta = dot(-lightToFragDir, normalize(spotDirection));
    
    // Smooth interpolation between inner and outer cone
    // innerAngleCos > outerAngleCos (because cos is decreasing)
    float epsilon = innerAngleCos - outerAngleCos;
    float intensity = clamp((theta - outerAngleCos) / epsilon, 0.0, 1.0);
    return intensity;
}

// Calculate Blinn-Phong lighting for a single light source
vec3 calculateBlinnPhong(
    GPU_LightData light,
    Material material,
    vec3 fragPos,
    vec3 normal,
    vec3 viewDir
) {
    vec3 lightColor = light.colorAndIntensity.rgb;
    float lightIntensity = light.colorAndIntensity.w;
    float lightType = light.positionAndType.w;
    
    vec3 lightDir;
    float attenuation = 1.0;
    
    // Determine light direction and attenuation based on type
    if (lightType == LIGHT_TYPE_DIRECTIONAL) {
        // Directional light
        lightDir = normalize(-light.directionAndRange.xyz);
    } else {
        // Point or Spot light
        vec3 lightPos = light.positionAndType.xyz;
        vec3 lightToFrag = lightPos - fragPos;
        float distance = length(lightToFrag);
        lightDir = lightToFrag / distance;
        
        float range = light.directionAndRange.w;
        attenuation = calculateAttenuation(distance, range);
        
        // Additional spot light calculations
        if (lightType == LIGHT_TYPE_SPOT) {
            vec3 spotDir = light.directionAndRange.xyz;
            // Angles are already in radians, convert to cosines
            float innerAngleCos = cos(light.spotAnglesRadians.x);
            float outerAngleCos = cos(light.spotAnglesRadians.y);
            float spotEffect = calculateSpotEffect(lightDir, spotDir, innerAngleCos, outerAngleCos);
            attenuation *= spotEffect;
        }
    }
    
    // Early exit if light doesn't reach fragment
    if (attenuation < 0.001) {
        return vec3(0.0);
    }
    
    // Normalize vectors
    vec3 norm = normalize(normal);
    vec3 viewDirection = normalize(viewDir);
    
    // Diffuse calculation
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * material.diffuseColor;
    
    // Specular calculation (Blinn-Phong)
    vec3 halfwayDir = normalize(lightDir + viewDirection);
    float spec = pow(max(dot(norm, halfwayDir), 0.0), material.shininess);
    vec3 specular = spec * material.specularColor;
    
    // Combine with light color, intensity, and attenuation
    return (diffuse + specular) * lightColor * lightIntensity * attenuation;
}

#endif // LIGHTING_GLSL
// ==== End of manual include

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
    vec3 Tangent;
} fs_in;

out vec4 FragColor;

// Material properties with textures
struct MaterialProperties {
    float shininess;
    sampler2D diffuseMap;
    sampler2D specularMap;
    sampler2D normalMap;
};

// SSBO binding for light data
layout(std430, binding = 1) readonly buffer LightBuffer {
    GPU_LightData lights[];
};

// Uniforms
uniform MaterialProperties uMaterial;
uniform vec3 uViewPos;
uniform int uNumLights;
uniform vec3 uAmbientLight;

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
    vec3 worldNormal = normalize(TBN * texNormal);
    
    // Prepare material for lighting calculations
    Material material;
    material.diffuseColor = texDiffuse.rgb;
    material.specularColor = texSpecular;
    material.shininess = uMaterial.shininess;
    
    vec3 viewDir = uViewPos - fs_in.FragPos;
    
    // Start with ambient lighting
    vec3 result = uAmbientLight * material.diffuseColor;
    
    // Add contribution from each light
    for (int i = 0; i < uNumLights; i++) {
        result += calculateBlinnPhong(
            lights[i],
            material,
            fs_in.FragPos,
            worldNormal,  // Use the normal-mapped normal
            viewDir
        );
    }
    
    // Final color
    FragColor = vec4(result, texDiffuse.a);
}