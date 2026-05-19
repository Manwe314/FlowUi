#pragma once

#include "devMode/devFlowElements/common.hpp"
#include "devMode/devFlowElements/devBasicInputField.hpp"

enum class devNumericInputValueKind : uint8_t {
	SignedInt = 0,
	UnsignedInt = 1,
	Floating = 2,
};

inline std::string devNumericTrimText(std::string_view text) {
	std::size_t begin = 0u;
	while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])) != 0)
	{
		++begin;
	}

	std::size_t end = text.size();
	while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1u])) != 0)
	{
		--end;
	}

	return std::string(text.substr(begin, end - begin));
}

inline bool devNumericTryParseInt64(std::string_view text, int64_t& outValue) {
	const std::string trimmed = devNumericTrimText(text);
	if (trimmed.empty())
	{
		return false;
	}

	errno = 0;
	char* end = nullptr;
	const long long parsed = std::strtoll(trimmed.c_str(), &end, 10);
	if (end == nullptr || *end != '\0' || errno == ERANGE)
	{
		return false;
	}

	if (
		parsed < static_cast<long long>(std::numeric_limits<int64_t>::min()) ||
		parsed > static_cast<long long>(std::numeric_limits<int64_t>::max()))
	{
		return false;
	}

	outValue = static_cast<int64_t>(parsed);
	return true;
}

inline bool devNumericTryParseDouble(std::string_view text, double& outValue) {
	const std::string trimmed = devNumericTrimText(text);
	if (trimmed.empty())
	{
		return false;
	}
	if (
		trimmed == "+" ||
		trimmed == "-" ||
		trimmed == "." ||
		trimmed == "+." ||
		trimmed == "-.")
	{
		return false;
	}
	if (!trimmed.empty())
	{
		const char last = trimmed.back();
		if (last == '.' || last == 'e' || last == 'E')
		{
			return false;
		}
		if ((last == '+' || last == '-') && trimmed.size() >= 2u)
		{
			const char prev = trimmed[trimmed.size() - 2u];
			if (prev == 'e' || prev == 'E')
			{
				return false;
			}
		}
	}

	errno = 0;
	char* end = nullptr;
	const double parsed = std::strtod(trimmed.c_str(), &end);
	if (end == nullptr || *end != '\0' || errno == ERANGE || !std::isfinite(parsed))
	{
		return false;
	}

	outValue = parsed;
	return true;
}

inline bool devNumericTryParseText(
	devNumericInputValueKind kind,
	std::string_view text,
	double& outValue) {
	const std::string trimmed = devNumericTrimText(text);
	if (trimmed.empty())
	{
		return false;
	}

	if (kind == devNumericInputValueKind::Floating)
	{
		double parsed = 0.0;
		if (!devNumericTryParseDouble(trimmed, parsed))
		{
			return false;
		}
		outValue = parsed;
		return true;
	}

	int64_t parsed = 0;
	if (!devNumericTryParseInt64(trimmed, parsed))
	{
		return false;
	}

	if (kind == devNumericInputValueKind::UnsignedInt && parsed < 0)
	{
		return false;
	}

	outValue = static_cast<double>(parsed);
	return true;
}

inline double devNumericRoundTo2Decimals(double value) {
	double rounded = std::round(value * 100.0) / 100.0;
	if (std::fabs(rounded) < 0.005)
	{
		rounded = 0.0;
	}
	return rounded;
}

