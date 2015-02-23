// Created by Jeremy Cowles, 2015

#version 410 core

uniform vec3      iResolution;           // viewport resolution (in pixels)
uniform sampler2D iChannel1;

layout(location=0) in vec2 position;

out vec2 uvCoord;
flat out float deadSpace;
out vec4 posPos;

void main() {
    uvCoord = (position + 1)*.5;

    vec2 texSize = textureSize(iChannel1, 0);
    float widthScale = iResolution.x / texSize.x;

    // x/y = aspect(e.g. 2.39) but below we're solving for Y
    // so we need the reciprocal.
    float a = 1.0/(iResolution.x/iResolution.y);

    // How much black space would be on the screen if we imposed the input
    // aspect on the output image?
    deadSpace = (texSize.y - (texSize.x*a)) / texSize.y;

    // Distribute dead space evenly between top and bottom of frame.
    deadSpace /= 2.0;

    // The X coordinate is easy, we know we want it to fill the entire screen,
    // so we just multiply by the widthScale. 
    // 
    // The Y coordinate is tricky, because we want to scale up to the exact
    // aspect of the input image and leave variable sized black bars above and
    // below. Therefore, we first offset the local-uv-space Y by the deadspace
    // computed above. Next, we scale that uv-coord by the widthScale.
    uvCoord = vec2(widthScale*uvCoord.x, widthScale*(uvCoord.y - deadSpace));
    deadSpace *= widthScale * .5*widthScale;

    float FXAA_SUBPIX_SHIFT = 1.0/4.0;
    vec2 rcp = 1/iResolution.xy;
    posPos.xy = uvCoord;
    posPos.zw = uvCoord - rcp * (.5 - FXAA_SUBPIX_SHIFT);

    gl_Position = vec4(position, 0.0, 1);
}
