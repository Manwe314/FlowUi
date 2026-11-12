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
	/** Acquire translators in declaration order, rolling back earlier acquisitions on failure. */
	[[nodiscard]] Status initialize(const InputTranslatorContext& context) {
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
	}
	/** Poll each acquired translator exactly once in declaration order. */
	[[nodiscard]] Status poll(const InputTranslatorContext& context, InputSink& sink) {
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
	/** Borrow concrete translators for source-build configuration or deterministic sampling. */
	[[nodiscard]] std::tuple<Translators...>& translators() noexcept { return translators_; }
	/** Observe concrete translators. */
	[[nodiscard]] const std::tuple<Translators...>& translators() const noexcept {
		return translators_;
	}
	/** Fallback cleanup for standalone composition. */
	~InputTranslatorPack() { shutdown(); }
	InputTranslatorPack() = default;
	InputTranslatorPack(const InputTranslatorPack&) = delete;
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
