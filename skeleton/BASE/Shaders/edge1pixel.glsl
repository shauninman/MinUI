#version 130

// Insert a single interpolated display (high-res)
// pixel row between each pair of adjacent source (low-res) pixels.
// by decavoid


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
COMPAT_VARYING vec4 COL0;
COMPAT_VARYING vec4 TEX0;
COMPAT_VARYING vec2 INVERSE_SCALE_HALF;

uniform mat4 MVPMatrix;
uniform COMPAT_PRECISION int FrameDirection;
uniform COMPAT_PRECISION int FrameCount;
uniform COMPAT_PRECISION vec2 OutputSize;
uniform COMPAT_PRECISION vec2 TextureSize;
uniform COMPAT_PRECISION vec2 InputSize;

// vertex compatibility #defines
#define vTexCoord TEX0.xy
#define SourceSize vec4(TextureSize, 1.0 / TextureSize) //either TextureSize or InputSize

void main()
{
    gl_Position = MVPMatrix * VertexCoord;
    TEX0.xy = TexCoord.xy * TextureSize.xy; // NES x: [0; 256], y: [0; 240]
    INVERSE_SCALE_HALF = InputSize / OutputSize * 0.5;
}

#elif defined(FRAGMENT)

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

#if __VERSION__ >= 130
#define COMPAT_VARYING in
#define COMPAT_TEXTURE texture
out COMPAT_PRECISION vec4 FragColor;
#else
#define COMPAT_VARYING varying
#define FragColor gl_FragColor
#define COMPAT_TEXTURE texture2D
#endif

uniform COMPAT_PRECISION int FrameDirection;
uniform COMPAT_PRECISION int FrameCount;
uniform COMPAT_PRECISION vec2 OutputSize;
uniform COMPAT_PRECISION vec2 TextureSize;
uniform COMPAT_PRECISION vec2 InputSize;
uniform sampler2D Texture;
COMPAT_VARYING vec4 TEX0;
COMPAT_VARYING vec2 INVERSE_SCALE_HALF;

// fragment compatibility #defines
#define Source Texture
#define vTexCoord TEX0.xy

#define SourceSize vec4(TextureSize, 1.0 / TextureSize) //either TextureSize or InputSize

vec2 isInside(vec2 v, vec2 left, vec2 right)
{
	return step(left, v) - step(right, v);
}

// uncomment to see a red grid of modified pixels
//#define DEBUG_DRAW_EDGES

void main()
{
	vec2 pixelCoords = vTexCoord;
	vec2 iPixelCoords = floor(pixelCoords);
	vec2 coordAtPixelCenter = iPixelCoords + vec2(0.5);
	vec2 coordBetweenPixels = round(pixelCoords);
	vec2 f = pixelCoords - iPixelCoords + vec2(1e-3);
	vec2 isFractionInside = isInside(f, INVERSE_SCALE_HALF, 1.0 - INVERSE_SCALE_HALF);

/*
	Equivalent:
	if (isFractionInside.x)
	{
		// shift coord to the nearest pixel center to prevent interpolation
		newCoord.x = coordAtPixelCenter.x;
	}
	else
	{
		// shift exactly between pixels for perfect interpolation
		newCoord.x = coordBetweenPixels.x;
	}
*/

	vec2 newCoord = isFractionInside * coordAtPixelCenter + (1.0 - isFractionInside) * coordBetweenPixels;
	vec2 newTexCoord = newCoord * SourceSize.zw;
	FragColor = vec4(COMPAT_TEXTURE(Source, newTexCoord).rgb, 1.0);

#ifdef DEBUG_DRAW_EDGES
	if (isFractionInside.x + isFractionInside.y < 2)
		FragColor.r = 1;
#endif
}

#endif
