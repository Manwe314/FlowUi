#version 460
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec2 vLocal;
layout(location = 1) in vec2 vHalf;
layout(location = 2) in vec4 vColor;
layout(location = 3) in vec4 vRadius;
layout(location = 4) flat in uint vSolidMode;
layout(location = 5) in vec2 vUv;
layout(location = 6) flat in uint vTexIndex;

layout(set = 1, binding = 1) uniform sampler2D uiTextureSamplers[256];

layout(location = 0) out vec4 outColor;

vec4 normalizedRadii(vec2 halfExtent, vec4 radii) {
	float rmax = min(halfExtent.x, halfExtent.y);
	vec4 r = clamp(radii, 0.0, rmax);

	float kxTop = (r.x + r.y) > 0.0 ? min(1.0, halfExtent.x / (r.x + r.y)) : 1.0;
	float kxBottom = (r.w + r.z) > 0.0 ? min(1.0, halfExtent.x / (r.w + r.z)) : 1.0;
	float kyLeft = (r.x + r.w) > 0.0 ? min(1.0, halfExtent.y / (r.x + r.w)) : 1.0;
	float kyRight = (r.y + r.z) > 0.0 ? min(1.0, halfExtent.y / (r.y + r.z)) : 1.0;

	float k = min(min(kxTop, kxBottom), min(kyLeft, kyRight));
	return r * k;
}

float cornerRadiusForPoint(vec2 localPoint, vec4 radii) {
	if (localPoint.x < 0.0) {
		return (localPoint.y < 0.0) ? radii.x : radii.w;
	}
	return (localPoint.y < 0.0) ? radii.y : radii.z;
}

float roundedRectSdf(vec2 localPoint, vec2 halfExtent, vec4 radii) {
	vec4 normalized = normalizedRadii(halfExtent, radii);
	float cornerRadius = cornerRadiusForPoint(localPoint, normalized);
	vec2 boxHalf = halfExtent - vec2(cornerRadius);
	vec2 q = abs(localPoint) - boxHalf;
	return length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - cornerRadius;
}

float aaCoverage(float signedDistance) {
	float aa = max(fwidth(signedDistance), 1e-4);
	return clamp(0.5 - signedDistance / aa, 0.0, 1.0);
}

void main() {
	uint texIndex = min(vTexIndex, 255u);
	vec4 sampled = texture(uiTextureSamplers[nonuniformEXT(texIndex)], vUv);

	bool tintEnabled = (vSolidMode & 0x1u) != 0u;
	if (tintEnabled) {
		sampled *= vColor;
	}

	float outerDistance = roundedRectSdf(vLocal, vHalf, vRadius);
	float coverage = aaCoverage(outerDistance);
	float alpha = sampled.a * coverage;

	outColor = vec4(sampled.rgb, alpha);
	if (outColor.a <= 0.001) {
		discard;
	}
}
