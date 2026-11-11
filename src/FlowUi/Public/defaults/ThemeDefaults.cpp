#include "FlowUi/Public/FlowUiTheme.hpp"

namespace FlowUi {

FlowUiTheme FlowUiTheme::dark() noexcept {
	return FlowUiTheme{};
}

FlowUiTheme FlowUiTheme::light() noexcept {
	FlowUiTheme theme{};
	theme.background = {1.0f, 1.0f, 1.0f, 1.0f};
	theme.surface = {243.0f / 255.0f, 243.0f / 255.0f, 243.0f / 255.0f, 1.0f};
	theme.surfaceHeader = {229.0f / 255.0f, 229.0f / 255.0f, 229.0f / 255.0f, 1.0f};
	theme.surfaceHover = {232.0f / 255.0f, 232.0f / 255.0f, 232.0f / 255.0f, 1.0f};
	theme.textPrimary = {0.0f, 0.0f, 0.0f, 1.0f};
	theme.textSecondary = {51.0f / 255.0f, 51.0f / 255.0f, 51.0f / 255.0f, 1.0f};
	theme.border = {204.0f / 255.0f, 204.0f / 255.0f, 204.0f / 255.0f, 1.0f};
	return theme;
}

} // namespace FlowUi
