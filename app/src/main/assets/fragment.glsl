#version 320 es
precision highp float;

uniform vec3 uStartColor;
uniform vec3 uEndColor;
uniform float uProgress;
uniform vec2 uResolution;

in vec2 vUv;

out vec4 fragColor;

// these numbers are for the aspect-ratio based data
const float outerRadius = 0.8;
const float innerRadius = 0.6;
const vec2 center = vec2(0.0, -0.4);

const vec4 lineColor = vec4(1.0);

const float PI = 3.14159265359;

const int nSegments = 10;
const int nSubSegments = 5;
const float DRAW_OUTLINE = 1.0;


// we want the mask of the points whose distances are greater than the inner radius
// and smaller than the greater
// the first smoothstep will return 0 if its smaller than the radius or 1 if its greater
// the seconds smoothstep does the same thing but for the outer radius (both with some antialiasing)
// we want to paint when the first returns 1 and the second 0
float isInsideDonutMask(float d, float ir, float or, float aa) {
    return smoothstep(ir, ir + aa, d)
    - smoothstep(or, or + aa, d);
}

// from iq https://iquilezles.org/articles/distfunctions2d/
float udSegment(in vec2 p, in vec2 a, in vec2 b)
{
    vec2 ba = b - a;
    vec2 pa = p - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - h * ba);
}

float sdOrientedBox(in vec2 p, in vec2 a, in vec2 b, float th)
{
    float l = length(b - a);
    vec2 d = (b - a) / l;
    vec2 q = (p - (a + b) * 0.5);
    q = mat2(d.x, -d.y, d.y, d.x) * q;
    q = abs(q) - vec2(l, th) * 0.5;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0);
}


vec4 addSegment(vec4 currentColorMix, vec4 lc, vec2 a, vec2 b, vec2 uv, float lw, float aa) {

    float d = sdOrientedBox(uv, a, b, lw);
    float mask = 1.0 - smoothstep(lw, lw + aa, d);

    return mix(currentColorMix, lc, mask);
}

void main()
{
    // this would be based just on the uvs from the fragment shader without taking into consideration
    // the resolution or aspect ratio. this approach stretches the image
    //    vec2 uv = vUv - 0.5f;

    // screen coordinates adjusted to aspect ratio an centered in the center
    vec2 uv = (gl_FragCoord.xy - 0.5 * uResolution.xy) / uResolution.y;

    // withouth the resolution uniform we should use smaller numbers
    float blurDistance = 3.0 / uResolution.y;
    float lineWidth = 5.0 / uResolution.y;

    // ======= SEMICIRCLE / DONUT =========

    float d = distance(uv, center);

    float donutMask = isInsideDonutMask(d, innerRadius, outerRadius, blurDistance);
    float donutFillMask = isInsideDonutMask(d, innerRadius, outerRadius - lineWidth, blurDistance);


    // if the point.y is below the center we ignore it
    float semiCircleMask = smoothstep(center.y, center.y + blurDistance, uv.y);

    // with the tan^-1 we can get the angle between the center and the fragment
    // it will be between pi and 0 so we map it to 0,1
    float angle = atan(uv.y - center.y, uv.x - center.x);
    float fillProgress = 1.0 - (angle / PI);

    // we know compute the progress of the fill based on the angle and uProgress
    // we fill the semidonut if the angle is less or equal than the uProgress
    float fillMask = 1.0 - smoothstep(uProgress, uProgress + blurDistance, fillProgress);

    // we know mix the fill color based on fillProgress
    vec4 finalFillColor = vec4(mix(uStartColor, uEndColor, fillProgress), 1.0);

    float borderMask = (donutMask - donutFillMask) * semiCircleMask * DRAW_OUTLINE;

    float fillAreaMask = donutFillMask * semiCircleMask * fillMask;


    // ======= NEEDLE =========

    vec2 nA = center;

    // the angle will be pi if uProgress is 0 and 0 if uProgress is 1
    float nAngle = (1.0 - uProgress) * PI;
    float nLength = innerRadius * 0.7;
    vec2 nB = center + vec2(cos(nAngle), sin(nAngle)) * nLength;

    float nD = udSegment(uv, nA, nB);

    float nMask = 1.0 - smoothstep(lineWidth, lineWidth + blurDistance, nD);

    // gradient to black for the points that are closer to the center
    float gradientMask = smoothstep(innerRadius * 0.9, outerRadius, d);

    vec4 baseColor = (borderMask * lineColor) + (fillAreaMask * finalFillColor * gradientMask);

    vec4 finalColor = mix(baseColor, lineColor, nMask);


    // ======= SEGMENTS =========

    for (int i = 0; i <= nSegments * nSubSegments; i++) {

        float sAngle = PI / float(nSegments * nSubSegments) * float(i);

        float sLength = innerRadius * 1.15;

        vec2 sA = center + vec2(cos(sAngle), sin(sAngle)) * sLength;
        vec2 sB = center + vec2(cos(sAngle), sin(sAngle)) * (sLength + 0.35 * (outerRadius - innerRadius));

        float sW = lineWidth * 0.1; // segment width

        if (i % nSubSegments == 0) { // we are in a "big" segment marker
                                     sLength = innerRadius * 1.09;
                                     sA = center + vec2(cos(sAngle), sin(sAngle)) * sLength;
                                     sB = center + vec2(cos(sAngle), sin(sAngle)) * (sLength + 0.7 * (outerRadius - innerRadius));
                                     sW = lineWidth * 0.5;
        }


        finalColor = addSegment(finalColor, lineColor, sA, sB, uv, sW, blurDistance);

    }

    fragColor = finalColor;
}