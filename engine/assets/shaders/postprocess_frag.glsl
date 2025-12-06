#version 450 core

in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uSceneTexture;
uniform sampler2D uBloomTexture;
uniform float uBloomStrength;

vec3 ToneMap_Reinhard(vec3 color) {
    return color / (1.0 + color);
}

vec3 ToneMap_ReinhardExtended(vec3 color, float maxWhite) {
    vec3 numerator = color * (1.0 + (color / vec3(maxWhite * maxWhite)));
    return numerator / (1.0 + color);
}

vec3 ToneMap_Aces_Fitted(vec3 color) {
    // ACES approximation by Krzysztof Narkowicz
    // Source: https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
    
    color *= 0.6; // Exposure adjustment
    
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

void main() {
    vec3 scene = texture(uSceneTexture, vUV).rgb;
    vec3 bloom = texture(uBloomTexture, vUV).rgb;
    vec3 composite = scene + bloom * uBloomStrength;
    vec3 tonemapped = ToneMap_Aces_Fitted(composite);

    // Gamma correction
    vec3 gamma_corrected = pow(tonemapped, vec3(1.0/2.2));

    FragColor = vec4(gamma_corrected, 1.0);
}
