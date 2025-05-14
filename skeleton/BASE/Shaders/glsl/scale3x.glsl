/*
	Scale3x

Filter:	Nearest
Scale:	3x

Scale3x is real-time graphics effect able to increase the size of small bitmaps guessing the missing pixels without blurring the images.
It was originally developed for the AdvanceMAME project in the year 2001 to improve the quality of old games with a low video resolution.

Homepage: http://scale2x.sourceforge.net/
Copyright (C) 2001, 2002, 2003, 2004 Andrea Mazzoleni 
License: GNU-GPL  

*/

#define saturate(c) clamp(c, 0.0, 1.0)
#define lerp(a,b,c) mix(a,b,c)
#define mul(a,b) (b*a)
#define fmod(c,d) mod(c,d)
#define frac(c) fract(c)
#define tex2D(c,d) COMPAT_TEXTURE(c,d)
#define float2 vec2
#define float3 vec3
#define float4 vec4
#define int2 ivec2
#define int3 ivec3
#define int4 ivec4
#define bool2 bvec2
#define bool3 bvec3
#define bool4 bvec4
#define float2x2 mat2x2
#define float3x3 mat3x3
#define float4x4 mat4x4

#define decal Source

bool eq(float3 A, float3 B){
	return (A==B);
}

bool neq(float3 A, float3 B){
	return (A!=B);
}

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
COMPAT_VARYING vec4 t1;
COMPAT_VARYING vec4 t2;
COMPAT_VARYING vec4 t3;

uniform mat4 MVPMatrix;
uniform COMPAT_PRECISION int FrameDirection;
uniform COMPAT_PRECISION int FrameCount;
uniform COMPAT_PRECISION vec2 OutputSize;
uniform COMPAT_PRECISION vec2 TextureSize;
uniform COMPAT_PRECISION vec2 InputSize;

// vertex compatibility #defines
#define vTexCoord TEX0.xy
#define SourceSize vec4(TextureSize, 1.0 / TextureSize) //either TextureSize or InputSize
#define outsize vec4(OutputSize, 1.0 / OutputSize)

void main()
{
    gl_Position = MVPMatrix * VertexCoord;
    COL0 = COLOR;
    TEX0.xy = TexCoord.xy;
	
	float2 ps = float2(SourceSize.z, SourceSize.w);
	float dx = ps.x;
	float dy = ps.y;

	t1 = TEX0.xxxy + float4(-dx,  0, dx,-dy);	// A, B, C
	t2 = TEX0.xxxy + float4(-dx,  0, dx,  0);	// D, E, F
	t3 = TEX0.xxxy + float4(-dx,  0, dx, dy);	// G, H, I
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
COMPAT_VARYING vec4 TEX0;
COMPAT_VARYING vec4 t1;
COMPAT_VARYING vec4 t2;
COMPAT_VARYING vec4 t3;

// fragment compatibility #defines
#define Source Texture
#define vTexCoord TEX0.xy

#define SourceSize vec4(TextureSize, 1.0 / TextureSize) //either TextureSize or InputSize
#define outsize vec4(OutputSize, 1.0 / OutputSize)

void main()
{
	// subpixel determination
	float2 fp = floor(3.0 * frac(vTexCoord*SourceSize.xy));

	/*
		A B C		E0 E1 E2
		D E F		E3 E4 E5
		G H I		E6 E7 E8
	*/

	// reading the texels
	float3 A = tex2D(decal, t1.xw).xyz;
	float3 B = tex2D(decal, t1.yw).xyz;
	float3 C = tex2D(decal, t1.zw).xyz;
	float3 D = tex2D(decal, t2.xw).xyz;
	float3 E = tex2D(decal, t2.yw).xyz;
	float3 F = tex2D(decal, t2.zw).xyz;
	float3 G = tex2D(decal, t3.xw).xyz;
	float3 H = tex2D(decal, t3.yw).xyz;
	float3 I = tex2D(decal, t3.zw).xyz;

	// equality checks
	bool eqBD = eq(B,D), eqBF = eq(B,F), eqHD = eq(H,D), eqHF = eq(H,F), neqEA = neq(E,A), neqEC = neq(E,C), neqEG = neq(E,G), neqEI = neq(E,I); 

	// rules
	float3 E0 = eqBD ? B : E;
	float3 E1 = eqBD && neqEC || eqBF && neqEA ? B : E;
	float3 E2 = eqBF ? B : E;
	float3 E3 = eqBD && neqEG || eqHD && neqEA ? D : E;
	float3 E5 = eqBF && neqEI || eqHF && neqEC ? F : E;
	float3 E6 = eqHD ? H : E;
	float3 E7 = eqHD && neqEI || eqHF && neqEG ? H : E;
	float3 E8 = eqHF ? H : E;

	// general condition & subpixel output
	FragColor = vec4(neq(B,H) && neq(D,F) ? (fp.y == 0. ? (fp.x == 0. ? E0 : fp.x == 1. ? E1 : E2) : (fp.y == 1. ? (fp.x == 0. ? E3 : fp.x == 1. ? E : E5) : (fp.x == 0. ? E6 : fp.x == 1. ? E7 : E8))) : E, 1.0);
} 
#endif
