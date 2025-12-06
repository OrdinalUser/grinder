#version 450 core

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
} fs_in;

out vec4 FragColor;

// Light types
const uint LIGHT_TYPE_DIRECTIONAL = 0u;
const uint LIGHT_TYPE_POINT = 1u;
const uint LIGHT_TYPE_SPOT = 2u;

// Cluster dimensions
const uint CLUSTER_GRID_X = 16u;
const uint CLUSTER_GRID_Y = 9u;
const uint CLUSTER_GRID_Z = 24u;

// Material properties
struct Material {
    vec3 diffuseColor;
    vec3 specularColor;
    float shininess;
    float opacity;
    float emmisiveIntensity;
    vec3 emmisiveColor;
};

// GPU Light Data (matches CPU structure)
struct LightData {
    vec4 positionAndType;   // xyz = position, w = type
    vec4 directionAndRange; // xyz = direction, w = range
    vec4 colorAndIntensity; // rgb = color, a = intensity
    vec4 spotAngles;        // x = innerCutoff, y = outerCutoff
};

struct LightGrid {
    uint count;
    uint offset;
};

// SSBOs (same bindings as compute shader)
layout(std430, binding = 1) readonly buffer LightGridBuffer {
    LightGrid lightGrid[];
};

layout(std430, binding = 2) readonly buffer LightIndicesBuffer {
    uint globalLightIndexCounter;
    uint lightIndices[];
};

layout(std430, binding = 3) readonly buffer LightsBuffer {
    LightData lights[];
};

// Uniforms
uniform Material uMaterial;
uniform vec3 uViewPos;
uniform mat4 uViewMatrix;
uniform vec2 uScreenDimensions;
uniform float uZNear;
uniform float uZFar;

// ==================== Utility Functions ====================

// Convert fragment position to cluster index
uint GetClusterIndex(vec3 fragPos) {
    // Transform fragment to view space
    vec4 viewPos = uViewMatrix * vec4(fragPos, 1.0);
    float viewZ = -viewPos.z; // OpenGL uses negative Z in view space
    
    // Calculate screen-space tile
    vec2 screenPos = gl_FragCoord.xy / uScreenDimensions;
    uint clusterX = uint(screenPos.x * float(CLUSTER_GRID_X));
    uint clusterY = uint(screenPos.y * float(CLUSTER_GRID_Y));
    
    // Calculate depth slice (exponential)
    float depthRatio = log(viewZ / uZNear) / log(uZFar / uZNear);
    uint clusterZ = uint(clamp(depthRatio, 0.0, 1.0) * float(CLUSTER_GRID_Z));
    
    // Clamp to valid range
    clusterX = min(clusterX, CLUSTER_GRID_X - 1);
    clusterY = min(clusterY, CLUSTER_GRID_Y - 1);
    clusterZ = min(clusterZ, CLUSTER_GRID_Z - 1);
    
    return clusterX + clusterY * CLUSTER_GRID_X + clusterZ * CLUSTER_GRID_X * CLUSTER_GRID_Y;
}

// ==================== Lighting Calculations ====================

vec3 CalculateDirectionalLight(LightData light, vec3 normal, vec3 viewDir, vec3 fragPos) {
    vec3 lightDir = normalize(-light.directionAndRange.xyz);
    vec3 lightColor = light.colorAndIntensity.rgb;
    float intensity = light.colorAndIntensity.a;
    
    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * lightColor * intensity * uMaterial.diffuseColor;
    
    // Specular (Blinn-Phong)
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), uMaterial.shininess);
    vec3 specular = spec * lightColor * intensity * uMaterial.specularColor;
    
    return diffuse + specular;
}

