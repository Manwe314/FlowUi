#pragma once

#include <cstdint>

namespace FlowUi {

/**
 * How a deliberately defined FlowUi failure travels to its resolution boundary.
 * This is independent from ErrorImpact: transport distance is not severity.
 */
enum class ErrorCategory : std::uint8_t {
	Static = 0, /**< Invalid usage rejected during compilation. */
	Local, /**< Failure returned to a nearby caller. */
	Unwind, /**< Failure carried to an enclosing lifecycle boundary. */
	Fatal, /**< No supported continuation; the process must stop. */
};

/** Stable ownership domain for an error code. */
enum class ErrorDomain : std::uint8_t {
	None = 0, /**< No owning subsystem. */
	Core, /**< Core subsystem. */
	App, /**< App subsystem. */
	Platform, /**< Platform subsystem. */
	Window, /**< Window subsystem. */
	Frame, /**< Frame subsystem. */
	Vulkan, /**< Vulkan subsystem. */
	Renderer, /**< Renderer subsystem. */
	Storage, /**< Storage subsystem. */
	Resource, /**< Resource subsystem. */
	Asset, /**< Asset subsystem. */
	Font, /**< Font subsystem. */
	Image, /**< Image subsystem. */
	Icon, /**< Icon subsystem. */
	Element, /**< Element subsystem. */
	Action, /**< Action subsystem. */
	Theme, /**< Theme subsystem. */
	Input, /**< Input subsystem. */
	Popup, /**< Popup subsystem. */
	Shortcut, /**< Shortcut subsystem. */
	Viewport, /**< Viewport subsystem. */
	Agentic, /**< Agentic subsystem. */
};

/** The boundary that is intended to decide the outcome of an error. */
enum class ErrorBoundary : std::uint8_t {
	CompileExpression = 0, /**< Compile-time expression validation. */
	DirectCaller, /**< Immediate caller of the operation. */
	ManagerOperation, /**< Manager operation that owns recovery. */
	WindowFrame, /**< Current window frame boundary. */
	WindowLifecycle, /**< Window creation, recreation, or destruction boundary. */
	AppPoll, /**< Application event-poll boundary. */
	AppLifecycle, /**< Application initialization or lifetime boundary. */
	Process, /**< Process termination boundary. */
};

/** Whether valid continuation exists, and who can select it. */
enum class ErrorResolvability : std::uint8_t {
	AutomaticallyResolved = 0, /**< The runtime applies a supported continuation. */
	CallerResolvable, /**< The caller may select a continuation. */
	RecreateWindow, /**< Continuation requires window recreation. */
	RecreateApp, /**< Continuation requires application recreation. */
	NotResolvable, /**< No supported continuation exists. */
};

/** Consequence of an occurrence after its configured/default resolution. */
enum class ErrorImpact : std::uint8_t {
	None = 0, /**< No observable degradation. */
	Degraded, /**< Operation continues with reduced fidelity. */
	OperationFailed, /**< The requested operation did not complete. */
	FrameCanceled, /**< The current frame was canceled. */
	WindowUnavailable, /**< A window cannot be used. */
	AppUnavailable, /**< The application cannot be used. */
	Fatal, /**< The process cannot safely continue. */
};

/** Who is allowed to select a resolution for this code. */
enum class ErrorResolutionPolicy : std::uint8_t {
	HardCoded = 0, /**< The runtime fixes the resolution. */
	AppConfigured, /**< An App-creation policy selects the resolution. */
	CallerDefined, /**< The calling application selects a resolution. */
	None, /**< No selectable resolution exists. */
};

/** Stable resolution vocabulary shared by observers and development reports. */
enum class ErrorResolution : std::uint8_t {
	None = 0, /**< No resolution recorded. */
	Rejected, /**< Reject the request without applying it. */
	Ignored, /**< Ignore the failing optional operation. */
	Retried, /**< Retry the operation. */
	UsedFallback, /**< Continue with a fallback resource or behavior. */
	Skipped, /**< Skip the affected operation or visual. */
	GrewCapacity, /**< Grow storage and continue. */
	EvictedAndRetried, /**< Evict cache entries and retry. */
	CanceledOperation, /**< Cancel the affected operation. */
	CanceledFrame, /**< Cancel the current frame. */
	RecreatedWindow, /**< Recreate the affected window. */
	ClosedWindow, /**< Close the affected window. */
	RecreatedApp, /**< Recreate the application. */
	Propagated, /**< Pass the failure to an enclosing boundary. */
	Halted, /**< Halt supported operation. */
	Terminated, /**< Terminate the process. */
};

/** Kind of stable numeric subject carried by FlowUiError::subject. */
enum class ErrorSubjectKind : std::uint8_t {
	None = 0, /**< No numeric subject attribution. */
	App, /**< App identity. */
	Window, /**< Window identity. */
	Frame, /**< Frame identity. */
	Submission, /**< Submission identity. */
	Resource, /**< Resource identity. */
	ResourceKey, /**< Resource Key identity. */
	ElementDefinition, /**< Element Definition identity. */
	ElementInstance, /**< Element Instance identity. */
	Action, /**< Action identity. */
	Theme, /**< Theme identity. */
	Font, /**< Font identity. */
	Image, /**< Image identity. */
	Icon, /**< Icon identity. */
	Popup, /**< Popup identity. */
	Shortcut, /**< Shortcut identity. */
	Viewport, /**< Viewport identity. */
	MemoryTarget, /**< Stable memory-capacity target identity. */
};

} // namespace FlowUi
