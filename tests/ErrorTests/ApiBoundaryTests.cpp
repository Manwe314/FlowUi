#include "FlowUi/Private/ApiBoundary.hpp"
#include "FlowUi/Public/Resources.hpp"
#include "FlowUi/Public/managers/FontManager.hpp"
#include "FlowUi/Public/managers/ImageManager.hpp"
#include "FlowUi/Public/managers/ThemeManager.hpp"
#include "FlowUi/Public/input/InputTranslatorPack.hpp"

#include <new>
#include <stdexcept>
#include <utility>

static_assert(noexcept(FlowUi::path_from_utf8(std::declval<std::string_view>())));
static_assert(noexcept(FlowUi::path_to_utf8(std::declval<const std::filesystem::path&>())));
static_assert(noexcept(FlowUi::read_file_bytes(std::declval<const std::filesystem::path&>())));
static_assert(noexcept(std::declval<FlowUi::FontManager&>().getFamilyId(std::string_view{})));
static_assert(noexcept(std::declval<FlowUi::ImageManager&>().contains(std::string_view{})));

namespace {
struct ThrowingTranslator {
	FlowUi::Status initialize(const FlowUi::InputTranslatorContext&) { throw std::bad_alloc{}; }
	FlowUi::Status poll(const FlowUi::InputTranslatorContext&, FlowUi::InputSink&) {
		throw std::bad_alloc{};
	}
	void resetTarget(FlowUi::WindowId, FlowUi::InputSink&) noexcept {}
	void shutdown() noexcept {}
};
} // namespace

int main() {
	using namespace FlowUi;
	const auto original = makeError(ErrorCode::PlatformSurfaceCreationFailed,
		ErrorSite::WindowSurfaceCreate, 42, 77, 123);
	const auto preserved = detail::catchResult<Status>(ErrorSite::AppConstruct, [&]() -> Status {
		throw FlowUiException(original);
	});
	if (preserved || preserved.error().code != original.code ||
		preserved.error().site != original.site || preserved.error().subject != original.subject ||
		preserved.error().auxiliary != original.auxiliary ||
		preserved.error().nativeCode != original.nativeCode)
		return 1;
	const auto allocation = detail::catchResult<Result<int>>(ErrorSite::ResourceReadFile,
		[]() -> Result<int> { throw std::bad_alloc{}; });
	if (allocation || allocation.error().code != ErrorCode::AllocationFailed ||
		allocation.error().site != ErrorSite::ResourceReadFile)
		return 2;
	const auto foreign = detail::catchResult<Status>(ErrorSite::AppFrameDispatch,
		[]() -> Status { throw std::runtime_error("callback failure"); });
	if (foreign || foreign.error().code != ErrorCode::ForeignExceptionObserved)
		return 3;
	const auto nested = detail::catchResult<Status>(ErrorSite::AppConstruct, [&]() -> Status {
		detail::requireApiSuccess(unexpectedError(original));
		return {};
	});
	if (nested || nested.error().site != original.site)
		return 4;
	const auto path = path_from_utf8("fonts/Unicode-\xc3\xa9.bin");
	if (!path)
		return 5;
	const auto encoded = path_to_utf8(*path);
	if (!encoded || *encoded != "fonts/Unicode-\xc3\xa9.bin")
		return 6;
	FontManager fonts;
	const auto family = fonts.getFamilyId("missing");
	const auto resolved = fonts.resolveFont("missing");
	ImageManager images;
	const auto contained = images.contains("missing");
	ThemeManager themes;
	const auto theme = themes.getActiveTheme<int>();
	if (family || resolved || contained || theme ||
		family.error().code != ErrorCode::ObjectNotInitialized ||
		resolved.error().code != ErrorCode::ObjectNotInitialized ||
		contained.error().code != ErrorCode::ObjectNotInitialized ||
		theme.error().code != ErrorCode::ObjectNotInitialized)
		return 7;
	InputTranslatorPack<ThrowingTranslator> translators;
	const auto initialized = translators.initialize({});
	if (initialized || initialized.error().code != ErrorCode::AllocationFailed)
		return 8;
	InputSink sink;
	if (!sink.initialize(1) || sink.initialize(1))
		return 9;
	return 0;
}
