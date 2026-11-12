#pragma once

#include "FlowUi/Private/ErrorReporting.hpp"
#include "FlowUi/Private/platformProviderService/PlatformTypes.hpp"
#include "FlowUi/Public/errorManagement/ErrorCodeTypes.hpp"
#include "FlowUi/Public/errorManagement/ErrorEvents.hpp"
#include "FlowUi/Public/errorManagement/ErrorSites.hpp"
#include "FlowUi/Public/errorManagement/Result.hpp"
#ifndef GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_VULKAN
#endif
#include <GLFW/glfw3.h>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <new>
#include <string_view>
#include <thread>
#include <vector>
#include <vulkan/vulkan.h>

namespace FlowUi::detail::platform {
/** Shared GLFW initialization reference owned by one platform coordinator. */
class GlfwRuntime {
public:
	GlfwRuntime() noexcept = default;
	~GlfwRuntime() noexcept { shutdown(); }
	GlfwRuntime(const GlfwRuntime&) = delete;
	GlfwRuntime& operator=(const GlfwRuntime&) = delete;
	/** Acquire GLFW on its process-wide platform thread. */
	[[nodiscard]] Status initialize(const PlatformConfig&) noexcept {
		std::lock_guard lock(sharedMutex_);
		if (initialized_)
			return unexpectedError(
				makeError(ErrorCode::ObjectAlreadyInitialized, ErrorSite::WindowBackendInitialize));
		const auto callingThread = std::this_thread::get_id();
		if (referenceCount_ != 0 && callingThread != sharedThread_)
			return unexpectedError(
				makeError(ErrorCode::WrongThread, ErrorSite::WindowBackendInitialize));
		if (referenceCount_ == 0) {
			glfwSetErrorCallback([](int code, const char* description) noexcept {
				reportErrorEvent(ErrorEventView{
					.error = makeError(ErrorCode::None, ErrorSite::WindowBackendDiagnostic, 0u, 0u,
									   static_cast<std::uint32_t>(code)),
					.nativeMessage =
						description ? std::string_view{description} : std::string_view{},
					.kind = ErrorEventKind::BackendDiagnostic});
			});
			if (glfwInit() != GLFW_TRUE)
				return unexpectedError(makeError(ErrorCode::PlatformInitializationFailed,
												 ErrorSite::WindowBackendInitialize));
			sharedThread_ = callingThread;
		}
		++referenceCount_;
		platformThread_ = callingThread;
		initialized_ = true;
		return {};
	}
	/** Validate readiness and thread before any native mutation. */
	[[nodiscard]] Status validate() const noexcept {
		if (!initialized_)
			return unexpectedError(
				makeError(ErrorCode::ObjectNotInitialized, ErrorSite::WindowBackendInitialize));
		if (std::this_thread::get_id() != platformThread_)
			return unexpectedError(makeError(ErrorCode::WrongThread, ErrorSite::WindowOperation));
		return {};
	}
	/** Pump GLFW exactly once. */
	[[nodiscard]] Status pollEvents() noexcept {
		if (const auto status = validate(); !status)
			return status;
		glfwGetError(nullptr);
		glfwPollEvents();
		return nativeResult();
	}
	/** Release the shared runtime after all windows belonging to it are released. */
	void shutdown() noexcept {
		if (!initialized_)
			return;
		if (const auto status = validate(); !status) {
			reportErrorEvent(
				ErrorEventView{.error = status.error(), .kind = ErrorEventKind::Reported});
			return;
		}
		if (windowCount_ != 0) {
			reportErrorEvent(ErrorEventView{
				.error = makeError(ErrorCode::ObjectAlreadyInitialized, ErrorSite::WindowDestroy),
				.kind = ErrorEventKind::Reported});
			return;
		}
		std::lock_guard lock(sharedMutex_);
		initialized_ = false;
		if (--referenceCount_ == 0)
			glfwTerminate();
	}
	/** Observe runtime acquisition. */
	[[nodiscard]] bool ready() const noexcept { return initialized_; }
	/** Retrieve borrowed Vulkan extension names valid until runtime shutdown. */
	[[nodiscard]] Result<std::vector<const char*>> requiredInstanceExtensions() const {
		if (const auto status = validate(); !status)
			return unexpectedError(status.error());
		std::uint32_t count = 0;
		const char** extensions = glfwGetRequiredInstanceExtensions(&count);
		if (!extensions || count == 0)
			return unexpectedError(makeError(ErrorCode::PlatformRequiredExtensionMissing,
											 ErrorSite::WindowRequiredExtensions));
		try {
			return std::vector<const char*>(extensions, extensions + count);
		} catch (const std::bad_alloc&) {
			return unexpectedError(
				makeError(ErrorCode::AllocationFailed, ErrorSite::WindowRequiredExtensions));
		}
	}
	/** Translate a GLFW error without discarding its native identity. */
	[[nodiscard]] static Status nativeResult(ErrorSite site = ErrorSite::WindowOperation) noexcept {
		const int nativeError = glfwGetError(nullptr);
		if (nativeError != GLFW_NO_ERROR)
			return unexpectedError(makeError(ErrorCode::WindowOperationFailed, site, 0u, 0u,
											 static_cast<std::uint32_t>(nativeError)));
		return {};
	}

private:
	friend class GlfwWindow;
	inline static std::mutex sharedMutex_;
	inline static std::thread::id sharedThread_{};
	inline static std::size_t referenceCount_ = 0;
	std::thread::id platformThread_{};
	std::size_t windowCount_ = 0;
	bool initialized_ = false;
};
} // namespace FlowUi::detail::platform
