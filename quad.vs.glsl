// Created by Jeremy Cowles, 2015

#version 410 core

layout(location=0) in vec2 position;
out vec4 fragColor;
out vec2 uvCoord;

void main() {
    fragColor = vec4((position + 1)*.5*0.75, 0.0, 1);
    uvCoord = (position + 1)*.5;
    gl_Position = vec4(position, 0.0, 1);
}
