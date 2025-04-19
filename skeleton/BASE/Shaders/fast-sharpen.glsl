/*
   Fast Sharpen Shader
   
   Copyright (C) 2005 - 2019 guest(r) - guest.r@gmail.com

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

#pragma parameter SHARPEN  "Sharpen strength"       1.00 0.0 2.00 0.05 
#pragma parameter CONTR    "Ammount of sharpening"  0.07 0.0 0.25 0.01 
#pragma parameter DETAILS  "Details sharpened "     1.00 0.0 1.00 0.05 

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

vec4 _oPosition1; 
uniform mat4 MVPMatrix;
uniform COMPAT_PRECISION int FrameDirection;
uniform COMPAT_PRECISION int FrameCount;
uniform COMPAT_PRECISION vec2 OutputSize;
uniform COMPAT_PRECISION vec2 TextureSize;
uniform COMPAT_PRECISION vec2 InputSize;
COMPAT_VARYING vec2 g10;
COMPAT_VARYING vec2 g01;
COMPAT_VARYING vec2 g12;
COMPAT_VARYING vec2 g21;

// compatibility #defines
#define vTexCoord TEX0.xy
#define SourceSize vec4(TextureSize, 1.0 / TextureSize) //either TextureSize or InputSize
#define OutSize vec4(OutputSize, 1.0 / OutputSize)

void main()
{
    gl_Position = MVPMatrix * VertexCoord;
    TEX0.xy = TexCoord.xy * 1.00001;
   g10 = vec2( 0.3333,-1.0)*SourceSize.zw;
   g01 = vec2(-1.0,-0.3333)*SourceSize.zw;
   g12 = vec2(-0.3333, 1.0)*SourceSize.zw;
   g21 = vec2( 1.0, 0.3333)*SourceSize.zw; 
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
COMPAT_VARYING vec2 g10;
COMPAT_VARYING vec2 g01;
COMPAT_VARYING vec2 g12;
COMPAT_VARYING vec2 g21;

// compatibility #defines
#define Source Texture
#define vTexCoord TEX0.xy

#define SourceSize vec4(TextureSize, 1.0 / TextureSize) //either TextureSize or InputSize
#define OutSize vec4(OutputSize, 1.0 / OutputSize)

#ifdef PARAMETER_UNIFORM
uniform COMPAT_PRECISION float SHARPEN;
uniform COMPAT_PRECISION float CONTR;
uniform COMPAT_PRECISION float DETAILS;
#else
#define SHARPEN   1.2
#define CONTR  0.08
#define DETAILS   1.0
#endif 

void main()
{
	vec3 c10 = COMPAT_TEXTURE(Source, vTexCoord + g10).rgb;
	vec3 c01 = COMPAT_TEXTURE(Source, vTexCoord + g01).rgb;
	vec3 c21 = COMPAT_TEXTURE(Source, vTexCoord + g21).rgb;
	vec3 c12 = COMPAT_TEXTURE(Source, vTexCoord + g12).rgb;
	vec3 c11 = COMPAT_TEXTURE(Source, vTexCoord      ).rgb;	
	vec3 b11 = (c10+c01+c12+c21)*0.25; 
	
	float contrast = max(max(c11.r,c11.g),c11.b);
	contrast = mix(2.0*CONTR, CONTR, contrast);
	
	vec3 mn1 = min(min(c10,c01),min(c12,c21)); mn1 = min(mn1,c11*(1.0-contrast));
	vec3 mx1 = max(max(c10,c01),max(c12,c21)); mx1 = max(mx1,c11*(1.0+contrast));
	
	vec3 dif = pow(mx1-mn1+0.0001, vec3(0.75,0.75,0.75));
	vec3 sharpen = mix(vec3(SHARPEN*DETAILS), vec3(SHARPEN), dif);
	
	c11 = clamp(mix(c11,b11,-sharpen), mn1,mx1);
	
	FragColor = vec4(c11,1.0); 
} 
#endif
