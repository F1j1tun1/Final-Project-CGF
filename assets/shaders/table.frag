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
uniform vec3 uFogColor;
uniform bool uUseTexture;
uniform bool uLighting;

void main() {
    vec3 surface = uColor;
    if (uUseTexture) {
        surface *= texture(uTexture, vTexCoord * uUvScale).rgb;
    }
    if (!uLighting) {
        fragColor = vec4(surface, uOpacity);
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
    fragColor = vec4(result, uOpacity);
}
