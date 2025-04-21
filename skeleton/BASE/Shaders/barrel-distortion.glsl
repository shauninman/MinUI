// barrel distortion only shader from davej
// http://blog.petrockblock.com/forums/topic/help-with-opelgl-barrel-distortion/#post-106535

/* COMPATIBILITY
   - GLSL compilers
*/

/*
    barrel-distortion

    Copyright (C) 2015 davej

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.
*/

#if defined(VERTEX)
uniform mediump mat4 MVPMatrix;
attribute mediump vec4 VertexCoord;
attribute mediump vec2 TexCoord;

varying mediump vec2 TEX0;

void main()
{
TEX0 = TexCoord;
gl_Position = MVPMatrix * VertexCoord;
}
#elif defined(FRAGMENT)
uniform sampler2D Texture;
uniform vec2 InputSize;
uniform vec2 TextureSize;
varying vec2 TEX0;

const mediump float BARREL_DISTORTION = 0.25;
const mediump float rescale = 1.0 - (0.25 * BARREL_DISTORTION);

void main()
{
vec2 scale = TextureSize / InputSize;
vec2 tex0 = TEX0 * scale;
vec2 texcoord = tex0 - vec2(0.5);
float rsq = texcoord.x * texcoord.x + texcoord.y * texcoord.y;
texcoord = texcoord + (texcoord * (BARREL_DISTORTION * rsq));
texcoord *= rescale;

if (abs(texcoord.x) > 0.5 || abs(texcoord.y) > 0.5)
gl_FragColor = vec4(0.0);
else
{
texcoord += vec2(0.5);
texcoord /= scale;
vec3 colour = texture2D(Texture, texcoord).rgb;

gl_FragColor = vec4(colour,1.0);
}
}
#endif
