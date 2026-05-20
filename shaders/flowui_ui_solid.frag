#version 460

layout(location = 0) in vec2 vLocal;
layout(location = 1) in vec2 vHalf;
layout(location = 2) in vec4 vColor;
layout(location = 3) in vec4 vRadius;
layout(location = 4) in vec4 vBorder;
layout(location = 5) flat in uint vSolidMode;

layout(location = 0) out vec4 outColor;

vec4 normalizedRadii(vec2 halfExtent, vec4 radii) {
    float rmax = min(halfExtent.x, halfExtent.y);
    vec4 r = clamp(radii, 0.0, rmax);

	vec2 extent = halfExtent * 2.0;
    float kxTop = (r.x + r.y) > 0.0 ? min(1.0, extent.x / (r.x + r.y)) : 1.0;
    float kxBottom = (r.w + r.z) > 0.0 ? min(1.0, extent.x / (r.w + r.z)) : 1.0;
    float kyLeft = (r.x + r.w) > 0.0 ? min(1.0, extent.y / (r.x + r.w)) : 1.0;
    float kyRight = (r.y + r.z) > 0.0 ? min(1.0, extent.y / (r.y + r.z)) : 1.0;

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
    float outerDistance = roundedRectSdf(vLocal, vHalf, vRadius);
    float outerCoverage = aaCoverage(outerDistance);
    float finalCoverage = outerCoverage;

    if (vSolidMode != 0u) {
        vec4 clampedBorder = vec4(
            clamp(vBorder.x, 0.0, vHalf.x),
            clamp(vBorder.y, 0.0, vHalf.y),
            clamp(vBorder.z, 0.0, vHalf.x),
            clamp(vBorder.w, 0.0, vHalf.y)
        );

        float left = -vHalf.x + clampedBorder.x;
        float top = -vHalf.y + clampedBorder.y;
        float right = vHalf.x - clampedBorder.z;
        float bottom = vHalf.y - clampedBorder.w;

        if (left < right && top < bottom) {
            vec2 innerCenter = vec2(0.5 * (left + right), 0.5 * (top + bottom));
            vec2 innerHalf = vec2(0.5 * (right - left), 0.5 * (bottom - top));

            float innerInsetTL = max(clampedBorder.x, clampedBorder.y);
            float innerInsetTR = max(clampedBorder.z, clampedBorder.y);
            float innerInsetBR = max(clampedBorder.z, clampedBorder.w);
            float innerInsetBL = max(clampedBorder.x, clampedBorder.w);
            vec4 innerRadius = max(vRadius - vec4(innerInsetTL, innerInsetTR, innerInsetBR, innerInsetBL), 0.0);

            float innerDistance = roundedRectSdf(vLocal - innerCenter, innerHalf, innerRadius);
            float innerCoverage = aaCoverage(innerDistance);
            finalCoverage = max(outerCoverage - innerCoverage, 0.0);
        }
    }

    float alpha = vColor.a * finalCoverage;
    outColor = vec4(vColor.rgb, alpha);
}
