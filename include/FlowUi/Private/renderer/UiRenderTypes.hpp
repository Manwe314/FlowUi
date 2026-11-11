#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

enum class UiType : uint8_t {
	Solid = 0,
	Msdf = 1,
	Textured = 2,
};

struct RectF {
	float x = 0.0f;
	float y = 0.0f;
	float w = 0.0f;
	float h = 0.0f;
};

struct UiInstance {
	uint32_t type = 0;
	float x = 0.0f;
	float y = 0.0f;
	float w = 0.0f;
	float h = 0.0f;

	uint32_t colorRGBA = 0;
	float r0 = 0.0f;
	float r1 = 0.0f;
	float r2 = 0.0f;
	float r3 = 0.0f;
	float borderL = 0.0f;
	float borderT = 0.0f;
	float borderR = 0.0f;
	float borderB = 0.0f;
	uint32_t solidMode = 0;

	float uv0x = 0.0f;
	float uv0y = 0.0f;
	float uv1x = 1.0f;
	float uv1y = 1.0f;
	uint32_t texIndex = 0;
	uint32_t atlasLayer = 0;
	uint32_t _pad0 = 0;
};

struct UiRun {
	UiType type = UiType::Solid;
	RectF scissor{};
	uint32_t firstInstance = 0;
	uint32_t instanceCount = 0;
};

static_assert(std::is_trivially_copyable_v<UiInstance>);
static_assert(sizeof(UiInstance) == 88);
static_assert(alignof(UiInstance) == 4);

namespace FlowUi::detail {

struct UiPushConstants {
	float viewport_width = 0.0f;
	float viewport_height = 0.0f;
	uint32_t instance_base_index = 0;
	// Zero disables primitive AA; positive values select linear coverage width.
	float primitive_coverage_width_scale = 1.0f;
};

static_assert(std::is_trivially_copyable_v<UiPushConstants>);
static_assert(sizeof(UiPushConstants) == 16);
static_assert(alignof(UiPushConstants) == 4);
static_assert(offsetof(UiPushConstants, viewport_width) == 0);
static_assert(offsetof(UiPushConstants, viewport_height) == 4);
static_assert(offsetof(UiPushConstants, instance_base_index) == 8);
static_assert(offsetof(UiPushConstants, primitive_coverage_width_scale) == 12);

} // namespace FlowUi::detail
