#include "FlowUi/Public/Color.hpp"

namespace FlowUi {

Clay_Color Flow_Color(std::string_view hexRgba) {
	return detail::decodeFlowColor(hexRgba);
}

} // namespace FlowUi
