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

uint GetClusterIndex(vec3 fragPos) {
    vec4 viewPos4 = uViewMatrix * vec4(fragPos, 1.0);
    float viewZ = viewPos4.z;            // negative in front of camera

    // screen-space normalized [0,1]
    vec2 screenPos = gl_FragCoord.xy / uScreenDimensions;

    // tile X/Y: multiply by grid count
    uint clusterX = uint(clamp(floor(screenPos.x * float(CLUSTER_GRID_X)), 0.0, float(CLUSTER_GRID_X - 1)));
    uint clusterY = uint(clamp(floor(screenPos.y * float(CLUSTER_GRID_Y)), 0.0, float(CLUSTER_GRID_Y - 1)));

    // depth slice (exponential), same formula as builder
    float depthRatio = log(-viewZ / uZNear) / log(uZFar / uZNear);
    depthRatio = clamp(depthRatio, 0.0, 0.999999);
    uint clusterZ = uint(depthRatio * float(CLUSTER_GRID_Z));
    clusterZ = min(clusterZ, CLUSTER_GRID_Z - 1u);

    return clusterX + clusterY * CLUSTER_GRID_X + clusterZ * (CLUSTER_GRID_X * CLUSTER_GRID_Y);
}


// // Convert fragment position to cluster index
// uint GetClusterIndex(vec3 fragPos) {
//     // Transform fragment to view space
//     vec4 viewPos = uViewMatrix * vec4(fragPos, 1.0);
//     float viewZ = viewPos.z; // Keep negative (OpenGL view space convention)
    
//     // Calculate screen-space tile
//     vec2 screenPos = gl_FragCoord.xy / uScreenDimensions;
//     uint clusterX = uint(screenPos.x / float(CLUSTER_GRID_X));
//     uint clusterY = uint(screenPos.y / float(CLUSTER_GRID_Y));
    
//     // Calculate depth slice (exponential) - MUST match cluster builder exactly
//     // viewZ is negative, so negate it to get positive depth for log calculation
//     // This matches: nearDepth = -uZNear * pow(uZFar / uZNear, tNear)
//     // Solving for t: -viewZ = uZNear * pow(uZFar / uZNear, t)
//     //                -viewZ / uZNear = pow(uZFar / uZNear, t)
//     //                log(-viewZ / uZNear) / log(uZFar / uZNear) = t
//     // float depthRatio = log(-viewZ / uZNear) / log(uZFar / uZNear);
//     // uint clusterZ = uint(clamp(depthRatio, 0.0, 0.999) * float(CLUSTER_GRID_Z));
//     uint zTile = uint((log(abs(fragPos.z) / uZNear) * CLUSTER_GRID_Z) / log(uZFar / uZNear));
    
//     // Clamp to valid range
//     clusterX = min(clusterX, CLUSTER_GRID_X - 1);
//     clusterY = min(clusterY, CLUSTER_GRID_Y - 1);
//     uint clusterZ = min(zTile, CLUSTER_GRID_Z - 1);
    
//     return clusterX + clusterY * CLUSTER_GRID_X + clusterZ * CLUSTER_GRID_X * CLUSTER_GRID_Y;
// }

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
    float ambientStrength = 0.005;
    vec3 ambient = ambientStrength * uMaterial.diffuseColor;
    
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

    //// DEBUG
    // vec4 viewPos = uViewMatrix * vec4(fs_in.FragPos, 1.0);
    // float viewZ = viewPos.z;
    
    // vec2 screenPos = gl_FragCoord.xy / uScreenDimensions;
    // uint clusterX = uint(screenPos.x * float(CLUSTER_GRID_X));
    // uint clusterY = uint(screenPos.y * float(CLUSTER_GRID_Y));
    
    // float depthRatio = log(-viewZ / uZNear) / log(uZFar / uZNear);
    // uint clusterZ = uint(clamp(depthRatio, 0.0, 0.999) * float(CLUSTER_GRID_Z));

    // result *= 0.00001;
    
    // vec3 dbg = vec3(float(clusterIndex) / float(CLUSTER_GRID_X * CLUSTER_GRID_Y * CLUSTER_GRID_Z), lightCount > 0 ? 1.0 : 0.0, 0.0);
    // vec3 dbg = vec3(float(clusterZ) / 24.0); // cluster Z
    // vec3 dbg = vec3(float(clusterX) / 16.0, float(clusterY) / 9.0, float(clusterZ) / 24.0); // components
    // vec3 dbg = vec3(lightCount * 10.0, 0.0, float(clusterZ) / 24.0);

    // result += dbg;
    
    FragColor = vec4(result, uMaterial.opacity);
}