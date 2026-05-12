#pragma once

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <FlowUi/Flow.hpp>
#include "devMode/devIcons.hpp"
#include "devMode/devJson.hpp"

inline Clay_Vector2 devScrollOffsetForElementId(
	FlowUi::UiManager& uiManager,
	std::string_view elementId) {
	const Clay_ScrollContainerData data =
		Clay_GetScrollContainerData(uiManager.toClayEID(elementId));
	if (!data.found || data.scrollPosition == nullptr)
	{
		return Clay_Vector2{0.0f, 0.0f};
	}
	return *data.scrollPosition;
}
