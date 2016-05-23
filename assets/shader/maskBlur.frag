#extension GL_ARB_texture_rectangle : enable

uniform sampler2DRect tex, mask;
uniform vec2 direction;
uniform int strength;

in vec2     ciTexCoord0;
out highp vec4   Color;

void main() {
    vec2 pos = ciTexCoord0.st;
    vec4 sum = texture2DRect(tex, pos);
    int i;
    for(i = 1; i < strength; i++) {
        vec2 curOffset = float(i) * direction;
        vec4 leftMask = texture2DRect(mask, pos - curOffset);
        vec4 rightMask = texture2DRect(mask, pos + curOffset);
        bool valid = leftMask.r == 1. && rightMask.r == 1.;
        if(valid) {
            sum +=
                texture2DRect(tex, pos + curOffset) +
                texture2DRect(tex, pos - curOffset);
        } else {
            break;
        }
    }
    int samples = 1 + (i - 1) * 2;
    Color = sum / float(samples);
}