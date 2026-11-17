#include "FlowUi/Public/Color.hpp"
#include "FlowUi/Private/ApiBoundary.hpp"

namespace FlowUi {

Result<Clay_Color> Flow_Color(std::string_view hexRgba) noexcept {
	try {
		return detail::decodeFlowColor(hexRgba);
	} catch (const std::invalid_argument&) {
		return unexpectedError(makeError(ErrorCode::ColorInvalid, ErrorSite::ColorParse));
	} catch (...) {
		return unexpectedError(detail::currentExceptionError(ErrorSite::ColorParse));
	}
}

void ValueOrThrow(Status status) {
	if (!status)
		throw FlowUiException(status.error());
}

} // namespace FlowUi
