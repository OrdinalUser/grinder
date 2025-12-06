#version 450 core

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
    vec3 Tangent;
} fs_in;

out vec4 FragColor;

// Light types
const uint LIGHT_TYPE_DIRECTIONAL = 0u;
const uint LIGHT_TYPE_POINT = 1u;
const uint LIGHT_TYPE_SPOT = 2u;

const uint CLUSTER_GRID_X = 16u;
const uint CLUSTER_GRID_Y = 9u;
const uint CLUSTER_GRID_Z = 24u;

struct Material {
    float shininess;
    sampler2D diffuseMap;
    sampler2D specularMap;
    sampler2D normalMap;
    sampler2D emmisiveMap;
    float emmisiveIntensity;
    vec3 emmisiveColor;
};

struct LightData {
    vec4 positionAndType;
    vec4 directionAndRange;
    vec4 colorAndIntensity;
    vec4 spotAngles;
};

struct LightGrid {
    uint count;
    uint offset;
};

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

// ========== Utility ==========

uint GetClusterIndex(vec3 fragPos) {
    vec4 viewPos = uViewMatrix * vec4(fragPos, 1.0);
    float viewZ = viewPos.z;

    vec2 screenPos = gl_FragCoord.xy / uScreenDimensions;
    uint clusterX = uint(screenPos.x / float(CLUSTER_GRID_X));
    uint clusterY = uint(screenPos.y / float(CLUSTER_GRID_Y));

    uint zTile =
        uint((log(abs(fragPos.z) / uZNear) * CLUSTER_GRID_Z) /
             log(uZFar / uZNear));

    clusterX = min(clusterX, CLUSTER_GRID_X - 1);
    clusterY = min(clusterY, CLUSTER_GRID_Y - 1);
    uint clusterZ = min(zTile, CLUSTER_GRID_Z - 1);

    return clusterX +
           clusterY * CLUSTER_GRID_X +
           clusterZ * CLUSTER_GRID_X * CLUSTER_GRID_Y;
}

// ========== Normals (with normal mapping) ==========

vec3 GetNormal() {
    vec3 N = normalize(fs_in.Normal);

    vec3 T = normalize(fs_in.Tangent);
    T = normalize(T - dot(T, N) * N);  
    vec3 B = cross(N, T);
    mat3 TBN = mat3(T, B, N);

    vec3 nm = texture(uMaterial.normalMap, fs_in.TexCoord).xyz * 2.0 - 1.0;

    // If normal map is invalid (flat), this degrades safely.
    return normalize(TBN * nm);
}

// ========== Sample Material Textures ==========

vec3 GetDiffuse() {
    return texture(uMaterial.diffuseMap, fs_in.TexCoord).rgb;
}

float GetOpacity() {
    return texture(uMaterial.diffuseMap, fs_in.TexCoord).a;
}

vec3 GetSpecular() {
    return texture(uMaterial.specularMap, fs_in.TexCoord).rgb;
}

vec3 GetEmissive(vec3 diffuse) {
    vec3 tex = texture(uMaterial.emmisiveMap, fs_in.TexCoord).rgb;
    return (tex * uMaterial.emmisiveIntensity) + (diffuse * uMaterial.emmisiveColor * uMaterial.emmisiveIntensity);
}

// ========== Lighting ==========

vec3 CalculateDirectionalLight(LightData light, vec3 N, vec3 V, vec3 fragPos) {
    vec3 L = normalize(-light.directionAndRange.xyz);
    vec3 col = light.colorAndIntensity.rgb * light.colorAndIntensity.a;

    float diff = max(dot(N, L), 0.0);
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), uMaterial.shininess);

    return diff * col * GetDiffuse() +
           spec * col * GetSpecular();
}

vec3 CalculatePointLight(LightData light, vec3 N, vec3 V, vec3 fragPos) {
    vec3 lightPos = light.positionAndType.xyz;
    float range = light.directionAndRange.w;

    vec3 L = lightPos - fragPos;
    float dist = length(L);
    L /= dist;

    vec3 col = light.colorAndIntensity.rgb * light.colorAndIntensity.a;

    float att = 1.0;
    if (range > 0.0) {
        att = clamp(1.0 - (dist * dist) / (range * range), 0.0, 1.0);
        att *= att;
    }

    float diff = max(dot(N, L), 0.0);
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), uMaterial.shininess);

    return att * (
        diff * col * GetDiffuse() +
        spec * col * GetSpecular()
    );
}

vec3 CalculateSpotLight(LightData light, vec3 N, vec3 V, vec3 fragPos) {
    vec3 lightPos = light.positionAndType.xyz;
    vec3 spotDir = normalize(light.directionAndRange.xyz);
    float range = light.directionAndRange.w;

    vec3 L = lightPos - fragPos;
    float dist = length(L);
    L /= dist;

    vec3 col = light.colorAndIntensity.rgb * light.colorAndIntensity.a;

    float att = 1.0;
    if (range > 0.0) {
        att = clamp(1.0 - (dist * dist) / (range * range), 0.0, 1.0);
        att *= att;
    }

    float theta = dot(L, -spotDir);
    float inner = light.spotAngles.x;
    float outer = light.spotAngles.y;
    float spot = clamp((theta - outer) / (inner - outer), 0.0, 1.0);

    att *= spot;

    float diff = max(dot(N, L), 0.0);
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), uMaterial.shininess);

    return att * (
        diff * col * GetDiffuse() +
        spec * col * GetSpecular()
    );
}

// ========== Main ==========

void main() {
    uint clusterIndex = GetClusterIndex(fs_in.FragPos);
    uint lightCount = lightGrid[clusterIndex].count;
    uint lightOffset = lightGrid[clusterIndex].offset;

    vec3 N = GetNormal();
    vec3 V = normalize(uViewPos - fs_in.FragPos);

    vec3 ambient = 0.005 * GetDiffuse();
    vec3 lighting = vec3(0.0);

    for (uint i = 0; i < lightCount; i++) {
        uint lightIndex = lightIndices[lightOffset + i];
        LightData light = lights[lightIndex];

        uint type = uint(light.positionAndType.w);

        if (type == LIGHT_TYPE_DIRECTIONAL)
            lighting += CalculateDirectionalLight(light, N, V, fs_in.FragPos);
        else if (type == LIGHT_TYPE_POINT)
            lighting += CalculatePointLight(light, N, V, fs_in.FragPos);
        else if (type == LIGHT_TYPE_SPOT)
            lighting += CalculateSpotLight(light, N, V, fs_in.FragPos);
    }

    vec3 color = ambient + lighting + GetEmissive(GetDiffuse());
    float alpha = GetOpacity();

    FragColor = vec4(color, alpha);
}
