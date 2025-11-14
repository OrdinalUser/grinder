#version 450 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform int uHorizontal;

uniform float weight[5] = float[] (0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main() {
    vec2 texOffset = 1.0 / textureSize(uTexture, 0);
    vec3 result = texture(uTexture, vUV).rgb * weight[0];
    
    if (uHorizontal == 1) {
        for(int i = 1; i < 5; ++i) {
            result += texture(uTexture, vUV + vec2(texOffset.x * i, 0.0)).rgb * weight[i];
            result += texture(uTexture, vUV - vec2(texOffset.x * i, 0.0)).rgb * weight[i];
        }
    } else {
        for(int i = 1; i < 5; ++i) {
            result += texture(uTexture, vUV + vec2(0.0, texOffset.y * i)).rgb * weight[i];
            result += texture(uTexture, vUV - vec2(0.0, texOffset.y * i)).rgb * weight[i];
        }
    }
    
    FragColor = vec4(result, 1.0);
}