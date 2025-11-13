#version 460
layout(local_size_x = 256) in;

struct BSphere {
    vec3 center;
    float radius;
};

struct InstanceData {
    mat4 model;
    BSphere sphere;     // xyz = center, w = radius
};

layout(std430, binding = 0) readonly buffer Instances { InstanceData instances[]; };
layout(std430, binding = 1) writeonly buffer Visible { uint visibleIndices[]; };
layout(std140, binding = 3) uniform FrustumPlanes { vec4 planes[6]; };

void main() {
    uint id = gl_GlobalInvocationID.x;
    if (id >= instances.length()) return;

    mat4 model = instances[id].model;
    BSphere s = instances[id].sphere;
    vec3 center = vec3(model * vec4(s.center, 1.0));
    float scale = max(max(length(model[0].xyz), length(model[1].xyz)), length(model[2].xyz));
    float radius = s.radius * scale;

    uint inside = 1;
    for (int i = 0; i < 6; ++i) {
        float d = dot(planes[i].xyz, center) + planes[i].w;
        if (d < -radius) { inside = 0; break; }
    }

    visibleIndices[id] = inside;
}
