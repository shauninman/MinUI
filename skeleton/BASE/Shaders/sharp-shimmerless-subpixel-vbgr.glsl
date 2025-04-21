/*
 * sharp-shimmerless-vrgb
 * Author: zadpos
 * License: Public domain
 * 
 * Sharp-Shimmerless shader for v-RGB subpixels
 */

#if defined(VERTEX)

#if __VERSION__ >= 130
#define COMPAT_VARYING out
#define COMPAT_ATTRIBUTE in
#define COMPAT_TEXTURE texture
#else
#define COMPAT_VARYING varying 
#define COMPAT_ATTRIBUTE attribute 
#define COMPAT_TEXTURE texture2D
#endif

#ifdef GL_ES
#define COMPAT_PRECISION mediump
#else
#define COMPAT_PRECISION
#endif

COMPAT_ATTRIBUTE vec4 VertexCoord;
COMPAT_ATTRIBUTE vec4 COLOR;
COMPAT_ATTRIBUTE vec4 TexCoord;
COMPAT_VARYING vec4 pixel;
COMPAT_VARYING vec4 scale;
COMPAT_VARYING vec4 invscale;

uniform mat4 MVPMatrix;
uniform COMPAT_PRECISION int FrameDirection;
uniform COMPAT_PRECISION int FrameCount;
uniform COMPAT_PRECISION vec2 OutputSize;
uniform COMPAT_PRECISION vec2 TextureSize;
uniform COMPAT_PRECISION vec2 InputSize;

void main()
{
    gl_Position = MVPMatrix * VertexCoord;

    vec2 pixel_xy = TexCoord.xy * OutputSize * TextureSize / InputSize;
    vec2 scale_xy = OutputSize / InputSize;
    vec2 invscale_xy = InputSize / OutputSize;
    
    pixel = pixel_xy.xyyy;
    scale = scale_xy.xyyy;
    invscale = invscale_xy.xyyy;
}

#elif defined(FRAGMENT)

#if __VERSION__ >= 130
#define COMPAT_VARYING in
#define COMPAT_TEXTURE texture
out vec4 FragColor;
#else
#define COMPAT_VARYING varying
#define FragColor gl_FragColor
#define COMPAT_TEXTURE texture2D
#endif

#ifdef GL_ES
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif
#define COMPAT_PRECISION mediump
#else
#define COMPAT_PRECISION
#endif

uniform COMPAT_PRECISION int FrameDirection;
uniform COMPAT_PRECISION int FrameCount;
uniform COMPAT_PRECISION vec2 OutputSize;
uniform COMPAT_PRECISION vec2 TextureSize;
uniform COMPAT_PRECISION vec2 InputSize;
uniform sampler2D Texture;
COMPAT_VARYING vec4 pixel;
COMPAT_VARYING vec4 scale;
COMPAT_VARYING vec4 invscale;

void main()
{
    vec4 pixel_tl = floor(pixel);
    pixel_tl.y -= 0.33;
    pixel_tl.w += 0.33;
    vec4 pixel_br = ceil(pixel);
    pixel_br.y -= 0.33;
    pixel_br.w += 0.33;

    vec4 texel_tl = floor(invscale * pixel_tl);
    vec4 texel_br = floor(invscale * pixel_br);

    vec4 mod_texel = texel_br + vec4(0.5, 0.5, 0.5, 0.5);
    mod_texel -= (vec4(1.0, 1.0, 1.0, 1.0) - step(texel_br, texel_tl)) * (scale * texel_br - pixel_tl);

    FragColor.b = COMPAT_TEXTURE(Texture, mod_texel.xy / TextureSize).b;
    FragColor.g = COMPAT_TEXTURE(Texture, mod_texel.xz / TextureSize).g;
    FragColor.r = COMPAT_TEXTURE(Texture, mod_texel.xw / TextureSize).r;
    FragColor.a = 1.0;
} 
#endif
