#version 460

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec2 vUv;
layout(location = 2) flat in uint vAtlasLayer;
layout(location = 3) in float vDistanceRangePx;

layout(set = 1, binding = 0) uniform sampler2DArray fontAtlasSampler;

layout(location = 0) out vec4 outColor;

float median(float a, float b, float c) {
	return max(min(a, b), min(max(a, b), c));
}

void main() {
	vec3 msdf = texture(fontAtlasSampler, vec3(vUv, float(vAtlasLayer))).rgb;
	float signedDistance = median(msdf.r, msdf.g, msdf.b);

	float pxRange = max(vDistanceRangePx, 1.0);
	vec2 atlasSize = vec2(textureSize(fontAtlasSampler, 0).xy);
	vec2 unitRange = vec2(pxRange) / atlasSize;
	vec2 screenTexSize = vec2(1.0) / max(fwidth(vUv), vec2(1e-6));
	float screenPxRange = max(0.5 * dot(unitRange, screenTexSize), 1.0);
	float alpha = clamp(screenPxRange * (signedDistance - 0.5) + 0.5, 0.0, 1.0);

	outColor = vec4(vColor.rgb, vColor.a * alpha);
	if (outColor.a <= 0.001) {
		discard;
	}
}
