#version 450 core

in vec2 vTexCoord;
in float vLife;
in float vSpeed;

out vec4 FragColor;

uniform float time;

void main()
{
    // Rain streak gradient: bright center, fades to edges and top/bottom
    vec2 uv = vTexCoord * 2.0 - 1.0; // -1..1 range
    float centerDist = length(uv * vec2(0.3, 1.0)); // thinner horizontally

    // Core brightness
    float core = exp(-centerDist * 12.0);

    // Fade out over lifetime
    float alpha = core * vLife * vLife; // quadratic fade = smoother disappearance

    // Add subtle motion blur shimmer
    float shimmer = sin(time * 15.0 + vTexCoord.y * 20.0 + vSpeed * 0.1) * 0.1 + 0.9;

    // Final color: bright white-blue with slight randomness
    vec3 color = vec3(0.8, 0.9, 1.0) * shimmer * 1.5;

    // Soft particle feel
    alpha *= smoothstep(0.0, 0.15, vLife); // extra fade at end of life

    FragColor = vec4(color, alpha * 0.9);

    // Optional: uncomment for bright white rain
    // FragColor = vec4(1.0, 1.0, 1.05, alpha * 0.8);
}