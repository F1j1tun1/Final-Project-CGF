#version 330 core

out vec4 fragColor;
uniform vec2 uResolution;

void main() {
    vec2 uv = gl_FragCoord.xy / uResolution;
    vec3 horizon = vec3(0.29, 0.18, 0.22);
    vec3 twilight = vec3(0.075, 0.065, 0.135);
    vec3 night = vec3(0.025, 0.035, 0.075);
    vec3 sky = mix(horizon, twilight, smoothstep(0.60, 0.88, uv.y));
    sky = mix(sky, night, smoothstep(0.84, 1.0, uv.y));

    // A low, soft sunset and sparse cloud bands keep this an evening sky.
    vec2 sunDistance = (uv - vec2(0.83, 0.70)) * vec2(uResolution.x / uResolution.y, 1.0);
    float glow = exp(-dot(sunDistance, sunDistance) * 10.0);
    sky += vec3(0.08, 0.035, 0.012) * glow;
    float sun = 1.0 - smoothstep(0.024, 0.031, length(sunDistance));
    sky = mix(sky, vec3(0.54, 0.31, 0.20), sun * 0.45);
    float band = sin(uv.y * 76.0 + sin(uv.x * 5.0) * 1.8);
    float clouds = smoothstep(0.55, 0.95, band) * (1.0 - smoothstep(0.57, 0.92, uv.y));
    sky = mix(sky, vec3(0.13, 0.10, 0.18), clouds * 0.10);
    fragColor = vec4(sky, 1.0);
}
