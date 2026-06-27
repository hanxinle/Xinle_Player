#version 330 core

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D image;
uniform float intensity;   // 来自 effects.json 的参数，范围 [0, 100]

void main() {
    vec4 color = texture(image, vTexCoord);

    // 将强度从 [0, 100] 映射到 [0, 1]。
    float i = intensity * 0.01;

    // 怀旧褐色调（Sepia）矩阵。
    vec3 sepia = vec3(
        dot(color.rgb, vec3(0.393, 0.769, 0.189)),
        dot(color.rgb, vec3(0.349, 0.686, 0.168)),
        dot(color.rgb, vec3(0.272, 0.534, 0.131))
    );

    // 在原图与褐色调之间按强度插值。
    vec3 finalColor = mix(color.rgb, sepia, i);

    FragColor = vec4(finalColor, color.a);
}
