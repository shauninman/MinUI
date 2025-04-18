/*
   Sharpsmoother shader
   
   Copyright (C) 2005-2017 guest(r) - guest.r@gmail.com

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#pragma parameter max_w   "Max filter weight"  0.10  0.00 0.20 0.01
#pragma parameter min_w   "Min filter weight" -0.07 -0.15 0.05 0.01
#pragma parameter smoot   "Smoothing strength" 0.55  0.00 1.50 0.01
#pragma parameter lumad   "Effects smoothing"  0.30  0.10 5.00 0.10
#pragma parameter mtric   "The metric we use"  0.70  0.10 2.00 0.10

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
COMPAT_VARYING vec4 t4;

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
   float x = 1.0 * (1.0 / SourceSize.x);
   float y = 1.0 * (1.0 / SourceSize.y);
   vec2 dg1 = vec2( x, y);
   vec2 dg2 = vec2(-x, y);
   vec2 dx = vec2(x, 0.0);
   vec2 dy = vec2(0.0, y);
   t1 = vec4(vTexCoord.xy-dg1,vTexCoord.xy-dy);
   t2 = vec4(vTexCoord.xy-dg2,vTexCoord.xy+dx);
   t3 = vec4(vTexCoord.xy+dg1,vTexCoord.xy+dy);
   t4 = vec4(vTexCoord.xy+dg2,vTexCoord.xy-dx);
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
COMPAT_VARYING vec4 t4;

// compatibility #defines
#define Source Texture
#define vTexCoord TEX0.xy

#define SourceSize vec4(TextureSize, 1.0 / TextureSize) //either TextureSize or InputSize
#define OutSize vec4(OutputSize, 1.0 / OutputSize)

#ifdef PARAMETER_UNIFORM
uniform COMPAT_PRECISION float max_w;
uniform COMPAT_PRECISION float min_w;
uniform COMPAT_PRECISION float smoot;
uniform COMPAT_PRECISION float lumad;
uniform COMPAT_PRECISION float mtric;
#else 
#define max_w    0.10
#define min_w   -0.07
#define smoot    0.55
#define lumad    0.30
#define mtric    0.70
#endif

vec3 dt = vec3(1.0, 1.0, 1.0);


float wt(vec3 A, vec3 B)
{	
	return clamp(smoot - ((6.0+lumad)/pow(3.0,mtric))*pow(dot(pow(abs(A-B),vec3(1.0/mtric)),dt),mtric)/(dot(A+B,dt)+lumad), min_w, max_w);
}

void main()
{
   vec3 c00 = COMPAT_TEXTURE(Source, t1.xy).xyz; 
   vec3 c10 = COMPAT_TEXTURE(Source, t1.zw).xyz; 
   vec3 c20 = COMPAT_TEXTURE(Source, t2.xy).xyz; 
   vec3 c01 = COMPAT_TEXTURE(Source, t4.zw).xyz; 
   vec3 c11 = COMPAT_TEXTURE(Source, vTexCoord.xy).xyz; 
   vec3 c21 = COMPAT_TEXTURE(Source, t2.zw).xyz; 
   vec3 c02 = COMPAT_TEXTURE(Source, t4.xy).xyz; 
   vec3 c12 = COMPAT_TEXTURE(Source, t3.zw).xyz; 
   vec3 c22 = COMPAT_TEXTURE(Source, t3.xy).xyz;
       
   float w10 = wt(c11,c10);
   float w21 = wt(c11,c21);
   float w12 = wt(c11,c12);
   float w01 = wt(c11,c01);
   float w00 = wt(c11,c00)*0.75;
   float w22 = wt(c11,c22)*0.75;
   float w20 = wt(c11,c20)*0.75;
   float w02 = wt(c11,c02)*0.75;

   FragColor = vec4(w10*c10+w21*c21+w12*c12+w01*c01+w00*c00+w22*c22+w20*c20+w02*c02+(1.0-w10-w21-w12-w01-w00-w22-w20-w02)*c11, 1.0);
} 
#endif
