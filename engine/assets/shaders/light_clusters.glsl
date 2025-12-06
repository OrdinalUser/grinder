// #version 450 core
// layout(local_size_x = 1, local_size_y = 1) in;

// struct AABB {
//     vec4 minPoint;
//     vec4 maxPoint;
// };

// layout(std430, binding = 0) writeonly buffer ClusterAABBs {
//     AABB clusters[];
// };

// uniform mat4 uInverseProjection;
// uniform uvec3 uGridSize;
// uniform uvec2 uScreenSize;

// uniform float uZNear;
// uniform float uZFar;

// vec4 clipToView(vec4 clip);
// vec4 screen2View(vec4 screen);
// vec3 lineIntersectionToZPlane(vec3 A, vec3 B, float zDistance);

// void main(){
//     // Eye position is zero in view space
//     const vec3 eyePos = vec3(0.0);

//     // Per Tile variables
//     uint tileSizePx = (uScreenSize / uGridSize.xy).x;
//     uint tileIndex = gl_WorkGroupID.x +
//                      gl_WorkGroupID.y * gl_NumWorkGroups.x +
//                      gl_WorkGroupID.z * (gl_NumWorkGroups.x * gl_NumWorkGroups.y);

//     // Calculating the min and max point in screen space
//     vec4 maxPoint_sS = vec4(vec2(gl_WorkGroupID.x + 1, gl_WorkGroupID.y + 1) * tileSizePx, -1.0, 1.0); // Top Right
//     vec4 minPoint_sS = vec4(gl_WorkGroupID.xy * tileSizePx, -1.0, 1.0); // Bottom left
    
//     // Pass min and max to view space
//     vec3 maxPoint_vS = screen2View(maxPoint_sS).xyz;
//     vec3 minPoint_vS = screen2View(minPoint_sS).xyz;

//     // Near and far values of the cluster in view space
//     float tileNear  = -uZNear * pow(uZFar/ uZNear, gl_WorkGroupID.z/float(gl_NumWorkGroups.z));
//     float tileFar   = -uZNear * pow(uZFar/ uZNear, (gl_WorkGroupID.z + 1) /float(gl_NumWorkGroups.z));

//     // Finding the 4 intersection points made from the maxPoint to the cluster near/far plane
//     vec3 minPointNear = lineIntersectionToZPlane(eyePos, minPoint_vS, tileNear );
//     vec3 minPointFar  = lineIntersectionToZPlane(eyePos, minPoint_vS, tileFar );
//     vec3 maxPointNear = lineIntersectionToZPlane(eyePos, maxPoint_vS, tileNear );
//     vec3 maxPointFar  = lineIntersectionToZPlane(eyePos, maxPoint_vS, tileFar );

//     vec3 minPointAABB = min(min(minPointNear, minPointFar),min(maxPointNear, maxPointFar));
//     vec3 maxPointAABB = max(max(minPointNear, minPointFar),max(maxPointNear, maxPointFar));

//     // Getting the 
//     clusters[tileIndex].minPoint  = vec4(minPointAABB , 0.0);
//     clusters[tileIndex].maxPoint  = vec4(maxPointAABB , 0.0);
// }

// //Creates a line from the eye to the screenpoint, then finds its intersection
// //With a z oriented plane located at the given distance to the origin
// vec3 lineIntersectionToZPlane(vec3 A, vec3 B, float zDistance){
//     //Because this is a Z based normal this is fixed
//     vec3 normal = vec3(0.0, 0.0, 1.0);

//     vec3 ab =  B - A;

//     //Computing the intersection length for the line and the plane
//     float t = (zDistance - dot(normal, A)) / dot(normal, ab);

//     //Computing the actual xyz position of the point along the line
//     vec3 result = A + t * ab;

//     return result;
// }

// vec4 clipToView(vec4 clip){
//     //View space transform
//     vec4 view = uInverseProjection * clip;

//     //Perspective projection
//     view = view / view.w;
    
//     return view;
// }

// vec4 screen2View(vec4 screen){
//     //Convert to NDC
//     vec2 texCoord = screen.xy / uScreenSize.xy;

//     //Convert to clipSpace
//     // vec4 clip = vec4(vec2(texCoord.x, 1.0 - texCoord.y)* 2.0 - 1.0, screen.z, screen.w);
//     vec4 clip = vec4(vec2(texCoord.x, texCoord.y)* 2.0 - 1.0, screen.z, screen.w);
//     //Not sure which of the two it is just yet

//     return clipToView(clip);
// }

//// ================================================================

#version 450 core
// layout(local_size_x = 4, local_size_y = 4, local_size_z = 4) in;
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

struct AABB {
    vec4 minPoint;
    vec4 maxPoint;
};

layout(std430, binding = 0) writeonly buffer ClusterAABBs {
    AABB clusters[];
};

uniform float uZNear;
uniform float uZFar;
uniform mat4 uInverseProjection;
uniform uvec3 uGridSize;
uniform uvec2 uScreenSize;

// Returns the intersection point of an infinite line and a
// plane perpendicular to the Z-axis
vec3 lineIntersectionWithZPlane(vec3 startPoint, vec3 endPoint, float zDistance)
{
    vec3 direction = endPoint - startPoint;
    vec3 normal = vec3(0.0, 0.0, -1.0); // plane normal

    // skip check if the line is parallel to the plane. ; TODO

    float t = (zDistance - dot(normal, startPoint)) / dot(normal, direction);
    return startPoint + t * direction; // the parametric form of the line equation
}

