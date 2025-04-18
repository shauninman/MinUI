/*
   Deblur Shader
   
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

#pragma parameter OFFSET "Deblur offset" 2.0 0.5 4.0 0.25 
#pragma parameter DEBLUR "Deblur str.  " 4.5 1.0 7.0 0.25 
#pragma parameter SMART  "Smart deblur " 0.5 0.0 1.0 0.05 

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
    TEX0.xy = TexCoord.xy * 1.00001;
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

// compatibility #defines
#define Source Texture
#define vTexCoord TEX0.xy

#define SourceSize vec4(TextureSize, 1.0 / TextureSize) //either TextureSize or InputSize
#define OutSize vec4(OutputSize, 1.0 / OutputSize)

#ifdef PARAMETER_UNIFORM
uniform COMPAT_PRECISION float OFFSET;
uniform COMPAT_PRECISION float DEBLUR;
uniform COMPAT_PRECISION float SMART;
#else
#define OFFSET  1.5
#define DEBLUR  4.0
#define SMART   0.5
#endif 

vec3  dt = vec3(1.,1.,1.);
vec3  dtt = vec3(0.0001, 0.0001, 0.0001); 

float wt(vec3 A, vec3 B)
{	
	return 4.0*length(A-B)/(dot(A+B,dt)+0.33);
} 

void main()
{
   vec2 tex = vTexCoord;	
   vec2 texsize = SourceSize.xy;
   float dx = OFFSET/texsize.x;
   float dy = OFFSET/texsize.y;
   
   vec4 yx = vec4( dx, dy,-dx,-dy);
   vec2 xx = vec2( dx, 0.0);
   vec2 yy = vec2( 0.0, dy);
   
   vec3 c11 = COMPAT_TEXTURE(Source, tex        ).xyz;  
   vec3 c00 = COMPAT_TEXTURE(Source, tex + yx.zw).xyz;
   vec3 c20 = COMPAT_TEXTURE(Source, tex + yx.xw).xyz;
   vec3 c22 = COMPAT_TEXTURE(Source, tex + yx.xy).xyz;
   vec3 c02 = COMPAT_TEXTURE(Source, tex + yx.zy).xyz;
   vec3 c10 = COMPAT_TEXTURE(Source, tex - yy   ).xyz;     
   vec3 c21 = COMPAT_TEXTURE(Source, tex + xx   ).xyz;
   vec3 c12 = COMPAT_TEXTURE(Source, tex + yy   ).xyz;
   vec3 c01 = COMPAT_TEXTURE(Source, tex - xx   ).xyz;
   
   vec3 mn1 = min (min (c00,c01),c02);
   vec3 mn2 = min (min (c10,c11),c12);
   vec3 mn3 = min (min (c20,c21),c22);
   vec3 mx1 = max (max (c00,c01),c02);
   vec3 mx2 = max (max (c10,c11),c12);
   vec3 mx3 = max (max (c20,c21),c22);

   vec3 d11;
   
   mn1 = min(min(mn1,mn2),mn3);
   mx1 = max(max(mx1,mx2),mx3);
   vec3 contrast = mx1 - mn1;
	
   vec3 dif1 = abs(c11-mn1) + dtt;
   vec3 dif2 = abs(c11-mx1) + dtt;
   
   float DB1 = DEBLUR; float dif;
   
   dif1=vec3(pow(dif1.x,DB1),pow(dif1.y,DB1),pow(dif1.z,DB1));
   dif2=vec3(pow(dif2.x,DB1),pow(dif2.y,DB1),pow(dif2.z,DB1));

   d11 = vec3((dif1.x*mx1.x + dif2.x*mn1.x)/(dif1.x + dif2.x),
                (dif1.y*mx1.y + dif2.y*mn1.y)/(dif1.y + dif2.y),
                (dif1.z*mx1.z + dif2.z*mn1.z)/(dif1.z + dif2.z));   

   float k10 = 1.0/(dot(abs(c10-d11),dt)+0.0001);
   float k01 = 1.0/(dot(abs(c01-d11),dt)+0.0001);
   float k11 = 1.0/(dot(abs(c11-d11),dt)+0.0001);  
   float k21 = 1.0/(dot(abs(c21-d11),dt)+0.0001);
   float k12 = 1.0/(dot(abs(c12-d11),dt)+0.0001);   
   float k00 = 1.0/(dot(abs(c00-d11),dt)+0.0001);
   float k02 = 1.0/(dot(abs(c02-d11),dt)+0.0001);  
   float k20 = 1.0/(dot(abs(c20-d11),dt)+0.0001);
   float k22 = 1.0/(dot(abs(c22-d11),dt)+0.0001);   
   
   float avg = (k10+k01+k11+k21+k12+k00+k02+k20+k22)/30.0;
   
   k10 = max(k10-avg, 0.0);
   k01 = max(k01-avg, 0.0);
   k11 = max(k11-avg, 0.0);   
   k21 = max(k21-avg, 0.0);
   k12 = max(k12-avg, 0.0);
   k00 = max(k00-avg, 0.0);
   k02 = max(k02-avg, 0.0);   
   k20 = max(k20-avg, 0.0);
   k22 = max(k22-avg, 0.0);
   
   d11 = (k10*c10 + k01*c01 + k11*c11 + k21*c21 + k12*c12 + k00*c00 + k02*c02 + k20*c20 + k22*c22 + 0.0001*c11)/(k10+k01+k11+k21+k12+k00+k02+k20+k22+0.0001);
   
   c11 = mix(c11, d11, clamp(1.75*contrast-0.125, 0.0, 1.0));
   c11 = mix(d11, c11, SMART);   
   
   FragColor = vec4(c11,1.0); 
} 
#endif