inline bool devNumericNormalizeValue(
	devNumericInputValueKind kind,
	double rawValue,
	double minValue,
	double maxValue,
	double& outValue) {
	if (!std::isfinite(rawValue) || !std::isfinite(minValue) || !std::isfinite(maxValue))
	{
		return false;
	}

	double clampedMin = minValue;
	double clampedMax = maxValue;
	if (clampedMax < clampedMin)
	{
		clampedMax = clampedMin;
	}

	if (kind == devNumericInputValueKind::UnsignedInt)
	{
		if (clampedMin < 0.0)
		{
			clampedMin = 0.0;
		}
		if (clampedMax < clampedMin)
		{
			clampedMax = clampedMin;
		}
	}

	if (kind == devNumericInputValueKind::Floating)
	{
		outValue = devNumericRoundTo2Decimals(std::clamp(rawValue, clampedMin, clampedMax));
		return true;
	}

	const double integerLowerBound =
		static_cast<double>(std::numeric_limits<int64_t>::min());
	const double integerUpperBound =
		static_cast<double>(std::numeric_limits<int64_t>::max());
	clampedMin = std::max(clampedMin, integerLowerBound);
	clampedMax = std::min(clampedMax, integerUpperBound);
	if (clampedMax < clampedMin)
	{
		clampedMax = clampedMin;
	}

	const double clamped = std::clamp(rawValue, clampedMin, clampedMax);
	const int64_t rounded = static_cast<int64_t>(std::llround(clamped));
	if (kind == devNumericInputValueKind::UnsignedInt && rounded < 0)
	{
		outValue = 0.0;
		return true;
	}

	outValue = static_cast<double>(rounded);
	return true;
}

inline bool devNumericValuesEqual(
	devNumericInputValueKind kind,
	double lhs,
	double rhs) {
	if (kind == devNumericInputValueKind::Floating)
	{
		return std::fabs(lhs - rhs) <= 1.0e-12;
	}
	return static_cast<int64_t>(std::llround(lhs)) == static_cast<int64_t>(std::llround(rhs));
}

inline std::string devNumericValueToText(
	devNumericInputValueKind kind,
	double value) {
	if (kind == devNumericInputValueKind::Floating)
	{
		const double rounded = devNumericRoundTo2Decimals(value);
		std::ostringstream stream{};
		stream.setf(std::ios::fixed, std::ios::floatfield);
		stream.precision(2);
		stream << rounded;
		std::string text = stream.str();
		while (!text.empty() && text.back() == '0')
		{
			text.pop_back();
		}
		if (!text.empty() && text.back() == '.')
		{
			text.pop_back();
		}
		if (text.empty() || text == "-0")
		{
			return "0";
		}
		return text;
	}
	if (kind == devNumericInputValueKind::UnsignedInt)
	{
		const int64_t rounded = std::max<int64_t>(
			0,
			static_cast<int64_t>(std::llround(value)));
		return std::to_string(rounded);
	}
	return std::to_string(static_cast<int64_t>(std::llround(value)));
}

struct devNumericInputParams {
	std::string fieldId = "";
	std::string initialText = "";
	std::string hintText = "float";
	bool showHint = true;
	devNumericInputValueKind valueKind = devNumericInputValueKind::Floating;
	double minValue = -1000000.0;
	double maxValue = 1000000.0;
	double floatRatePerPixel = 0.1;
	double integerRatePerPixel = 1.0;
	std::function<void(double)> onValueChangedCallback = nullptr;

	Clay_Sizing sizing = Clay_Sizing{
		.width = CLAY_SIZING_FIT(0),
		.height = CLAY_SIZING_FIT(0),
	};
	Clay_Padding padding = CLAY_PADDING_ALL(0);
	uint16_t childGap = 6;
	Clay_Color backgroundColor = FlowUi::Flow_Color("#00000000");

	Clay_Sizing inputSizing = Clay_Sizing{
		.width = CLAY_SIZING_FIT(64.0f, 180.0f),
		.height = CLAY_SIZING_FIT(0),
	};
	Clay_Padding inputPadding = CLAY_PADDING_ALL(8);
	Clay_Color inputBackgroundColor = FlowUi::Flow_Color("#252932ff");
	Clay_Color inputBorderColor = FlowUi::Flow_Color("#8f8d8dff");
	Clay_BorderWidth inputBorderWidth = Clay_BorderWidth{1, 1, 1, 1, 0};
	Clay_CornerRadius inputCornerRadius = CLAY_CORNER_RADIUS(6);
	uint16_t fontId = 0;
	uint16_t fontSize = 12;
	Clay_Color hintTextColor = FlowUi::Flow_Color("#a8b4ccff");
	Clay_Color valueTextColor = FlowUi::Flow_Color("#ffffffff");
};

