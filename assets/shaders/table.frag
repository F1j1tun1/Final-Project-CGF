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
uniform float uLampBrightness;
uniform int uMaterial;
uniform vec3 uFogColor;
uniform bool uUseTexture;
uniform bool uLighting;

vec4 finishSurface(vec3 color) {
    return vec4(color, uOpacity);
}

void main() {
    vec3 surface = uColor;
    if (uUseTexture) {
        surface *= texture(uTexture, vTexCoord * uUvScale).rgb;
    }
    if (uRoomLight > 0.0 && uMaterial == 0 && uVelvet > 0.55) {
        // Broad, low-contrast changes in the nap of the old table felt.
        vec2 p = vWorldPosition.xz;
        float wear = sin(p.x * 1.7 + sin(p.y * 0.8)) * sin(p.y * 2.1 - p.x * 0.4);
        surface *= 0.94 + wear * 0.06;
        surface = mix(surface, vec3(0.10, 0.115, 0.07), max(wear, 0.0) * 0.10);
    }
    if (uMaterial == 1 || uMaterial == 8) {
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
        if (uMaterial == 8) {
            float paintedDiamond = abs((p.x + 0.39) / 0.27) + abs((p.y - 0.20) / 0.43);
            float paint = 1.0 - smoothstep(0.94, 1.01, paintedDiamond);
            surface = mix(surface, vec3(0.34, 0.018, 0.035), paint * 0.90);
            float cheekAccent = abs((p.x + 0.58) / 0.095) + abs((p.y + 0.23) / 0.15);
            surface = mix(surface, vec3(0.028, 0.025, 0.028),
                (1.0 - smoothstep(0.94, 1.03, cheekAccent)) * 0.85);
        }
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
    } else if (uMaterial == 5) {
        // Diagonal panes follow the curved, slack fabric of each hat point.
        vec2 cloth = vec2(vTexCoord.x * 5.0, vTexCoord.y * 3.5);
        float pane = mod(floor(cloth.x + cloth.y) + floor(cloth.x - cloth.y), 2.0);
        surface = mix(surface, vec3(0.014, 0.014, 0.018), pane * 0.84);
        surface *= 0.97 + 0.025 * sin(vTexCoord.x * 100.0) * sin(vTexCoord.y * 140.0);
        float tipBand = smoothstep(0.935, 0.947, vTexCoord.y)
            * (1.0 - smoothstep(0.965, 0.978, vTexCoord.y));
        surface = mix(surface, vec3(0.48, 0.32, 0.12), tipBand * 0.8);
    } else if (uMaterial == 6 || uMaterial == 10) {
        float stripe = smoothstep(0.45, 0.53, fract(vTexCoord.x * 12.0 + sin(vTexCoord.y * 5.0) * 0.035));
        vec3 panes = mix(uColor, vec3(0.78, 0.725, 0.62), stripe * 0.88);
        if (uMaterial == 10) {
            float blackSleeve = smoothstep(0.32, 0.37, vTexCoord.y);
            surface = mix(panes, vec3(0.027, 0.018, 0.047), blackSleeve);
            float braid = 1.0 - smoothstep(0.008, 0.020, abs(vTexCoord.y - 0.35));
            surface = mix(surface, vec3(0.52, 0.34, 0.12), braid * 0.90);
        } else surface = panes;
        surface *= 0.98 + 0.02 * sin(vTexCoord.x * 100.0) * sin(vTexCoord.y * 140.0);
    } else if (uMaterial == 7) {
        vec2 lace = vec2(vTexCoord.x * 36.0, vTexCoord.y * 5.0);
        vec2 hole = fract(lace + vec2(mod(floor(lace.y), 2.0) * 0.5, 0.0)) - 0.5;
        float eyelet = dot(hole / vec2(0.21, 0.23), hole / vec2(0.21, 0.23));
        if (vTexCoord.y > 0.14 && vTexCoord.y < 0.90 && eyelet < 0.72) discard;
        float embroidery = 1.0 - smoothstep(0.90, 1.35, eyelet);
        surface *= 0.92 + embroidery * 0.08;
        float edgeBraid = smoothstep(0.94, 0.98, vTexCoord.y);
        surface = mix(surface, vec3(0.55, 0.39, 0.19), edgeBraid * 0.45);
    } else if (uMaterial == 9) {
        float a = vTexCoord.x * 6.2831853;
        surface = mix(uColor, vec3(0.18, 0.055, 0.31), smoothstep(-0.035, 0.035, cos(a)));
        if (uUseTexture) surface *= texture(uTexture, vTexCoord * uUvScale).rgb;
        float height = clamp((vTexCoord.y - 0.45) / 0.43, 0.0, 1.0);
        float x = abs(cos(a));
        float lapelCenter = 0.09 + height * 0.55;
        float lapelWidth = 0.10 + height * 0.06;
        float lapel = (1.0 - smoothstep(lapelWidth - 0.015, lapelWidth + 0.015, abs(x - lapelCenter)))
            * smoothstep(0.43, 0.47, vTexCoord.y) * (1.0 - smoothstep(0.88, 0.92, vTexCoord.y))
            * smoothstep(0.0, 0.2, sin(a));
        surface = mix(surface, vec3(0.020, 0.024, 0.030), lapel);
        float belt = smoothstep(0.264, 0.276, vTexCoord.y)
            * (1.0 - smoothstep(0.382, 0.394, vTexCoord.y));
        surface = mix(surface, vec3(0.018, 0.028, 0.030), belt);
        surface *= 0.975 + 0.025 * sin(vTexCoord.x * 145.0) * sin(vTexCoord.y * 120.0);
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
        vec3 candle = vec3(i == 0 ? -5.45 : 5.45, 1.5, -2.6);
        vec3 delta = candle - vWorldPosition;
        float glow = 1.0 / (1.0 + dot(delta, delta) * 1.8);
        float candleDiffuse = max(dot(normal, normalize(delta)), 0.0);
        result += surface * vec3(1.0, 0.48, 0.16) * glow
            * (0.35 + candleDiffuse * 1.6) * uRoomLight;
    }
    for (int i = 0; i < 2; ++i) {
        vec3 lamp = vec3(i == 0 ? -4.70 : 4.70, 3.10, -5.75);
        vec3 delta = lamp - vWorldPosition;
        float glow = 1.0 / (1.0 + dot(delta, delta) * 2.1);
        float lampDiffuse = max(dot(normal, normalize(delta)), 0.0);
        result += surface * vec3(1.0, 0.64, 0.34) * glow
            * (0.28 + lampDiffuse) * uRoomLight * uLampBrightness * 0.70;
    }

    // Subtle distance haze keeps the back edge of the table in the scene.
    float fog = smoothstep(9.0, 20.0, length(uCameraPosition - vWorldPosition));
    result = mix(result, uFogColor, fog * 0.45);
    if (uMaterial == 3)
        result *= exp(-max(length(uCameraPosition - vWorldPosition) - 7.0, 0.0) * 0.14);
    fragColor = finishSurface(result);
}
