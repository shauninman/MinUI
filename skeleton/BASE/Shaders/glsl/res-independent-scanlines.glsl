/*
    Resolution-Independent Scanlines
    based on
    Scanlines Sine Absolute Value
    An ultra light scanline shader
    by RiskyJumps
    license: public domain
*/

#pragma parameter amp          "Amplitude"      1.2500  0.000 2.000 0.05
#pragma parameter phase        "Phase"          0.5000  0.000 2.000 0.05
#pragma parameter lines_black  "Lines Blacks"   0.0000  0.000 1.000 0.05
#pragma parameter lines_white  "Lines Whites"   1.0000  0.000 2.000 0.05
#pragma parameter imageSize "Simulated Image Height" 224.0 144.0 288.0 1.0
#pragma parameter autoscale "Automatic Scale" 0.0 0.0 1.0 1.0

#define freq             0.500000
#define offset           0.000000
#define pi               3.141592654

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
COMPAT_ATTRIBUTE vec4 TexCoord;
COMPAT_VARYING vec4 TEX0;
COMPAT_VARYING float omega;

uniform mat4 MVPMatrix;
uniform COMPAT_PRECISION int FrameDirection;
uniform COMPAT_PRECISION int FrameCount;
uniform COMPAT_PRECISION vec2 OutputSize;
uniform COMPAT_PRECISION vec2 TextureSize;
uniform COMPAT_PRECISION vec2 InputSize;

// compatibility #defines
#define vTexCoord TEX0.xy
#define SourceSize vec4(TextureSize, 1.0 / TextureSize) //either TextureSize or InputSize
#define OutSize vec4(OutputSize, 1.0 / OutputSize)

#define pi 3.141592654

void main()
{
   gl_Position = MVPMatrix * VertexCoord;
   TEX0.xy = TexCoord.xy;
   omega = 2.0 * pi * freq;              // Angular frequency
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
COMPAT_VARYING float omega;

// compatibility #defines
#define Source Texture
#define vTexCoord TEX0.xy

#define SourceSize vec4(TextureSize, 1.0 / TextureSize) //either TextureSize or InputSize
#define OutSize vec4(OutputSize, 1.0 / OutputSize)


#ifdef PARAMETER_UNIFORM
uniform COMPAT_PRECISION float amp, phase, lines_black,  lines_white,
         imageSize,  autoscale;
#else
#define amp  1.25
#define phase  0.5
#define lines_black 0.0
#define lines_white 1.0
#define imageSize 224.0
#define autoscale 0.0

#endif

void main()
{
    float scale = imageSize; if (autoscale == 1.0) scale = InputSize.y; 
    float angle = (gl_FragCoord.y * OutSize.w) * omega * scale + phase;
    vec3 color = COMPAT_TEXTURE(Source, vTexCoord).xyz;
    float grid;
 
    float lines;
 
    lines = sin(angle);
    lines *= amp;
    lines += offset;
    lines = abs(lines);
    lines *= lines_white - lines_black;
    lines += lines_black;
    color *= lines;
 
    FragColor = vec4(color.xyz, 1.0);
}

#endif
