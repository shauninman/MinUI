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
    vec2 offsetX = vec2(texelSize.x, 0.0);
    vec2 offsetY = vec2(0.0, texelSize.y);

    float sharpenStrength = 0.3; // Change this value to control sharpness

    vec4 center = texture2D(Texture, vTexCoord);
    vec4 up     = texture2D(Texture, vTexCoord - offsetY);
    vec4 down   = texture2D(Texture, vTexCoord + offsetY);
    vec4 left   = texture2D(Texture, vTexCoord - offsetX);
    vec4 right  = texture2D(Texture, vTexCoord + offsetX);

    vec4 sharpened = center * 5.0 - (up + down + left + right);
    vec4 finalColor = mix(center, sharpened, sharpenStrength);

    gl_FragColor = clamp(finalColor, 0.0, 1.0);
}
#endif