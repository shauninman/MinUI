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
uniform vec2 texelSize;
varying vec2 vTexCoord;

void main() {
    vec2 offset = texelSize * 0.5;

    vec4 color       = texture2D(Texture, vTexCoord);
    vec4 colorRight  = texture2D(Texture, vTexCoord + vec2(offset.x, 0.0));
    vec4 colorDown   = texture2D(Texture, vTexCoord + vec2(0.0, offset.y));
    vec4 colorDiag   = texture2D(Texture, vTexCoord + offset);

    float weight = 0.5;
    vec4 smoothColor = mix(mix(color, colorRight, weight), mix(colorDown, colorDiag, weight), weight);

    gl_FragColor = smoothColor;
}
#endif