#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <clay.h>

namespace FlowUi::detail {

struct InputFieldRectOverride {
	int32_t insertBeforeCommandIndex = 0;
	Clay_BoundingBox boundingBox{};
	Clay_Color color = Clay_Color{0.0f, 0.0f, 0.0f, 0.0f};
};

struct InputFieldTextColorRangeOverride {
	size_t startByteOffset = 0u;
	size_t endByteOffset = 0u;
};

struct InputFieldTextColorOverride {
	int32_t commandIndex = 0;
	Clay_Color color = Clay_Color{0.0f, 0.0f, 0.0f, 0.0f};
	std::vector<InputFieldTextColorRangeOverride> ranges{};
};

struct InputFieldTextLayoutOverride {
	int32_t commandIndex = 0;
	uint8_t tabWidth = 4;
};

struct InputFieldFrameOverrides {
	std::vector<InputFieldRectOverride> rects{};
	std::vector<InputFieldTextColorOverride> textColorOverrides{};
	std::vector<InputFieldTextLayoutOverride> textLayoutOverrides{};
};

} // namespace FlowUi::detail
