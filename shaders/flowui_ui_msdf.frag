#version 460

layout(location = 0) in vec2 vLocal;
layout(location = 1) in vec2 vHalf;
layout(location = 2) in vec4 vColor;
layout(location = 3) in vec4 vRadius;
layout(location = 4) in vec4 vBorder;
layout(location = 5) flat in uint vSolidMode;
layout(location = 6) in vec2 vUv;
layout(location = 7) flat in uint vTexIndex;
layout(location = 8) flat in uint vAtlasLayer;

layout(set = 1, binding = 0) uniform sampler2DArray fontAtlasSampler;
layout(set = 1, binding = 1) uniform sampler2D uiTextureSamplers[256];

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(0.95, 0.45, 0.10, 1.0);
}
