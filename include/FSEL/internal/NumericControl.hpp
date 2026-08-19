#pragma once

#include <optional>

#include "FSEL/Numeric.hpp"

namespace FlowUi::FSEL::detail::numeric_control {

template<typename Context>
void invoke(Context& context, ActionCall action) {
	if (!action) {
		return;
	}
	if constexpr (requires { context.invoke(action); }) {
		(void)context.invoke(action);
	} else {
		(void)context.uiManager.invoke(action);
	}
}

template<NumericValueType T, typename Context>
void beginEdit(Context& context) {
	auto& session = context.state().editSession;
	if (session.active || context.params.value == nullptr) {
		return;
	}
	detail::numeric::beginSession(session, *context.params.value);
	invoke(context, context.params.edit.onBegin);
}

template<NumericValueType T, typename Context>
bool applyValue(Context& context, T value) {
	if (context.params.value == nullptr || *context.params.value == value) {
		return false;
	}
	*context.params.value = value;
	context.state().editSession.valueChanged = true;
	invoke(context, context.params.edit.onChanged);
	return true;
}

template<typename Context>
void commitEdit(Context& context) {
	auto& session = context.state().editSession;
	if (!session.active) {
		return;
	}
	if (session.valueChanged) {
		invoke(context, context.params.edit.onCommit);
	}
	detail::numeric::resetSession(session);
}

template<NumericValueType T, typename Context>
void commitTextEdit(
	Context& context,
	std::optional<T> currentCandidate,
	NumericTextSyncPolicy syncPolicy) {
	auto& session = context.state().editSession;
	if (!session.active) {
		return;
	}
	if (syncPolicy == NumericTextSyncPolicy::OnCommit) {
		if (currentCandidate.has_value()) {
			(void)applyValue<T>(context, *currentCandidate);
		} else if (session.hasLastValidValue) {
			(void)applyValue<T>(context, session.lastValidValue);
		}
	}
	commitEdit(context);
}

template<NumericValueType T, typename Context>
void cancelEdit(Context& context) {
	auto& session = context.state().editSession;
	if (!session.active || context.params.value == nullptr) {
		return;
	}
	*context.params.value = session.beginValue;
	invoke(context, context.params.edit.onCancel);
	detail::numeric::resetSession(session);
}

} // namespace FlowUi::FSEL::detail::numeric_control
