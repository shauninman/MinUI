/*
 * sharp-shimmerless-gridlines
 * Author: zadpos
 * License: Public domain
 * 
 * A retro gaming shader for sharpest pixels with no aliasing/shimmering.
 * Instead of pixels as point samples, this shader considers pixels as
 * ideal rectangles forming a grid, and interpolates pixel by calculating
 * the surface area an input pixel would occupy on an output pixel.
 *
 * This shader addes gridlines to the sharp-shimmerless shader,
 * with adjustable grid (vertical) and scanline (horizontal) thickness and opacity.
 */

// Parameter resolution: 0.01, updated per suggestion from SirPrimalform
#pragma parameter GRID_RATIO_Y "Gridline Thickness (Horizontal)" 0.3 0.0 1.0 0.01
#pragma parameter GRID_RATIO_X "Gridline Thickness (Vertical)" 0.3 0.0 1.00 0.01
#pragma parameter GRID_OPACITY_Y "Gridline Opacity (Horizontal)" 0.3 0.0 1.0 0.01
#pragma parameter GRID_OPACITY_X "Gridline Opacity (Vertical)" 0.3 0.0 1.0 0.01

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
COMPAT_VARYING vec2 pixel;
COMPAT_VARYING vec2 scale;
COMPAT_VARYING vec2 invscale;

uniform mat4 MVPMatrix;
uniform COMPAT_PRECISION int FrameDirection;
uniform COMPAT_PRECISION int FrameCount;
uniform COMPAT_PRECISION vec2 OutputSize;
uniform COMPAT_PRECISION vec2 TextureSize;
uniform COMPAT_PRECISION vec2 InputSize;

void main()
{
    gl_Position = MVPMatrix * VertexCoord;
    pixel = TexCoord.xy * OutputSize * TextureSize / InputSize;
    scale = OutputSize / InputSize;
    invscale = InputSize / OutputSize;
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
COMPAT_VARYING vec2 pixel;
COMPAT_VARYING vec2 scale;
COMPAT_VARYING vec2 invscale;

#ifdef PARAMETER_UNIFORM
// All parameter floats need to have COMPAT_PRECISION in front of them
uniform COMPAT_PRECISION float GRID_RATIO_X;
uniform COMPAT_PRECISION float GRID_RATIO_Y;
uniform COMPAT_PRECISION float GRID_OPACITY_X;
uniform COMPAT_PRECISION float GRID_OPACITY_Y;
#else
#define GRID_RATIO_X 0.3
#define GRID_RATIO_Y 0.3
#define GRID_OPACITY_X 0.3
#define GRID_OPACITY_Y 0.3
#endif

void main()
{
    // pixel: output screen pixels
    // texel: input texels == game pixels
    vec2 pixel_tl = invscale * floor(pixel);   // input pixel-coordinates of the output pixel's top-left corner
    vec2 pixel_br = invscale * ceil(pixel);    // input pixel-coordinates of the output pixel's bottom-right corner
    vec2 texel_tl = floor(pixel_tl); // texel the top-left of the pixel lies in
    vec2 texel_br = floor(pixel_br); // texel the bottom-right of the pixel lies in

    // grid ratio; 1.0 means no grid/scanline effect
    vec2 grid_ratio = vec2(1.0 - GRID_RATIO_X, 1.0 - GRID_RATIO_Y);
    // brightness of the grid lines; 1.0 means no grid/scanline effect
    vec2 grid_alpha = vec2(1.0 - GRID_OPACITY_X, 1.0 - GRID_OPACITY_Y);

    vec2 area_tl = mix(
        grid_ratio - fract(pixel_tl) + grid_alpha * (1.0 - grid_ratio),
        grid_alpha * (1.0 - grid_ratio),
        step(grid_ratio, fract(pixel_tl)));
    vec2 area_br = mix(
        fract(pixel_br),
        grid_ratio + grid_alpha * (fract(pixel_br) - grid_ratio),
        step(grid_ratio, fract(pixel_br)));
    vec2 interp_ratio = area_br / (area_tl + area_br);
    interp_ratio = mix(interp_ratio, vec2(1.0, 1.0), step(texel_br, texel_tl));

    vec2 scanline_factor = (fract(pixel_br) + clamp(grid_ratio - fract(pixel_tl), 0.0, 1.0)) / invscale;
    scanline_factor = mix(scanline_factor, grid_ratio / invscale, step(grid_ratio, fract(pixel_br)));
    scanline_factor = mix(scanline_factor, clamp((grid_ratio - fract(pixel_tl)) / invscale, 0.0, 1.0), step(texel_br, texel_tl));
    scanline_factor = grid_alpha + (1.0 - grid_alpha) * scanline_factor;
    float factor = scanline_factor.x * scanline_factor.y;

    vec2 mod_texel = texel_br - vec2(0.5, 0.5) + interp_ratio;

    FragColor = vec4(factor * COMPAT_TEXTURE(Texture, mod_texel / TextureSize).rgb, 1.0);
} 
#endif
