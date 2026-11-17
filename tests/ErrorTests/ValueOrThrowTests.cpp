#include "FlowUi/Public/Color.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace {

using FlowUi::Result;
using FlowUi::ValueOrThrow;

static_assert(std::is_same_v<decltype(ValueOrThrow(std::declval<Result<int>&>())), int&>);
static_assert(std::is_same_v<decltype(ValueOrThrow(std::declval<const Result<int>&>())), const int&>);
static_assert(std::is_same_v<decltype(ValueOrThrow(std::declval<Result<int>&&>())), int>);
static_assert(std::is_same_v<decltype(ValueOrThrow(std::declval<const Result<int>&&>())), int>);
static_assert(std::is_same_v<decltype(ValueOrThrow(std::declval<FlowUi::Status>())), void>);
static_assert(!noexcept(ValueOrThrow(std::declval<Result<int>>())));

[[nodiscard]] bool sameError(FlowUi::FlowUiError left, FlowUi::FlowUiError right) noexcept {
	return left.code == right.code && left.site == right.site && left.subject == right.subject &&
		left.auxiliary == right.auxiliary && left.nativeCode == right.nativeCode;
}

template <typename Operation>
[[nodiscard]] bool preservesFailure(Operation&& operation, FlowUi::FlowUiError expected) {
	try {
		std::forward<Operation>(operation)();
	} catch (const FlowUi::FlowUiException& exception) {
		return sameError(exception.error(), expected);
	}
	return false;
}

struct ThrowingMove {
	static inline bool rejectMove = false;
	ThrowingMove() = default;
	ThrowingMove(ThrowingMove&&) {
		if (rejectMove)
			throw std::runtime_error("payload move");
	}
};

} // namespace

int main() {
	Result<std::string> text{std::string{"original"}};
	if (std::addressof(ValueOrThrow(text)) != std::addressof(*text))
		return 1;
	ValueOrThrow(text) = "updated";
	if (*text != "updated" || std::addressof(ValueOrThrow(std::as_const(text))) != std::addressof(*text))
		return 2;
	const auto ownedText = ValueOrThrow(Result<std::string>{std::string{"temporary"}});
	if (ownedText != "temporary")
		return 3;
	const Result<std::string> constText{std::string{"copy"}};
	const auto copiedText = ValueOrThrow(std::move(constText));
	if (copiedText != "copy" || *constText != "copy")
		return 4;
	Result<std::unique_ptr<int>> pointer{std::make_unique<int>(42)};
	const auto ownedPointer = ValueOrThrow(std::move(pointer));
	if (!ownedPointer || *ownedPointer != 42 || *pointer)
		return 5;
	ValueOrThrow(FlowUi::Status{});
	const auto error = FlowUi::makeError(FlowUi::ErrorCode::AssetReadFailed,
		FlowUi::ErrorSite::ResourceReadFile, 42, 77, 123);
	Result<int> failed = FlowUi::unexpectedError(error);
	if (!preservesFailure([&] { (void)ValueOrThrow(failed); }, error) ||
		!preservesFailure([&] { (void)ValueOrThrow(std::as_const(failed)); }, error) ||
		!preservesFailure([&] { (void)ValueOrThrow(std::move(failed)); }, error))
		return 6;
	const Result<int> constFailed = FlowUi::unexpectedError(error);
	if (!preservesFailure([&] { (void)ValueOrThrow(std::move(constFailed)); }, error) ||
		!preservesFailure([&] { ValueOrThrow(FlowUi::Status{FlowUi::unexpectedError(error)}); }, error))
		return 7;
	Result<ThrowingMove> throwing;
	ThrowingMove::rejectMove = true;
	try {
		(void)ValueOrThrow(std::move(throwing));
		return 8;
	} catch (const std::runtime_error& exception) {
		if (std::string_view{exception.what()} != "payload move")
			return 9;
	}
}
