#version 330 core

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D image;
uniform float amount;

void main() {
    vec4 textureColor = texture(image, vTexCoord);
    float amount_val = amount * 0.01;
    vec3 col = textureColor.rgb +
               ((vec3(1.0) - textureColor.rgb - textureColor.rgb) * vec3(amount_val));
    FragColor = vec4(col, textureColor.a);
}
