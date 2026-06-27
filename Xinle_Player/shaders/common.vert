#version 330 core

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;

out vec2 vTexCoord;

uniform mat4 mvp;
uniform float flipY = 0.0;

void main() {
    vTexCoord = aTexCoord;
    if (flipY > 0.5) {
        vTexCoord.y = 1.0 - vTexCoord.y;
    }
    gl_Position = mvp * vec4(aPos, 0.0, 1.0);
}