vec3 screenToView(vec2 screenCoord)
{
    // normalize screenCoord to [-1, 1] and
    // set the NDC depth of the coordinate to be on the near plane. This is -1 by
    // default in OpenGL
    vec4 ndc = vec4(screenCoord / uScreenSize * 2.0 - 1.0, -1.0, 1.0);

    vec4 viewCoord = uInverseProjection * ndc;
    viewCoord /= viewCoord.w;
    return viewCoord.xyz;
}

void main()
{   
    // Calculate cluster index
    uint tileIndex = gl_WorkGroupID.x + (gl_WorkGroupID.y * uGridSize.x) + (gl_WorkGroupID.z * uGridSize.x * uGridSize.y);
    vec2 tileSize = uScreenSize / uGridSize.xy;

    // tile in screen-space
    vec2 minTile_screenspace = gl_WorkGroupID.xy * tileSize;
    vec2 maxTile_screenspace = (gl_WorkGroupID.xy + 1) * tileSize;

    // convert tile to view space sitting on the near plane
    vec3 minTile = screenToView(minTile_screenspace);
    vec3 maxTile = screenToView(maxTile_screenspace);

    float planeNear = uZNear * pow(uZFar / uZNear, gl_WorkGroupID.z / float(uGridSize.z));
    float planeFar = uZNear * pow(uZFar / uZNear, (gl_WorkGroupID.z + 1) / float(uGridSize.z));

    // the line goes from the eye position in view space (0, 0, 0)
    // through the min/max points of a tile to intersect with a given cluster's near-far planes
    vec3 minPointNear = lineIntersectionWithZPlane(vec3(0, 0, 0), minTile, planeNear);
    vec3 minPointFar = lineIntersectionWithZPlane(vec3(0, 0, 0), minTile, planeFar);
    vec3 maxPointNear = lineIntersectionWithZPlane(vec3(0, 0, 0), maxTile, planeNear);
    vec3 maxPointFar = lineIntersectionWithZPlane(vec3(0, 0, 0), maxTile, planeFar);

    clusters[tileIndex].minPoint = vec4(min(minPointNear, minPointFar), 0.0);
    clusters[tileIndex].maxPoint = vec4(max(maxPointNear, maxPointFar), 0.0);
}

// // Unproject screen space point to view space
// vec3 ScreenToView(vec2 screenPos, float depth) {
//     // screenPos is in [0, 1], convert to NDC [-1, 1]
//     vec2 ndc = screenPos * 2.0 - 1.0;
    
//     // Use linearized depth directly in view space (negative Z)
//     vec4 clip = vec4(ndc, 0.0, 1.0);
//     vec4 view = uInvProj * clip;
    
//     // Scale by depth
//     view.xyz /= view.w;
//     view.xyz *= (depth / view.z);
    
//     return view.xyz;
// }

// void main() {
//     uvec3 cluster = gl_GlobalInvocationID;
    
//     if (cluster.x >= uGridSize.x || cluster.y >= uGridSize.y || cluster.z >= uGridSize.z) {
//         return;
//     }
    
//     // Calculate cluster index
//     uint clusterIndex = cluster.x + cluster.y * uGridSize.x + 
//                        cluster.z * uGridSize.x * uGridSize.y;
    
//     // Screen space bounds [0, 1]
//     vec2 minScreen = vec2(cluster.xy) / vec2(uGridSize.xy);
//     vec2 maxScreen = vec2(cluster.xy + 1) / vec2(uGridSize.xy);
    
//     // Exponential depth slicing (in view space, negative Z)
//     float tNear = float(cluster.z) / float(uGridSize.z);
//     float tFar = float(cluster.z + 1) / float(uGridSize.z);
    
//     // View space depths (negative because OpenGL view space has -Z forward)
//     float nearDepth = -uZNear * pow(uZFar / uZNear, tNear);
//     float farDepth = -uZNear * pow(uZFar / uZNear, tFar);
    
//     // Calculate 8 corners of the cluster in view space
//     vec3 corners[8];
//     corners[0] = ScreenToView(vec2(minScreen.x, minScreen.y), nearDepth);
//     corners[1] = ScreenToView(vec2(maxScreen.x, minScreen.y), nearDepth);
//     corners[2] = ScreenToView(vec2(minScreen.x, maxScreen.y), nearDepth);
//     corners[3] = ScreenToView(vec2(maxScreen.x, maxScreen.y), nearDepth);
//     corners[4] = ScreenToView(vec2(minScreen.x, minScreen.y), farDepth);
//     corners[5] = ScreenToView(vec2(maxScreen.x, minScreen.y), farDepth);
//     corners[6] = ScreenToView(vec2(minScreen.x, maxScreen.y), farDepth);
//     corners[7] = ScreenToView(vec2(maxScreen.x, maxScreen.y), farDepth);
    
//     // Find AABB from corners
//     vec3 minPoint = corners[0];
//     vec3 maxPoint = corners[0];
//     for (int i = 1; i < 8; i++) {
//         minPoint = min(minPoint, corners[i]);
//         maxPoint = max(maxPoint, corners[i]);
//     }
    
//     clusters[clusterIndex].minPoint = vec4(minPoint, 1.0);
//     clusters[clusterIndex].maxPoint = vec4(maxPoint, 1.0);
// }