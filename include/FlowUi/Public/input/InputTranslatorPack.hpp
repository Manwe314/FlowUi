#pragma once

#include "FlowUi/Public/input/InputTranslatorConcept.hpp"
#include <cstddef>
#include <tuple>
#include <utility>

namespace FlowUi {

/** Statically composed additional devices; native keyboard and mouse remain enabled. */
template <InputTranslatorConcept... Translators>
class InputTranslatorPack final {
public:
	/** Acquire translators in declaration order, rolling back earlier successful acquisitions in
	 * reverse order on failure. The failing translator must clean up its own partial acquisition.
	 * Reinitialization returns ObjectAlreadyInitialized; thrown exceptions become a failed Status.
	 * @par Example
	 * @code{.cpp}
	 * FlowUi::InputTranslatorPack<> pack;
	 * const auto status = pack.initialize(context);
	 * if (!status)
	 *     throw FlowUi::FlowUiException(status.error());
	 * @endcode
	 */
	[[nodiscard]] Status initialize(const InputTranslatorContext& context) noexcept try {
		if (initialized_ != 0 || ready_)
			return unexpectedError(
				makeError(ErrorCode::ObjectAlreadyInitialized, ErrorSite::InputManagerInitialize));
		Status status{};
		std::apply(
			[&](auto&... translator) {
				(
					[&] {
						if (status) {
							status = translator.initialize(context);
							if (status)
								++initialized_;
						}
					}(),
					...);
			},
			translators_);
		if (!status) {
			shutdown();
			return status;
		}
		ready_ = true;
		return {};
	} catch (...) {
		shutdown();
		return unexpectedError(detail::currentExceptionError(ErrorSite::InputManagerInitialize));
	}
	/** Poll acquired translators in declaration order, stopping at the first failure.
	 * Earlier emissions remain in sink. Direct consumers must reject/cancel that tick,
	 * reset affected targets and resynchronize external device state before reuse.
	 * App-owned polling already cancels routed state and clears translated queues.
	 * resetTarget/shutdown release external captures; they do not undo SDK side effects.
	 * @par Example
	 * @code{.cpp}
	 * const auto status = pack.poll(context, sink);
	 * if (!status) {
	 *     pack.resetTarget(window, sink);
	 *     throw FlowUi::FlowUiException(status.error());
	 * }
	 * @endcode
	 */
	[[nodiscard]] Status poll(const InputTranslatorContext& context, InputSink& sink) noexcept try {
		if (!ready_)
			return unexpectedError(
				makeError(ErrorCode::ObjectNotInitialized, ErrorSite::InputBeginFrame));
		Status status{};
		std::apply(
			[&](auto&... translator) {
				(
					[&] {
						if (status)
							status = translator.poll(context, sink);
					}(),
					...);
			},
			translators_);
		return status;
	} catch (...) {
		return unexpectedError(detail::currentExceptionError(ErrorSite::InputBeginFrame));
	}
	/** Cancel translator-side capture before a target disappears. */
	void resetTarget(WindowId window, InputSink& sink) noexcept {
		if (ready_)
			std::apply([&](auto&... translator) { (translator.resetTarget(window, sink), ...); },
					   translators_);
	}
	/** Release acquired resources in reverse declaration order; safe repeatedly. */
	void shutdown() noexcept {
		shutdownReverse(std::make_index_sequence<sizeof...(Translators)>{});
		initialized_ = 0;
		ready_ = false;
	}
	/** Borrow concrete translators for pre-initialization configuration or deterministic sampling.
	 * Do not replace or independently shut down elements behind pack lifecycle bookkeeping. */
	[[nodiscard]] std::tuple<Translators...>& translators() noexcept { return translators_; }
	/** Observe concrete translators. */
	[[nodiscard]] const std::tuple<Translators...>& translators() const noexcept {
		return translators_;
	}
	/** Fallback cleanup for standalone composition. */
	~InputTranslatorPack() { shutdown(); }
	/** Default-construct concrete translators without acquiring device resources. */
	InputTranslatorPack() noexcept = default;
	/** Device acquisition ownership cannot be copied. */
	InputTranslatorPack(const InputTranslatorPack&) = delete;
	/** Device acquisition ownership cannot be copy-assigned. */
	InputTranslatorPack& operator=(const InputTranslatorPack&) = delete;

private:
	template <std::size_t... Indices>
	void shutdownReverse(std::index_sequence<Indices...>) noexcept {
		(
			[&] {
				constexpr std::size_t index = sizeof...(Translators) - Indices - 1;
				if (index < initialized_)
					std::get<index>(translators_).shutdown();
			}(),
			...);
	}
	std::tuple<Translators...> translators_{};
	std::size_t initialized_ = 0;
	bool ready_ = false;
};

/** Restrict selection to the concrete pack composition API. */
template <class Type>
struct IsInputTranslatorPack : std::false_type {};
template <InputTranslatorConcept... Translators>
struct IsInputTranslatorPack<InputTranslatorPack<Translators...>> : std::true_type {};

} // namespace FlowUi
