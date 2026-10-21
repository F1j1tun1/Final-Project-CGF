#version 330 core

in vec3 vWorldPosition;
in vec3 vNormal;
in vec2 vTexCoord;
out vec4 fragColor;

uniform sampler2D uTexture;
uniform vec3 uColor;
uniform vec2 uUvScale;
uniform vec3 uCameraPosition;
uniform vec3 uLightPosition;
uniform float uLightIntensity;
uniform float uShininess;
uniform float uSpecular;
uniform float uEmission;
uniform float uOpacity;
uniform float uVelvet;
uniform float uRoomLight;
uniform vec4 uViewport;
uniform int uMaterial;
uniform vec3 uFogColor;
uniform bool uUseTexture;
uniform bool uLighting;

vec4 finishSurface(vec3 color) {
    if (uRoomLight > 0.0) {
        vec2 p = (gl_FragCoord.xy - uViewport.xy) / uViewport.zw;
        float edge = smoothstep(0.0, 0.075, p.x) * (1.0 - smoothstep(0.925, 1.0, p.x))
            * smoothstep(0.0, 0.09, p.y) * (1.0 - smoothstep(0.955, 1.0, p.y));
        color = mix(vec3(0.026, 0.014, 0.050), color, edge);
    }
    return vec4(color, uOpacity);
}

void main() {
    vec3 surface = uColor;
    if (uUseTexture) {
        surface *= texture(uTexture, vTexCoord * uUvScale).rgb;
    }
    if (uMaterial == 1) {
        vec2 p = vTexCoord * 2.0 - 1.0;
        float eyeY = 0.20 + 0.10 * (abs(p.x) - 0.39);
        vec2 eye = vec2((abs(p.x) - 0.39) / 0.225, (p.y - eyeY) / 0.085);
        float eyeDistance = dot(eye, eye);
        if (eyeDistance < 0.84) discard;
        float mouthY = -0.43 + 0.68 * p.x * p.x;
        float mouth = abs(p.y - mouthY);
        float goldEye = 1.0 - smoothstep(1.01, 1.18, eyeDistance);
        float edge = smoothstep(0.955, 0.985, abs(p.x));
        float cheekDiamond = abs(abs(p.x) - 0.61) + abs(p.y + 0.15) * 0.65;
        float goldCheek = 1.0 - smoothstep(0.055, 0.075, cheekDiamond);
        surface = mix(surface, vec3(0.63, 0.43, 0.18), clamp(goldEye + edge + goldCheek * 0.60, 0.0, 1.0));
        float smile = (1.0 - smoothstep(0.008, 0.024, mouth))
            * (1.0 - smoothstep(0.24, 0.29, abs(p.x)));
        surface = mix(surface, vec3(0.28, 0.075, 0.09), smile * 0.82);
    } else if (uMaterial == 2 || uMaterial == 4) {
        if (uMaterial == 4)
            surface = mix(vec3(0.26, 0.024, 0.06), vec3(0.115, 0.030, 0.20),
                          smoothstep(0.22, 0.28, vTexCoord.x));
        float diamond = abs(fract(vTexCoord.x * 12.0) - 0.5)
            + abs(fract(vTexCoord.y * 9.0) - 0.5);
        float stitch = 1.0 - smoothstep(0.008, 0.026, abs(diamond - 0.50));
        surface *= 0.97 + 0.025 * sin(vTexCoord.x * 110.0) * sin(vTexCoord.y * 150.0);
        surface = mix(surface, vec3(0.40, 0.25, 0.095), stitch * 0.055);
    } else if (uMaterial == 3) {
        vec2 tile = vWorldPosition.xz / 1.3;
        float checker = mod(floor(tile.x) + floor(tile.y), 2.0);
        surface = mix(vec3(0.045, 0.033, 0.043), vec3(0.14, 0.112, 0.13), checker);
        float grout = min(fract(tile.x), fract(tile.y));
        surface *= mix(0.45, 1.0, smoothstep(0.0, 0.018, grout));
    }
    if (!uLighting) {
        fragColor = finishSurface(surface);
        return;
    }

    vec3 normal = normalize(vNormal);
    vec3 toLight = uLightPosition - vWorldPosition;
    float distanceToLight = length(toLight);
    vec3 lightDirection = normalize(toLight);
    vec3 viewDirection = normalize(uCameraPosition - vWorldPosition);
    vec3 halfway = normalize(lightDirection + viewDirection);
    float diffuse = max(dot(normal, lightDirection), 0.0);
    float specular = diffuse > 0.0
        ? pow(max(dot(normal, halfway), 0.0), uShininess) * uSpecular
        : 0.0;
    float attenuation = 1.0 / (1.0 + 0.012 * distanceToLight * distanceToLight);
    vec3 warmLight = vec3(1.0, 0.94, 0.82);
    vec3 result = surface * (0.34 + uEmission)
        + (surface * diffuse + vec3(specular))
          * warmLight * uLightIntensity * attenuation;
    float fibers = pow(1.0 - abs(dot(normal, viewDirection)), 3.0);
    result += surface * fibers * uVelvet * 0.32;
    float rim = pow(1.0 - max(dot(normal, viewDirection), 0.0), 4.0);
    result += vec3(0.34, 0.22, 0.11) * rim * uVelvet * uRoomLight * 0.16;
    for (int i = 0; i < 2; ++i) {
        vec3 candle = vec3(i == 0 ? -3.7 : 3.7, 1.5, -2.6);
        vec3 delta = candle - vWorldPosition;
        float glow = 1.0 / (1.0 + dot(delta, delta) * 1.8);
        float candleDiffuse = max(dot(normal, normalize(delta)), 0.0);
        result += surface * vec3(1.0, 0.48, 0.16) * glow
            * (0.35 + candleDiffuse * 1.6) * uRoomLight;
    }

    // Subtle distance haze keeps the back edge of the table in the scene.
    float fog = smoothstep(9.0, 20.0, length(uCameraPosition - vWorldPosition));
    result = mix(result, uFogColor, fog * 0.45);
    if (uMaterial == 3)
        result *= exp(-max(length(uCameraPosition - vWorldPosition) - 7.0, 0.0) * 0.14);
    fragColor = finishSurface(result);
}
