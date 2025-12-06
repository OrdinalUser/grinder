#version 450 core

// Workgroup size - processes multiple clusters per workgroup
layout(local_size_x = 16, local_size_y = 9, local_size_z = 1) in;

// Light types matching CPU enum
const uint LIGHT_TYPE_DIRECTIONAL = 0u;
const uint LIGHT_TYPE_POINT = 1u;
const uint LIGHT_TYPE_SPOT = 2u;

// Cluster dimensions
const uint CLUSTER_GRID_X = 16u;
const uint CLUSTER_GRID_Y = 9u;
const uint CLUSTER_GRID_Z = 24u;

struct AABB {
    vec4 minPoint;
    vec4 maxPoint;
};

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

// SSBOs
layout(std430, binding = 0) readonly buffer ClusterAABBs {
    AABB clusters[];
};

layout(std430, binding = 1) buffer LightGridBuffer {
    LightGrid lightGrid[];
};

layout(std430, binding = 2) buffer LightIndicesBuffer {
    uint globalLightIndexCounter;  // First element is atomic counter
    uint lightIndices[];           // Rest are light indices
};

layout(std430, binding = 3) readonly buffer LightsBuffer {
    LightData lights[];
};

// Uniforms
uniform uint uLightCount;
uniform float uZNear;
uniform float uZFar;

// Shared memory for light culling within workgroup
shared uint visibleLightCount;
shared uint visibleLightIndices[16u];  // Max lights per cluster

// ==================== Intersection Tests ====================

// Sphere-AABB intersection (for point lights)
bool SphereAABBIntersection(vec3 center, float radius, vec3 aabbMin, vec3 aabbMax) {
    vec3 closestPoint = clamp(center, aabbMin, aabbMax);
    vec3 distance = center - closestPoint;
    return dot(distance, distance) <= radius * radius;
}

// Cone-AABB intersection (for spot lights)
bool ConeAABBIntersection(vec3 tip, vec3 direction, float range, float cosAngle, 
                          vec3 aabbMin, vec3 aabbMax) {
    // Simple conservative test: sphere around cone
    // More accurate tests are expensive, this is good enough for culling
    vec3 coneCenter = tip + direction * (range * 0.5);
    float coneRadius = range * tan(acos(cosAngle)) + range * 0.5;
    
    return SphereAABBIntersection(coneCenter, coneRadius, aabbMin, aabbMax);
}

// Frustum-AABB intersection (for directional lights)
// Directional lights affect all clusters, but we can still cull based on shadow cascades
// For now, we'll just always include them
bool DirectionalLightIntersection() {
    return true;  // Directional lights always affect everything
}

// ==================== Main Culling Logic ====================

void main() {
    // Calculate cluster coordinates
    uvec3 clusterCoord = gl_GlobalInvocationID;
    
    // Check bounds
    if (clusterCoord.x >= CLUSTER_GRID_X || 
        clusterCoord.y >= CLUSTER_GRID_Y || 
        clusterCoord.z >= CLUSTER_GRID_Z) {
        return;
    }
    
    // Calculate flat cluster index
    uint clusterIndex = clusterCoord.x + 
                        clusterCoord.y * CLUSTER_GRID_X + 
                        clusterCoord.z * CLUSTER_GRID_X * CLUSTER_GRID_Y;
    
    
    // Initialize shared memory (first thread in workgroup)
    if (gl_LocalInvocationIndex == 0) {
        visibleLightCount = 0;
    }
    barrier();
    
    // Get cluster AABB in view space
    vec3 clusterMin = clusters[clusterIndex].minPoint.xyz;
    vec3 clusterMax = clusters[clusterIndex].maxPoint.xyz;
    
    // Divide lights among threads in workgroup
    uint threadCount = gl_WorkGroupSize.x * gl_WorkGroupSize.y * gl_WorkGroupSize.z;
    uint lightsPerThread = (uLightCount + threadCount - 1) / threadCount;
    uint startLight = gl_LocalInvocationIndex * lightsPerThread;
    uint endLight = min(startLight + lightsPerThread, uLightCount);
    
    // Test each light assigned to this thread
    for (uint i = startLight; i < endLight; i++) {
        LightData light = lights[i];
        
        // Extract light properties (already in world space)
        uint lightType = uint(light.positionAndType.w);
        vec3 lightPos = light.positionAndType.xyz;
        vec3 lightDir = normalize(light.directionAndRange.xyz);
        float lightRange = light.directionAndRange.w;
        
        bool intersects = false;
        
        // Perform intersection test based on light type
        if (lightType == LIGHT_TYPE_DIRECTIONAL) {
            // Directional lights affect all clusters
            intersects = DirectionalLightIntersection();
        }
        else if (lightType == LIGHT_TYPE_POINT) {
            // Point light: sphere-AABB test (in world space)
            intersects = SphereAABBIntersection(lightPos, lightRange, 
                                               clusterMin, clusterMax);
        }
        else if (lightType == LIGHT_TYPE_SPOT) {
            // Spot light: cone-AABB test (in world space)
            float outerCutoff = light.spotAngles.y;  // Already cosine
            intersects = ConeAABBIntersection(lightPos, lightDir, 
                                             lightRange, outerCutoff,
                                             clusterMin, clusterMax);
        }
        
        // If light intersects cluster, add to visible list
        if (intersects) {
            uint offset = atomicAdd(visibleLightCount, 1u);
            if (offset < 16u) {  // Prevent overflow
                visibleLightIndices[offset] = i;
            }
        }
    }
    
    // Wait for all threads to finish testing
    barrier();
    
    // Write results to global memory (only first thread in workgroup)
    if (gl_LocalInvocationIndex == 0) {
        uint count = min(visibleLightCount, 16u);
        
        // Allocate space in global light index list
        uint offset = atomicAdd(globalLightIndexCounter, count);
        
        // Store count and offset in grid
        lightGrid[clusterIndex].count = count;
        lightGrid[clusterIndex].offset = offset;
        
        // Copy light indices to global list
        for (uint i = 0; i < count; i++) {
            lightIndices[offset + i] = visibleLightIndices[i];
        }
    }
}