vec3 CalculatePointLight(LightData light, vec3 normal, vec3 viewDir, vec3 fragPos) {
    vec3 lightPos = light.positionAndType.xyz;
    vec3 lightColor = light.colorAndIntensity.rgb;
    float intensity = light.colorAndIntensity.a;
    float range = light.directionAndRange.w;
    
    vec3 lightDir = normalize(lightPos - fragPos);
    float distance = length(lightPos - fragPos);
    
    // Attenuation (inverse square with smooth falloff at range)
    float attenuation = 1.0;
    if (range > 0.0) {
        attenuation = clamp(1.0 - (distance * distance) / (range * range), 0.0, 1.0);
        attenuation *= attenuation; // Squared falloff for smoother transition
    }
    
    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * lightColor * intensity * uMaterial.diffuseColor * attenuation;
    
    // Specular (Blinn-Phong)
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), uMaterial.shininess);
    vec3 specular = spec * lightColor * intensity * uMaterial.specularColor * attenuation;
    
    return diffuse + specular;
}

vec3 CalculateSpotLight(LightData light, vec3 normal, vec3 viewDir, vec3 fragPos) {
    vec3 lightPos = light.positionAndType.xyz;
    vec3 lightDir = normalize(light.directionAndRange.xyz);
    vec3 lightColor = light.colorAndIntensity.rgb;
    float intensity = light.colorAndIntensity.a;
    float range = light.directionAndRange.w;
    float innerCutoff = light.spotAngles.x; // Already cosine
    float outerCutoff = light.spotAngles.y; // Already cosine
    
    vec3 toFragment = normalize(lightPos - fragPos);
    float distance = length(lightPos - fragPos);
    
    // Distance attenuation
    float attenuation = 1.0;
    if (range > 0.0) {
        attenuation = clamp(1.0 - (distance * distance) / (range * range), 0.0, 1.0);
        attenuation *= attenuation;
    }
    
    // Spotlight cone attenuation
    float theta = dot(toFragment, -lightDir);
    float epsilon = innerCutoff - outerCutoff;
    float spotIntensity = clamp((theta - outerCutoff) / epsilon, 0.0, 1.0);
    
    attenuation *= spotIntensity;
    
    // Diffuse
    float diff = max(dot(normal, toFragment), 0.0);
    vec3 diffuse = diff * lightColor * intensity * uMaterial.diffuseColor * attenuation;
    
    // Specular (Blinn-Phong)
    vec3 halfwayDir = normalize(toFragment + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), uMaterial.shininess);
    vec3 specular = spec * lightColor * intensity * uMaterial.specularColor * attenuation;
    
    return diffuse + specular;
}

// ==================== Main ====================

void main() {
    // Get cluster index for this fragment
    uint clusterIndex = GetClusterIndex(fs_in.FragPos);
    
    // Get light count and offset for this cluster
    uint lightCount = lightGrid[clusterIndex].count;
    uint lightOffset = lightGrid[clusterIndex].offset;
    
    // Base lighting
    vec3 norm = normalize(fs_in.Normal);
    vec3 viewDir = normalize(uViewPos - fs_in.FragPos);
    
    // Ambient (very low global illumination)
    vec3 ambient = 0.03 * uMaterial.diffuseColor;
    
    // Accumulate lighting from all visible lights in this cluster
    vec3 lighting = vec3(0.0);
    
    for (uint i = 0; i < lightCount; i++) {
        // Get light index from global list
        uint lightIndex = lightIndices[lightOffset + i];
        LightData light = lights[lightIndex];
        
        // Extract light type
        uint lightType = uint(light.positionAndType.w);
        
        // Calculate lighting based on type
        if (lightType == LIGHT_TYPE_DIRECTIONAL) {
            lighting += CalculateDirectionalLight(light, norm, viewDir, fs_in.FragPos);
        }
        else if (lightType == LIGHT_TYPE_POINT) {
            lighting += CalculatePointLight(light, norm, viewDir, fs_in.FragPos);
        }
        else if (lightType == LIGHT_TYPE_SPOT) {
            lighting += CalculateSpotLight(light, norm, viewDir, fs_in.FragPos);
        }
    }
    
    // Add ambient and emissive
    vec3 result = ambient + lighting;
    vec3 emission = uMaterial.emmisiveIntensity * uMaterial.emmisiveColor;
    result += emission;
    
    FragColor = vec4(result, uMaterial.opacity);
}