struct devNumericInputState {
	bool initialized = false;
	bool hasValue = false;
	double value = 0.0;
	std::string normalizedText{};
	bool pendingFieldReset = false;

	bool dragging = false;
	float dragPressMouseX = 0.0f;
	double dragPressValue = 0.0;
};

using DevNumericInputDef = FlowUi::ElementDefinition<
	devNumericInputParams,
	devNumericInputState,
	void,
	FLOW_DEF_ID("DevNumericInput"),
	true>;

inline const DevNumericInputDef kDevNumericInput = {
	nullptr,
	+[](DevNumericInputDef::InteractionContext& context) {
		devNumericInputState& state = DevNumericInputDef::getOrCreateState(FlowUi::toFlowId(context.elementID));
		const Clay_ElementId hintId = context.uiManager.toClayEID(context.createChildElementId("hint"));
		if (!context.previousInteraction.isPressed(hintId))
		{
			state.dragging = false;
			return;
		}
		state.dragging = true;
		state.dragPressMouseX = context.uiManager.getCurrentFrameInput().mouseX;
		state.dragPressValue = state.hasValue ? state.value : 0.0;
	},
	nullptr,
	+[](DevNumericInputDef::InteractionContext& context) {
		devNumericInputState& state = DevNumericInputDef::getOrCreateState(FlowUi::toFlowId(context.elementID));
		state.dragging = false;
	},
	+[](DevNumericInputDef::InteractionContext& context) {
		devNumericInputState& state = DevNumericInputDef::getOrCreateState(FlowUi::toFlowId(context.elementID));
		const FlowUi::FrameInput& input = context.uiManager.getCurrentFrameInput();
		if (!input.mouseDown[0])
		{
			state.dragging = false;
			return;
		}
		if (!state.dragging)
		{
			return;
		}

		const double rate =
			context.params.valueKind == devNumericInputValueKind::Floating
			? context.params.floatRatePerPixel
			: context.params.integerRatePerPixel;
		if (!std::isfinite(rate))
		{
			return;
		}

		const double deltaPixels = static_cast<double>(input.mouseX - state.dragPressMouseX);
		const double rawValue = state.dragPressValue + deltaPixels * rate;
		double normalized = 0.0;
		if (!devNumericNormalizeValue(
			context.params.valueKind,
			rawValue,
			context.params.minValue,
			context.params.maxValue,
			normalized))
		{
			return;
		}

		if (state.hasValue && devNumericValuesEqual(context.params.valueKind, state.value, normalized))
		{
			return;
		}

		state.hasValue = true;
		state.value = normalized;
		state.normalizedText = devNumericValueToText(context.params.valueKind, normalized);
		state.pendingFieldReset = true;
		if (context.params.onValueChangedCallback != nullptr)
		{
			context.params.onValueChangedCallback(normalized);
		}
	},
	nullptr,
	+[](DevNumericInputDef::BuildContext& context) {
		devNumericInputState& state = DevNumericInputDef::getOrCreateState(FlowUi::toFlowId(context.elementID));
		const uint64_t elementFlowId = FlowUi::toFlowId(context.elementID);
		const std::string fieldId =
			context.params.fieldId.empty()
			? context.createChildElementId("numeric-field")
			: context.params.fieldId;

		if (!state.initialized)
		{
			double parsed = 0.0;
			if (!devNumericTryParseText(context.params.valueKind, context.params.initialText, parsed))
			{
				parsed = 0.0;
			}

			double normalized = 0.0;
			if (!devNumericNormalizeValue(
				context.params.valueKind,
				parsed,
				context.params.minValue,
				context.params.maxValue,
				normalized))
			{
				normalized = 0.0;
			}
			state.initialized = true;
			state.hasValue = true;
			state.value = normalized;
			state.normalizedText = devNumericValueToText(context.params.valueKind, normalized);
		}

		if (state.pendingFieldReset)
		{
			(void)context.uiManager.inputFields().removeField(fieldId);
			state.pendingFieldReset = false;
		}

		Clay_ElementDeclaration root{};
		const Clay_ElementId rootId = context.uiManager.toClayEID(context.elementID);
		root.layout.sizing = context.params.sizing;
		root.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
		root.layout.padding = context.params.padding;
		root.layout.childGap = context.params.childGap;
		root.layout.childAlignment = {
			.x = CLAY_ALIGN_X_LEFT,
			.y = CLAY_ALIGN_Y_CENTER,
		};
		root.backgroundColor = context.params.backgroundColor;

		Clay_TextElementConfig hintTextConfig{};
		hintTextConfig.textColor = context.params.hintTextColor;
		hintTextConfig.fontId = context.params.fontId;
		hintTextConfig.fontSize = context.params.fontSize;
		hintTextConfig.wrapMode = CLAY_TEXT_WRAP_NONE;
		hintTextConfig.textAlignment = CLAY_TEXT_ALIGN_LEFT;

		const std::string hintWithColon =
			(context.params.hintText.empty() ? std::string("float") : context.params.hintText) + ":";

		CLAY(rootId, root){
			if (context.params.showHint)
			{
				CLAY(context.uiManager.toClayEID(context.createChildElementId("hint")), {}){
					CLAY_TEXT(
						context.uiManager.toClayString(hintWithColon),
						CLAY_TEXT_CONFIG(hintTextConfig));
				};
			}

			context.uiManager
				.createElement(kDevBasicInputField, context.createChildElementId("input"))
				.setParameters(devBasicInputFieldParams{
					.fieldId = fieldId,
					.initialText = state.normalizedText,
					.onTextChangedCallback = [
						elementFlowId,
						valueKind = context.params.valueKind,
						minValue = context.params.minValue,
						maxValue = context.params.maxValue,
						onValueChanged = context.params.onValueChangedCallback
					](std::string_view text) {
						devNumericInputState* latestState = DevNumericInputDef::tryGetState(elementFlowId);
						if (latestState == nullptr)
						{
							return;
						}

						double parsed = 0.0;
						if (!devNumericTryParseText(valueKind, text, parsed))
						{
							return;
						}

						double normalized = 0.0;
						if (!devNumericNormalizeValue(valueKind, parsed, minValue, maxValue, normalized))
						{
							return;
						}
						const bool normalizedDiffersFromParsed =
							!devNumericValuesEqual(valueKind, parsed, normalized);

						const bool changed =
							!latestState->hasValue ||
							!devNumericValuesEqual(valueKind, latestState->value, normalized);
						latestState->hasValue = true;
						latestState->value = normalized;
						latestState->normalizedText = devNumericValueToText(valueKind, normalized);
						if (normalizedDiffersFromParsed)
						{
							latestState->pendingFieldReset = true;
						}

						if (changed && onValueChanged != nullptr)
						{
							onValueChanged(normalized);
						}
						},
						.padding = context.params.inputPadding,
						.sizing = context.params.inputSizing,
						.borderColor = context.params.inputBorderColor,
						.borderWidth = context.params.inputBorderWidth,
						.backgroundColor = context.params.inputBackgroundColor,
						.cornerRadius = context.params.inputCornerRadius,
						.clipHorizontal = false,
						.clipVertical = false,
						.childTextAlignment = {
							.x = CLAY_ALIGN_X_LEFT,
							.y = CLAY_ALIGN_Y_CENTER,
						},
					.textWrapMode = CLAY_TEXT_WRAP_NONE,
					.textAlignment = CLAY_TEXT_ALIGN_LEFT,
					.fontId = context.params.fontId,
					.fontSize = context.params.fontSize,
					.textColor = context.params.valueTextColor,
				})
				.draw();
		};
	},
};
