/*
Ported from https://github.com/glslify/glsl-hash-blur

The MIT License (MIT)

Copyright (c) 2015 Matt DesLauriers

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
OR OTHER DEALINGS IN THE SOFTWARE.

ported to RetroArch's glsl format by Aytos with some light changes by hunterk
*/

// Parameter lines go here:
#pragma parameter STRENGTH "Blur Strength" 10.0 0.0 60.0 0.5
#pragma parameter ITERATIONS "Iterations" 13.0 0.0 20.0 1.0
#pragma parameter ANIMATE "Animate Blurring (demo)" 0.0 0.0 1.0 1.0

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
// in variables go here as COMPAT_VARYING whatever

// compatibility #defines
#define Source Texture
#define vTexCoord TEX0.xy

#define SourceSize vec4(TextureSize, 1.0 / TextureSize) //either TextureSize or InputSize
#define OutSize vec4(OutputSize, 1.0 / OutputSize)

#ifdef PARAMETER_UNIFORM
uniform COMPAT_PRECISION float ITERATIONS;
uniform COMPAT_PRECISION float ANIMATE;
uniform COMPAT_PRECISION float STRENGTH;
#else
#define ITERATIONS 13.0
#define ANIMATE 0.0
#define STRENGTH 10.0
#endif

#ifndef TAU
  #define TAU 6.28318530718
#endif

vec3 sample(vec2 uv) {
  return COMPAT_TEXTURE(Source, uv).rgb;
}

COMPAT_PRECISION float random(vec2 co)
{
    COMPAT_PRECISION float a = 12.9898;
    COMPAT_PRECISION float b = 78.233;
    COMPAT_PRECISION float c = 43758.5453;
    COMPAT_PRECISION float dt= dot(co.xy ,vec2(a,b));
    COMPAT_PRECISION float sn= mod(dt,3.14);
    return fract(sin(sn) * c);
}

//Use last part of hash function to generate new random radius and angle
vec2 mult(inout vec2 r) {
  r = fract(r * vec2(12.9898,78.233));
  return sqrt(r.x + .001) * vec2(sin(r.y * TAU), cos(r.y * TAU));
}

vec3 blur(vec2 uv, float radius, float aspect, float offset) {
  vec2 circle = vec2(radius);
  circle.x *= aspect;
  vec2 rnd = vec2(random(vec2(uv + offset)));

  vec3 acc = vec3(0.0);
  for (int i = 0; i < int(ITERATIONS); i++) {
    acc += sample(uv + circle * mult(rnd)).xyz;
  }
  return acc / ITERATIONS;
}

vec3 blur(vec2 uv, float radius, float aspect) {
  return blur(uv, radius, aspect, 0.0);
}

vec3 blur(vec2 uv, float radius) {
  return blur(uv, radius, 1.0);
}

void main() {
  vec2 uv = vTexCoord.xy - 0.5;//vec2(vTexCoord.x, 1.0 - vTexCoord.y);
//  uv.y = 1.0 - uv.y;

  float texelSize = 1.0 / OutputSize.x;
  float aspect = OutputSize.x / OutputSize.y;

  //animate strength
  float anim = ANIMATE * sin(float(FrameCount)/60.0)/2.0+0.5;
  float strength = STRENGTH * anim * texelSize;

  //vignette blur
  float radius = 1.0 - length(uv - 0.5);
  radius = smoothstep(0.7, 0.0, radius) * strength;
  
  //jitter the noise but not every frame
  float tick = floor(fract(float(FrameCount)/60.0)*20.0);
  float jitter = mod(tick * 382.0231, 21.321);
  
  uv = uv + 0.5;

  //Apply the blur effect...
  //We do this on every fragment, but you 
  //might get a performance boost by only
  //blurring fragments where radius > 0
  vec3 color = blur(uv, radius, aspect, jitter);
  FragColor = vec4(color, 1.0);
}
#endif
