#version 460

layout(push_constant) uniform UiPushConstants {
    float viewportW;
    float viewportH;
    uint instanceBaseIndex;
    uint _pad;
} pushData;

struct UiInstance {
    uint type;
    float x;
    float y;
    float w;
    float h;
    uint colorRGBA;
    float r0;
    float r1;
    float r2;
    float r3;
    float borderL;
    float borderT;
    float borderR;
    float borderB;
    uint solidMode;
    float uv0x;
    float uv0y;
    float uv1x;
    float uv1y;
    uint texIndex;
    uint atlasLayer;
    uint _pad0;
};

layout(set = 0, binding = 0, std430) readonly buffer UiInstanceBuffer {
    UiInstance instances[];
} instanceBuffer;

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inUv;

layout(location = 0) out vec2 vLocal;
layout(location = 1) out vec2 vHalf;
layout(location = 2) out vec4 vColor;
layout(location = 3) out vec4 vRadius;
layout(location = 4) out vec4 vBorder;
layout(location = 5) flat out uint vSolidMode;
layout(location = 6) out vec2 vUv;
layout(location = 7) flat out uint vTexIndex;
layout(location = 8) flat out uint vAtlasLayer;

vec4 unpackColor(uint packedRgba8) {
    float r = float((packedRgba8 >> 0u) & 0xFFu) / 255.0;
    float g = float((packedRgba8 >> 8u) & 0xFFu) / 255.0;
    float b = float((packedRgba8 >> 16u) & 0xFFu) / 255.0;
    float a = float((packedRgba8 >> 24u) & 0xFFu) / 255.0;
    return vec4(r, g, b, a);
}

void main() {
    uint instanceIndex = pushData.instanceBaseIndex + uint(gl_InstanceIndex);
    UiInstance instance = instanceBuffer.instances[instanceIndex];

    vec2 size = vec2(instance.w, instance.h);
    vec2 pixelPosition = vec2(instance.x, instance.y) + inPos * size;

    float invViewportW = (pushData.viewportW > 0.0) ? (1.0 / pushData.viewportW) : 0.0;
    float invViewportH = (pushData.viewportH > 0.0) ? (1.0 / pushData.viewportH) : 0.0;
    vec2 clipPosition = vec2(
        pixelPosition.x * invViewportW * 2.0 - 1.0,
        1.0 - pixelPosition.y * invViewportH * 2.0
    );
    gl_Position = vec4(clipPosition, 0.0, 1.0);

    vec2 uvMin = vec2(instance.uv0x, instance.uv0y);
    vec2 uvMax = vec2(instance.uv1x, instance.uv1y);

    vLocal = (inPos - vec2(0.5)) * size;
    vHalf = size * 0.5;
    vColor = unpackColor(instance.colorRGBA);
    vRadius = vec4(instance.r0, instance.r1, instance.r2, instance.r3);
    vBorder = vec4(instance.borderL, instance.borderT, instance.borderR, instance.borderB);
    vSolidMode = instance.solidMode;
    vUv = mix(uvMin, uvMax, inUv);
    vTexIndex = instance.texIndex;
    vAtlasLayer = instance.atlasLayer;
}
