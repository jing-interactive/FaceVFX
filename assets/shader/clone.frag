uniform sampler2D src, srcBlur, dstBlur;
uniform vec2 ciWindowSize;

in vec2     ciTexCoord0;
out highp vec4   Color;

void main() {
    vec4 srcColorBlur = texture2D(srcBlur, ciTexCoord0);
    if(srcColorBlur.a > 0.) {
        vec3 srcColor = texture2D(src, ciTexCoord0).rgb;
        vec3 dstColorBlur = texture2D(dstBlur, ciTexCoord0).rgb;
        vec3 offset = (dstColorBlur.rgb - srcColorBlur.rgb);
        Color = vec4(srcColor + offset, 1.);
    } else {
        Color = vec4(0.);
    }
}