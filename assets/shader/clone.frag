#extension GL_ARB_texture_rectangle : enable

uniform sampler2DRect src, srcBlur, dstBlur;

in vec2     ciTexCoord0;
out highp vec4   Color;

void main() {
    vec2 pos = ciTexCoord0.st;   
    vec4 srcColorBlur = texture2DRect(srcBlur, pos);
    if(srcColorBlur.a > 0.) {
        vec3 srcColor = texture2DRect(src, pos).rgb;
        vec3 dstColorBlur = texture2DRect(dstBlur, pos).rgb;
        vec3 offset = (dstColorBlur.rgb - srcColorBlur.rgb);
        Color = vec4(srcColor + offset, 1.);
    } else {
        Color = vec4(0.);
    }
}