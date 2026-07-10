#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace FlowUi::devMode {

struct ViewPortFrameDiagnostics {
	std::string key{};
	double recordCpuMs = 0.0;
	double callbackCpuMs = 0.0;
	uint32_t width = 0u;
	uint32_t height = 0u;
	bool resized = false;
	bool hadCallback = false;
};

struct FrameDiagnostics {
	uint64_t frameIndex = 0u;

	double deltaMs = 0.0;
	double beginFrameMs = 0.0;
	double endFrameMs = 0.0;
	double clayLayoutMs = 0.0;
	double resourcePrepMs = 0.0;

	double drawFrameMs = 0.0;
	double fenceWaitMs = 0.0;
	double acquireMs = 0.0;
	double viewportRecordMs = 0.0;
	double uiRecordMs = 0.0;
	double submitMs = 0.0;
	double presentMs = 0.0;

	int32_t clayCommandCount = 0;
	uint32_t uiInstanceCount = 0u;
	uint32_t uiRunCount = 0u;
	uint32_t textGlyphCount = 0u;
	uint32_t imageCommandCount = 0u;

	uint32_t referencedViewportCount = 0u;
	uint32_t resizedViewportCount = 0u;
	uint64_t viewportPixelArea = 0u;

	bool swappedSuboptimal = false;
	bool swapchainRecreated = false;
	bool instanceBufferGrew = false;
	bool textureDescriptorsRebuilt = false;

	std::unordered_map<std::string, ViewPortFrameDiagnostics> viewports{};
};

struct RollingDiagnostics {
	double fps = 0.0;
	double avgFrameMs = 0.0;
	double p95FrameMs = 0.0;
	double maxFrameMs = 0.0;

	double avgUiRecordMs = 0.0;
	double avgViewportRecordMs = 0.0;
	double avgFenceWaitMs = 0.0;
	double avgPresentMs = 0.0;

	uint32_t framesOver16ms = 0u;
	uint32_t framesOver33ms = 0u;

	FrameDiagnostics latest{};
};

class PerformanceDiagnostics {
public:
	using Clock = std::chrono::steady_clock;

	void beginFrame(uint64_t frameIndex, double deltaSeconds) {
		current_ = FrameDiagnostics{};
		current_.frameIndex = frameIndex;
		current_.deltaMs = deltaSeconds * 1000.0;
	}

	FrameDiagnostics& current() { return current_; }
	const FrameDiagnostics& current() const { return current_; }
	const RollingDiagnostics& rolling() const { return rolling_; }

	void endCompletedFrame() {
		pushSample(current_);
		rolling_.latest = current_;
	}

	static double elapsedMs(Clock::time_point start, Clock::time_point end = Clock::now()) {
		return std::chrono::duration<double, std::milli>(end - start).count();
	}

private:
	static constexpr uint32_t kSampleCount = 180u;

	void pushSample(const FrameDiagnostics& frame) {
		samples_[sampleCursor_] = frame;
		sampleCursor_ = (sampleCursor_ + 1u) % kSampleCount;
		sampleCount_ = std::min<uint32_t>(sampleCount_ + 1u, kSampleCount);

		double totalFrameMs = 0.0;
		double totalUiRecordMs = 0.0;
		double totalViewportMs = 0.0;
		double totalFenceWaitMs = 0.0;
		double totalPresentMs = 0.0;
		double maxFrameMs = 0.0;
		uint32_t over16 = 0u;
		uint32_t over33 = 0u;

		std::array<double, kSampleCount> sorted{};
		for (uint32_t i = 0u; i < sampleCount_; ++i) {
			const FrameDiagnostics& sample = samples_[i];
			const double frameMs = sample.deltaMs;
			sorted[i] = frameMs;
			totalFrameMs += frameMs;
			totalUiRecordMs += sample.uiRecordMs;
			totalViewportMs += sample.viewportRecordMs;
			totalFenceWaitMs += sample.fenceWaitMs;
			totalPresentMs += sample.presentMs;
			maxFrameMs = std::max(maxFrameMs, frameMs);
			over16 += frameMs > 16.7 ? 1u : 0u;
			over33 += frameMs > 33.3 ? 1u : 0u;
		}

		std::sort(sorted.begin(), sorted.begin() + sampleCount_);
		const uint32_t p95Index = sampleCount_ == 0u
			? 0u
			: static_cast<uint32_t>((sampleCount_ - 1u) * 0.95);

		rolling_.avgFrameMs = sampleCount_ == 0u ? 0.0 : totalFrameMs / sampleCount_;
		rolling_.fps = rolling_.avgFrameMs > 0.0 ? 1000.0 / rolling_.avgFrameMs : 0.0;
		rolling_.p95FrameMs = sampleCount_ == 0u ? 0.0 : sorted[p95Index];
		rolling_.maxFrameMs = maxFrameMs;
		rolling_.avgUiRecordMs = sampleCount_ == 0u ? 0.0 : totalUiRecordMs / sampleCount_;
		rolling_.avgViewportRecordMs = sampleCount_ == 0u ? 0.0 : totalViewportMs / sampleCount_;
		rolling_.avgFenceWaitMs = sampleCount_ == 0u ? 0.0 : totalFenceWaitMs / sampleCount_;
		rolling_.avgPresentMs = sampleCount_ == 0u ? 0.0 : totalPresentMs / sampleCount_;
		rolling_.framesOver16ms = over16;
		rolling_.framesOver33ms = over33;
	}

	FrameDiagnostics current_{};
	RollingDiagnostics rolling_{};
	std::array<FrameDiagnostics, kSampleCount> samples_{};
	uint32_t sampleCursor_ = 0u;
	uint32_t sampleCount_ = 0u;
};

} // namespace FlowUi::devMode

#endif
