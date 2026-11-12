#include "FlowUi/Public/input/InputEvents.hpp"
#include "FlowUi/Public/input/InputSink.hpp"
#include "FlowUi/Public/input/InputTranslation.hpp"
#include "FlowUi/Public/input/InputTranslatorConcept.hpp"
#include "FlowUi/Public/input/InputTranslatorContext.hpp"
#include "FlowUi/Public/input/InputTranslatorPack.hpp"
#include "FlowUi/Public/input/Key.hpp"

#ifdef GLFW_VERSION_MAJOR
#error "Public input authoring headers must not include GLFW"
#endif
#ifdef VK_VERSION_1_0
#error "Public input authoring headers must not include Vulkan"
#endif

static_assert(FlowUi::keyCode(FlowUi::Key::A) == 65);
static_assert(FlowUi::keyCode(FlowUi::Key::LeftControl) == 341);
static_assert(!FlowUi::keyFromCode(-1));
static_assert(!FlowUi::keyFromCode(511));
static_assert(!FlowUi::keySlot(static_cast<FlowUi::Key>(0xffff)));
static_assert(FlowUi::IsInputTranslatorPack<FlowUi::InputTranslatorPack<>>::value);

int main() {
	FlowUi::InputSink sink(1);
	if (!sink.key({.key = FlowUi::Key::A, .pressed = true}))
		return 1;
	const auto overflow = sink.key({.key = FlowUi::Key::A, .pressed = false});
	return !overflow && overflow.error().code == FlowUi::ErrorCode::PlatformInputQueueOverflow ? 0
																							   : 1;
}
