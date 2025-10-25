#version 320 es

uniform mat4 uModelViewProjectionMatrix;

layout (location = 0) in vec2 iPos;
layout (location = 1) in vec2 iUv;

out vec2 vUv;

void main() {
    vUv = iUv;
    // the matrix mult is not needed but its there by the pdf requirements
    gl_Position = uModelViewProjectionMatrix * vec4(iPos.x, iPos.y, 0.0, 1.0);
}