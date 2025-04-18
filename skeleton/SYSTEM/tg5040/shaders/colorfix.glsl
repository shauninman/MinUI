#if defined(VERTEX)
attribute vec2 VertexCoord;
attribute vec2 TexCoord;
varying vec2 vTexCoord;

void main() {
    vTexCoord = TexCoord;
    gl_Position = vec4(VertexCoord, 0.0, 1.0);
}
#endif

#if defined(FRAGMENT)
precision mediump float;
uniform sampler2D Texture;
varying vec2 vTexCoord;

void main() {
    vec4 pixel = texture2D(Texture, vec2(vTexCoord.x, 1.0 - vTexCoord.y));
    gl_FragColor = vec4(pixel.a, pixel.b, pixel.g, pixel.r);
}
#endif