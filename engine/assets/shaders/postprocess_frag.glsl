#version 450 core

in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uSceneTexture;
uniform sampler2D uBloomTexture;
uniform float uBloomStrength;

void main() {
    vec3 scene = texture(uSceneTexture, vUV).rgb;
    vec3 bloom = texture(uBloomTexture, vUV).rgb;
    vec3 composite = scene + bloom * uBloomStrength;

    // Gamma correction
    vec3 gamma_corrected = pow(composite, vec3(1.0/2.2));

    FragColor = vec4(gamma_corrected, 1.0);
}
