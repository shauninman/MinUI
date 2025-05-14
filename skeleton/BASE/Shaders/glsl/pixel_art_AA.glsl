#version 110

/*
   Pixel art AA shader by DariusG 2023
   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 2 of the License, or (at your option)
   any later version.
*/

#pragma parameter AMSTRAD "Amstrad wide pixel art" 0.0 0.0 1.0 1.0


#define iTime float(FrameCount)
#define pi 3.141592
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

vec4 _oPosition1; 
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

void main()
{
    gl_Position = MVPMatrix * VertexCoord;
    TEX0.xy = TexCoord.xy;
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

// compatibility #defines
#define Source Texture
#define vTexCoord TEX0.xy

#define SourceSize vec4(TextureSize, 1.0 / TextureSize) //either TextureSize or InputSize
#define OutSize vec4(OutputSize, 1.0 / OutputSize)

#ifdef PARAMETER_UNIFORM
uniform COMPAT_PRECISION float AMSTRAD;

#else
#define AMSTRAD 0.0
#endif


/*
A B C 
D E F
G H I
*/



void main()
{
vec2 pos = vTexCoord;
float dx = AMSTRAD == 1.0 ? 2.0*SourceSize.z : SourceSize.z;
float dy = SourceSize.w;

vec3 A = COMPAT_TEXTURE(Source,vTexCoord + vec2(dx,-dy)).rgb;
vec3 B = COMPAT_TEXTURE(Source,vTexCoord + vec2(0.0,-dy)).rgb;
vec3 C = COMPAT_TEXTURE(Source,vTexCoord + vec2(-dx,-dy)).rgb;
vec3 D = COMPAT_TEXTURE(Source,vTexCoord + vec2(dx,0.0)).rgb;
vec3 E = COMPAT_TEXTURE(Source,vTexCoord).rgb;
vec3 F = COMPAT_TEXTURE(Source,vTexCoord + vec2(-dx,0.0)).rgb;
vec3 G = COMPAT_TEXTURE(Source,vTexCoord + vec2(dx,dy)).rgb;
vec3 H = COMPAT_TEXTURE(Source,vTexCoord + vec2(0.0,dy)).rgb;
vec3 I = COMPAT_TEXTURE(Source,vTexCoord + vec2(-dx,dy)).rgb;

#define lumweight vec3(0.3,0.6,0.1)
#define lum(c) dot(lumweight,c)

//pattern type 1
float L = (D == B && B == C && E != D && B !=A) ? 1.0 : 0.0;
float R = (A == B && A == F && E != F && B !=C) ? 1.0 : 0.0;
float DL = (D == H && D == I && E != D && H != G) ? 1.0 : 0.0;
float DR = (F == G && F == H && E != F && H != I) ? 1.0 : 0.0;

E = (L == 1.0 && lum(E)<lum(D) || DL == 1.0 && lum(E)<lum(D)) ? (E+D)/2.0 : E;
E = (R == 1.0 && lum(E)<lum(F) || DR == 1.0 && lum(E)<lum(F)) ? (E+F)/2.0 : E;

//pattern type 2
float GL = (E == H && E == F && E != D) ? 1.0 : 0.0;
float GR = (E == D && E == H && E != F) ? 1.0 : 0.0;
float GDL = (E == B && E == F && E != D) ? 1.0 : 0.0;
float GDR = (E == D && E == B && E != F) ? 1.0 : 0.0;

E = (GL == 1.0 && lum(E)>lum(D) || GDL == 1.0 && lum(E)>lum(D)) ? (E+D)/2.0 : E;
E = (GR == 1.0 && lum(E)>lum(F) || GDR == 1.0 && lum(E)>lum(F)) ? (E+F)/2.0 : E;

//pattern type 3
float SL  = (B == D && B == G && E != D) ? 1.0 : 0.0;
float SR  = (B == F && B == I && E != F) ? 1.0 : 0.0;
float SDL = (H == D && H == A && E != D) ? 1.0 : 0.0;
float SDR = (H == F && H == C && E != F) ? 1.0 : 0.0;

E = (SL == 1.0 && lum(E)<lum(D) || SDL == 1.0 && lum(E)<lum(D)) ? (E+D)/2.0 : E;
E = (SR == 1.0 && lum(E)<lum(F) || SDR == 1.0 && lum(E)<lum(F)) ? (E+F)/2.0 : E;



FragColor = vec4(E,1.0);
}
#endif