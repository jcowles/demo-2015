// Created by Jeremy Cowles, 2015

#version 410 core

in vec4 fragColor;
in vec2 uvCoord;
out vec4 color;
uniform float     iRandom;
uniform vec3      iResolution;           // viewport resolution (in pixels)
uniform float     iGlobalTime;           // shader playback time (in seconds)
uniform sampler2D iChannel0;
uniform sampler2D iChannel1;
void main( void )
{
    vec2 xy = -1.0 + 2.0*gl_FragCoord.xy/iResolution.xy;
    // Multiplying by random creates spazzing chunky grains
    float grain = texture(iChannel0, xy + iRandom, 0).x;
    grain = grain*2.0 - 1.0;
    grain = (grain*grain) * 0.04;
    vec4 col = texture(iChannel1, vec2(uvCoord.x*.5, uvCoord.y*.5 - .125), 0).rgba;
    if (uvCoord.y < .75 && uvCoord.y > .25)
        col.rgb += grain;
	color = col;
}
