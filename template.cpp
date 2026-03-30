#include "template.hpp"

#include <FlowUi/Flow.hpp>

#include <string_view>

void drawTemplateElement(FlowUi::UiManager& ui)
{
	ui.createElement(kTemplate, "template/main")
		.setParameters(templateParams{
			.importantInt = 64,
		})
		.draw();
}

FlowUi::FlowElementId getTemplateElementFlowId(std::string_view elementID)
{
	return FlowUi::toFlowId(elementID);
}

bool isTemplateElementEnabled(std::string_view elementID)
{
	const templateState* state = TemplateDefiition::tryGetStateConst(FlowUi::toFlowId(elementID));
	if (!state) {
		return false;
	}
	return state->enabled;
}

void setTemplateElementEnabled(std::string_view elementID, bool enabled)
{
	templateState& state = TemplateDefiition::getOrCreateState(FlowUi::toFlowId(elementID));
	state.enabled = enabled;
}

bool eraseTemplateElementState(std::string_view elementID)
{
	return TemplateDefiition::eraseState(FlowUi::toFlowId(elementID));
}

void initTemplateResources(FlowUi::UiManager& ui)
{
	(void)TemplateDefiition::getResources(ui);
}
