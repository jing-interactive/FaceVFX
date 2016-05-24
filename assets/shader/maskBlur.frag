uniform sampler2D tex, mask;
uniform vec2 direction;
uniform int strength;
uniform ivec2 ciWindowSize;

in vec2     TexCoord;
out highp vec4   Color;

void main() {
    vec2 dir = direction / ciWindowSize;
    vec4 sum = texture2D(tex, TexCoord);
    int i;
    for(i = 1; i < strength; i++) {
        vec2 curOffset = float(i) * dir;
        vec4 leftMask = texture2D(mask, TexCoord - curOffset);
        vec4 rightMask = texture2D(mask, TexCoord + curOffset);
        bool valid = leftMask.r == 1. && rightMask.r == 1.;
        if(valid) {
            sum +=
                texture2D(tex, TexCoord + curOffset) +
                texture2D(tex, TexCoord - curOffset);
        } else {
            break;
        }
    }
    int samples = 1 + (i - 1) * 2;
    Color = sum / float(samples);
}