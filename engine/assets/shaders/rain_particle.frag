#version 450

layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec3 vVelocity;
layout(location = 2) in float vLife;

layout(location = 0) out vec4 outColor;

void main() {
    float alpha = (1.0 - length(vTexCoord - 0.5) * 2.0) * vLife;
    alpha = clamp(alpha * 8.0, 0.0, 1.0);
    
    vec3 rainColor = mix(vec3(0.7, 0.8, 1.0), vec3(0.4, 0.5, 0.8), vVelocity.y * 0.1);
    
    outColor = vec4(rainColor, alpha);
}