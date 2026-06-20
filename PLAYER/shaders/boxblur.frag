#version 330 core

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D image;
uniform float radius;
uniform vec2 resolution;
uniform bool horiz_blur;
uniform bool vert_blur;
uniform int iteration;

void main() {
    float rad = ceil(radius);
    float divider = 1.0 / rad;
    vec4 color = vec4(0.0);
    bool radius_is_zero = (rad == 0.0);

    vec2 fragCoord = vTexCoord * resolution;

    if (iteration == 0 && horiz_blur && !radius_is_zero) {
        for (float x = -rad + 0.5; x <= rad; x += 2.0) {
            color += texture(image, (vec2(fragCoord.x + x, fragCoord.y)) / resolution) * divider;
        }
        FragColor = color;
    } else if (iteration == 1 && vert_blur && !radius_is_zero) {
        for (float y = -rad + 0.5; y <= rad; y += 2.0) {
            color += texture(image, (vec2(fragCoord.x, fragCoord.y + y)) / resolution) * divider;
        }
        FragColor = color;
    } else {
        FragColor = texture(image, vTexCoord);
    }
}
