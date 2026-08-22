#pragma once

#include <cstdint>
#include <exception>
#include <expected>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace FlowUi {

/**
 * How a deliberately defined FlowUi failure travels to its resolution boundary.
 * This is independent from ErrorImpact: transport distance is not severity.
 */
enum class ErrorCategory : std::uint8_t {
	Static = 0,
	Local,
	Unwind,
	Fatal,
};

/** Stable ownership domain for an error code. */
enum class ErrorDomain : std::uint8_t {
	None = 0,
	Core,
	App,
	Platform,
	Window,
	Frame,
	Vulkan,
	Renderer,
	Storage,
	Resource,
	Asset,
	Font,
	Image,
	Icon,
	Element,
	Action,
	Theme,
	Input,
	Popup,
	Shortcut,
	Viewport,
};

/** The boundary that is intended to decide the outcome of an error. */
enum class ErrorBoundary : std::uint8_t {
	CompileExpression = 0,
	DirectCaller,
	ManagerOperation,
	WindowFrame,
	WindowLifecycle,
	AppPoll,
	AppLifecycle,
	Process,
};

/** Whether valid continuation exists, and who can select it. */
enum class ErrorResolvability : std::uint8_t {
	AutomaticallyResolved = 0,
	CallerResolvable,
	RecreateWindow,
	RecreateApp,
	NotResolvable,
};

/** Consequence of an occurrence after its configured/default resolution. */
enum class ErrorImpact : std::uint8_t {
	None = 0,
	Degraded,
	OperationFailed,
	FrameCanceled,
	WindowUnavailable,
	AppUnavailable,
	Fatal,
};

/** Who is allowed to select a resolution for this code. */
enum class ErrorResolutionPolicy : std::uint8_t {
	HardCoded = 0,
	AppConfigured,
	CallerDefined,
	None,
};

/** Stable resolution vocabulary shared by observers and development reports. */
enum class ErrorResolution : std::uint8_t {
	None = 0,
	Rejected,
	Ignored,
	Retried,
	UsedFallback,
	Skipped,
	GrewCapacity,
	EvictedAndRetried,
	CanceledOperation,
	CanceledFrame,
	RecreatedWindow,
	ClosedWindow,
	RecreatedApp,
	Propagated,
	Halted,
	Terminated,
};

/** App-creation policies for deliberately recoverable error families. */
enum class DefaultFontFailurePolicy : std::uint8_t {
	TryFallbackThenDisableText = 0,
	TryFallbackThenFailApp,
	FailAppImmediately,
};

enum class MissingVisualPolicy : std::uint8_t {
	UseFallbackTexture = 0,
	SkipVisual,
};

enum class IconGenerationFailurePolicy : std::uint8_t {
	UseFallbackTexture = 0,
	SkipVisual,
	FailRequest,
};

enum class CapacityFailurePolicy : std::uint8_t {
	GrowWithinBudget = 0,
	EvictAndRetry,
	RejectOperation,
};

enum class PopupDuplicatePolicy : std::uint8_t {
	FirstSubmissionWins = 0,
	RejectDuplicate,
};

enum class PopupMissingAnchorPolicy : std::uint8_t {
	SkipPopup = 0,
	DeferUntilAnchorExists,
	AnchorToViewport,
};

enum class PopupCapacityPolicy : std::uint8_t {
	ClampLayer = 0,
	SkipOverflowingPopup,
};

enum class InputQueueOverflowPolicy : std::uint8_t {
	DropNewest = 0,
	DropOldest,
};

/**
 * Error resolutions selected once at App creation. Development mode may observe
 * these decisions but never changes them.
 */
struct ErrorPolicy {
	DefaultFontFailurePolicy defaultFont = DefaultFontFailurePolicy::TryFallbackThenDisableText;
	MissingVisualPolicy missingImage = MissingVisualPolicy::UseFallbackTexture;
	MissingVisualPolicy missingViewport = MissingVisualPolicy::UseFallbackTexture;
	IconGenerationFailurePolicy iconGeneration = IconGenerationFailurePolicy::UseFallbackTexture;
	CapacityFailurePolicy transientCapacity = CapacityFailurePolicy::GrowWithinBudget;
	CapacityFailurePolicy persistentCapacity = CapacityFailurePolicy::GrowWithinBudget;
	CapacityFailurePolicy descriptorCapacity = CapacityFailurePolicy::RejectOperation;
	PopupDuplicatePolicy duplicatePopup = PopupDuplicatePolicy::FirstSubmissionWins;
	PopupMissingAnchorPolicy missingPopupAnchor = PopupMissingAnchorPolicy::SkipPopup;
	PopupCapacityPolicy popupCapacity = PopupCapacityPolicy::ClampLayer;
	InputQueueOverflowPolicy inputQueueOverflow = InputQueueOverflowPolicy::DropNewest;
	std::uint32_t inputTextQueueCapacity = 4096;
};

/** Kind of stable numeric subject carried by FlowUiError::subject. */
enum class ErrorSubjectKind : std::uint8_t {
	None = 0,
	App,
	Window,
	Frame,
	Submission,
	Resource,
	ResourceKey,
	ElementDefinition,
	ElementInstance,
	Action,
	Theme,
	Font,
	Image,
	Icon,
	Popup,
	Shortcut,
	Viewport,
	MemoryTarget,
};

/**
 * Initial stable code space for known FlowUi error families.
 *
 * Values are explicitly grouped by domain. Once a value is part of a published
 * API it must not be reused for a different meaning. More specific codes can be
 * added without changing the compact FlowUiError representation.
 */
enum class ErrorCode : std::uint16_t {
	None = 0x0000,

	InternalInvariantBroken = 0x0101,
	IdentitySpaceExhausted = 0x0102,
	ArithmeticOverflow = 0x0103,
	AllocationFailed = 0x0104,
	ObjectNotInitialized = 0x0105,
	ObjectAlreadyInitialized = 0x0106,
	UnsupportedBuildFeature = 0x0107,
	WrongThread = 0x0108,
	CleanupIncomplete = 0x0109,
	ForeignExceptionObserved = 0x010a,

	AppUnavailable = 0x0201,
	AppInitializationFailed = 0x0202,
	AppTickSpaceExhausted = 0x0203,

	PlatformInitializationFailed = 0x0301,
	PlatformMonitorUnavailable = 0x0302,
	PlatformRequiredExtensionMissing = 0x0303,
	PlatformSurfaceCreationFailed = 0x0304,
	PlatformInputQueueOverflow = 0x0305,

	InvalidWindowId = 0x0401,
	InvalidWindowConfiguration = 0x0402,
	WindowCreationFailed = 0x0403,
	WindowIdSpaceExhausted = 0x0404,
	WindowRegistryCollision = 0x0405,
	MainWindowDestructionForbidden = 0x0406,
	WindowPresentationUnsupported = 0x0407,
	WindowRecreationFailed = 0x0408,

	FrameAlreadyActive = 0x0501,
	FramePhaseViolation = 0x0502,
	FrameNotReady = 0x0503,
	FrameNumberSpaceExhausted = 0x0504,
	PreparedFrameStale = 0x0505,
	FrameCapacityExceeded = 0x0506,

	VulkanVersionUnsupported = 0x0601,
	VulkanExtensionMissing = 0x0602,
	VulkanDeviceUnavailable = 0x0603,
	VulkanFeatureMissing = 0x0604,
	VulkanNativeCallFailed = 0x0605,
	VulkanDeviceLost = 0x0606,
	SwapchainUnavailable = 0x0607,
	SwapchainOutOfDate = 0x0608,
	SwapchainImageInvalid = 0x0609,
	PresentationFailed = 0x060a,
	PresentIdSpaceExhausted = 0x060b,

	ShaderUnavailable = 0x0701,
	ShaderInvalid = 0x0702,
	RendererConfigurationInvalid = 0x0703,
	RendererCapacityExceeded = 0x0704,
	RenderCommandInvalid = 0x0705,
	RendererGenerationStale = 0x0706,
	RendererNativeResourceInvalid = 0x0707,

	StorageConfigurationInvalid = 0x0801,
	StorageBudgetExceeded = 0x0802,
	StorageCapacityExceeded = 0x0803,
	StorageMutationSealed = 0x0804,
	StorageFrameProtocolViolation = 0x0805,
	StorageSubmissionProtocolViolation = 0x0806,
	StoragePublicationFailed = 0x0807,
	StorageGenerationExhausted = 0x0808,

	InvalidResourceKey = 0x0901,
	ResourceNotFound = 0x0902,
	ResourceHandleInvalid = 0x0903,
	ResourceKindMismatch = 0x0904,
	ResourceWindowMismatch = 0x0905,
	ResourceCreationFailed = 0x0906,
	ResourcePublicationFailed = 0x0907,
	ResourceInUse = 0x0908,
	ResourceGenerationExhausted = 0x0909,

	AssetPathEmpty = 0x0a01,
	AssetNotFound = 0x0a02,
	AssetOpenFailed = 0x0a03,
	AssetReadFailed = 0x0a04,
	AssetFormatUnsupported = 0x0a05,
	AssetPayloadInvalid = 0x0a06,
	AssetDecodeFailed = 0x0a07,

	FontFamilyAlreadyExists = 0x0b01,
	FontFamilyNotFound = 0x0b02,
	FontBakeUnavailable = 0x0b03,
	FontBakeFailed = 0x0b04,
	FontAtlasCapacityExceeded = 0x0b05,
	FontIdSpaceExhausted = 0x0b06,
	DefaultFontUnavailable = 0x0b07,

	ImageDecodeFailed = 0x0c01,
	ImageSizeOverflow = 0x0c02,
	ImagePublicationFailed = 0x0c03,

	IconSourceInvalid = 0x0d01,
	IconNotFound = 0x0d02,
	IconRasterizationFailed = 0x0d03,
	IconRasterInvalid = 0x0d04,
	IconAtlasCapacityExceeded = 0x0d05,
	IconKeyCollision = 0x0d06,

	InvalidElementId = 0x0e01,
	ElementDefinitionConflict = 0x0e02,
	ElementStateUnavailable = 0x0e03,
	ElementResourceUnavailable = 0x0e04,
	ElementResourceConstructionFailed = 0x0e05,
	ElementResourceRecursiveConstruction = 0x0e06,
	ElementResourceDestroying = 0x0e07,
	ElementStorageStale = 0x0e08,

	InvalidActionId = 0x0f01,
	ActionAlreadyBound = 0x0f02,
	ActionNotBound = 0x0f03,
	ActionDisabled = 0x0f04,
	ActionResultTypeMismatch = 0x0f05,
	ActionPublicationConflict = 0x0f06,

	ThemeTypeNotFound = 0x1001,
	ThemeVariantNotFound = 0x1002,
	ThemeActiveVariantMissing = 0x1003,
	ThemeMutationFailed = 0x1004,

	InputFieldUnavailable = 0x1101,
	InputRangeInvalid = 0x1102,
	InputUtf8Invalid = 0x1103,
	InputSizeLimitExceeded = 0x1104,

	PopupFrameInactive = 0x1201,
	PopupDuplicateSubmission = 0x1202,
	PopupAnchorNotFound = 0x1203,
	PopupCapacityExceeded = 0x1204,

	ShortcutInvalid = 0x1301,
	ShortcutIdSpaceExhausted = 0x1302,
	ShortcutNotFound = 0x1303,

	ViewportDetached = 0x1401,
	ViewportConfigurationInvalid = 0x1402,
	ViewportNotFound = 0x1403,
	ViewportKeyCollision = 0x1404,
	ViewportGenerationIncomplete = 0x1405,
	ViewportGenerationExhausted = 0x1406,
	ViewportRecordingFailed = 0x1407,
};

/** Static catalog data associated with one ErrorCode. */
struct ErrorDescriptor {
	ErrorCode code = ErrorCode::None;
	ErrorDomain domain = ErrorDomain::None;
	ErrorCategory category = ErrorCategory::Local;
	ErrorBoundary boundary = ErrorBoundary::DirectCaller;
	ErrorResolvability resolvability = ErrorResolvability::CallerResolvable;
	ErrorImpact impact = ErrorImpact::None;
	ErrorResolutionPolicy resolutionPolicy = ErrorResolutionPolicy::CallerDefined;
	ErrorResolution defaultResolution = ErrorResolution::None;
	std::string_view name = "None";
	std::string_view message = "No FlowUi error.";
};

namespace detail {

[[nodiscard]] constexpr ErrorDomain errorDomainFromValue(ErrorCode code) noexcept {
	switch (static_cast<std::uint16_t>(code) >> 8u) {
	case 0x01: return ErrorDomain::Core;
	case 0x02: return ErrorDomain::App;
	case 0x03: return ErrorDomain::Platform;
	case 0x04: return ErrorDomain::Window;
	case 0x05: return ErrorDomain::Frame;
	case 0x06: return ErrorDomain::Vulkan;
	case 0x07: return ErrorDomain::Renderer;
	case 0x08: return ErrorDomain::Storage;
	case 0x09: return ErrorDomain::Resource;
	case 0x0a: return ErrorDomain::Asset;
	case 0x0b: return ErrorDomain::Font;
	case 0x0c: return ErrorDomain::Image;
	case 0x0d: return ErrorDomain::Icon;
	case 0x0e: return ErrorDomain::Element;
	case 0x0f: return ErrorDomain::Action;
	case 0x10: return ErrorDomain::Theme;
	case 0x11: return ErrorDomain::Input;
	case 0x12: return ErrorDomain::Popup;
	case 0x13: return ErrorDomain::Shortcut;
	case 0x14: return ErrorDomain::Viewport;
	default: return ErrorDomain::None;
	}
}

[[nodiscard]] constexpr ErrorDescriptor localDescriptor(
	ErrorCode code,
	std::string_view name,
	std::string_view message,
	ErrorBoundary boundary = ErrorBoundary::DirectCaller) noexcept {
	return ErrorDescriptor{
		.code = code,
		.domain = errorDomainFromValue(code),
		.category = ErrorCategory::Local,
		.boundary = boundary,
		.resolvability = ErrorResolvability::CallerResolvable,
		.impact = ErrorImpact::OperationFailed,
		.resolutionPolicy = ErrorResolutionPolicy::CallerDefined,
		.defaultResolution = ErrorResolution::Rejected,
		.name = name,
		.message = message,
	};
}

} // namespace detail

/** Return the initial descriptor for a stable error code. */
[[nodiscard]] constexpr ErrorDescriptor describeError(ErrorCode code) noexcept {
	using detail::localDescriptor;
	switch (code) {
	case ErrorCode::None:
		return {};

	case ErrorCode::InternalInvariantBroken:
		return {code, ErrorDomain::Core, ErrorCategory::Fatal, ErrorBoundary::Process,
			ErrorResolvability::NotResolvable, ErrorImpact::Fatal,
			ErrorResolutionPolicy::None, ErrorResolution::Terminated,
			"InternalInvariantBroken", "A required FlowUi internal invariant was broken."};
	case ErrorCode::IdentitySpaceExhausted:
		return {code, ErrorDomain::Core, ErrorCategory::Fatal, ErrorBoundary::Process,
			ErrorResolvability::NotResolvable, ErrorImpact::Fatal,
			ErrorResolutionPolicy::None, ErrorResolution::Terminated,
			"IdentitySpaceExhausted", "A non-reusable FlowUi identity space was exhausted."};
	case ErrorCode::ArithmeticOverflow:
		return localDescriptor(code, "ArithmeticOverflow", "A FlowUi size or offset calculation overflowed.");
	case ErrorCode::AllocationFailed:
		return {code, ErrorDomain::Core, ErrorCategory::Unwind, ErrorBoundary::AppLifecycle,
			ErrorResolvability::RecreateApp, ErrorImpact::AppUnavailable,
			ErrorResolutionPolicy::CallerDefined, ErrorResolution::Propagated,
			"AllocationFailed", "FlowUi could not allocate required memory."};
	case ErrorCode::ObjectNotInitialized:
		return localDescriptor(code, "ObjectNotInitialized", "The FlowUi object is not initialized.");
	case ErrorCode::ObjectAlreadyInitialized:
		return localDescriptor(code, "ObjectAlreadyInitialized", "The FlowUi object is already initialized.");
	case ErrorCode::UnsupportedBuildFeature:
		return {code, ErrorDomain::Core, ErrorCategory::Static, ErrorBoundary::CompileExpression,
			ErrorResolvability::NotResolvable, ErrorImpact::OperationFailed,
			ErrorResolutionPolicy::None, ErrorResolution::Rejected,
			"UnsupportedBuildFeature", "The requested FlowUi feature is not present in this build."};
	case ErrorCode::WrongThread:
		return localDescriptor(code, "WrongThread", "The operation was called from the wrong thread.");
	case ErrorCode::CleanupIncomplete:
		return {code, ErrorDomain::Core, ErrorCategory::Fatal, ErrorBoundary::Process,
			ErrorResolvability::NotResolvable, ErrorImpact::AppUnavailable,
			ErrorResolutionPolicy::HardCoded, ErrorResolution::Terminated,
			"CleanupIncomplete", "FlowUi could not complete required cleanup."};
	case ErrorCode::ForeignExceptionObserved:
		return {code, ErrorDomain::Core, ErrorCategory::Unwind, ErrorBoundary::AppLifecycle,
			ErrorResolvability::CallerResolvable, ErrorImpact::OperationFailed,
			ErrorResolutionPolicy::CallerDefined, ErrorResolution::Propagated,
			"ForeignExceptionObserved", "User or third-party code threw while FlowUi was active."};

	case ErrorCode::AppUnavailable:
		return localDescriptor(code, "AppUnavailable", "The FlowUi application is unavailable.");
	case ErrorCode::AppInitializationFailed:
		return {code, ErrorDomain::App, ErrorCategory::Unwind, ErrorBoundary::AppLifecycle,
			ErrorResolvability::CallerResolvable, ErrorImpact::AppUnavailable,
			ErrorResolutionPolicy::CallerDefined, ErrorResolution::Propagated,
			"AppInitializationFailed", "FlowUi application initialization failed."};
	case ErrorCode::AppTickSpaceExhausted:
		return {code, ErrorDomain::App, ErrorCategory::Fatal, ErrorBoundary::Process,
			ErrorResolvability::NotResolvable, ErrorImpact::Fatal,
			ErrorResolutionPolicy::None, ErrorResolution::Terminated,
			"AppTickSpaceExhausted", "The FlowUi application tick identity space was exhausted."};

	case ErrorCode::PlatformInitializationFailed:
		return {code, ErrorDomain::Platform, ErrorCategory::Unwind, ErrorBoundary::AppLifecycle,
			ErrorResolvability::CallerResolvable, ErrorImpact::AppUnavailable,
			ErrorResolutionPolicy::CallerDefined, ErrorResolution::Propagated,
			"PlatformInitializationFailed", "The platform window system could not be initialized."};
	case ErrorCode::PlatformMonitorUnavailable:
		return localDescriptor(code, "PlatformMonitorUnavailable", "The requested platform monitor is unavailable.");
	case ErrorCode::PlatformRequiredExtensionMissing:
		return {code, ErrorDomain::Platform, ErrorCategory::Unwind, ErrorBoundary::AppLifecycle,
			ErrorResolvability::CallerResolvable, ErrorImpact::AppUnavailable,
			ErrorResolutionPolicy::CallerDefined, ErrorResolution::Propagated,
			"PlatformRequiredExtensionMissing", "A required platform extension is unavailable."};
	case ErrorCode::PlatformSurfaceCreationFailed:
		return {code, ErrorDomain::Platform, ErrorCategory::Unwind, ErrorBoundary::WindowLifecycle,
			ErrorResolvability::CallerResolvable, ErrorImpact::WindowUnavailable,
			ErrorResolutionPolicy::CallerDefined, ErrorResolution::Propagated,
			"PlatformSurfaceCreationFailed", "The platform rendering surface could not be created."};
	case ErrorCode::PlatformInputQueueOverflow:
		return {code, ErrorDomain::Platform, ErrorCategory::Local, ErrorBoundary::ManagerOperation,
			ErrorResolvability::AutomaticallyResolved, ErrorImpact::Degraded,
			ErrorResolutionPolicy::AppConfigured, ErrorResolution::Ignored,
			"PlatformInputQueueOverflow", "The bounded platform input queue overflowed."};

	case ErrorCode::InvalidWindowId: return localDescriptor(code, "InvalidWindowId", "The window identity is invalid or no longer registered.");
	case ErrorCode::InvalidWindowConfiguration: return localDescriptor(code, "InvalidWindowConfiguration", "The window configuration is invalid.");
	case ErrorCode::WindowCreationFailed: return localDescriptor(code, "WindowCreationFailed", "The FlowUi window could not be created.", ErrorBoundary::WindowLifecycle);
	case ErrorCode::WindowIdSpaceExhausted:
	case ErrorCode::WindowRegistryCollision:
		return {code, ErrorDomain::Window, ErrorCategory::Fatal, ErrorBoundary::Process,
			ErrorResolvability::NotResolvable, ErrorImpact::Fatal,
			ErrorResolutionPolicy::None, ErrorResolution::Terminated,
			code == ErrorCode::WindowIdSpaceExhausted ? "WindowIdSpaceExhausted" : "WindowRegistryCollision",
			code == ErrorCode::WindowIdSpaceExhausted ? "The FlowUi window identity space was exhausted." : "The FlowUi window registry detected an identity collision."};
	case ErrorCode::MainWindowDestructionForbidden: return localDescriptor(code, "MainWindowDestructionForbidden", "The semantic main window cannot be destroyed explicitly.");
	case ErrorCode::WindowPresentationUnsupported: return localDescriptor(code, "WindowPresentationUnsupported", "The selected device cannot present to this window.", ErrorBoundary::WindowLifecycle);
	case ErrorCode::WindowRecreationFailed:
		return {code, ErrorDomain::Window, ErrorCategory::Unwind, ErrorBoundary::WindowLifecycle,
			ErrorResolvability::RecreateWindow, ErrorImpact::WindowUnavailable,
			ErrorResolutionPolicy::CallerDefined, ErrorResolution::Propagated,
			"WindowRecreationFailed", "The FlowUi window rendering resources could not be recreated."};

	case ErrorCode::FrameAlreadyActive: return localDescriptor(code, "FrameAlreadyActive", "Another FlowUi window frame is already active.");
	case ErrorCode::FramePhaseViolation: return localDescriptor(code, "FramePhaseViolation", "The frame operation is invalid in the current phase.");
	case ErrorCode::FrameNotReady: return localDescriptor(code, "FrameNotReady", "The FlowUi window is not ready for the requested frame operation.");
	case ErrorCode::FrameNumberSpaceExhausted:
		return {code, ErrorDomain::Frame, ErrorCategory::Fatal, ErrorBoundary::Process,
			ErrorResolvability::NotResolvable, ErrorImpact::Fatal,
			ErrorResolutionPolicy::None, ErrorResolution::Terminated,
			"FrameNumberSpaceExhausted", "The FlowUi frame identity space was exhausted."};
	case ErrorCode::PreparedFrameStale:
		return {code, ErrorDomain::Frame, ErrorCategory::Fatal, ErrorBoundary::WindowFrame,
			ErrorResolvability::NotResolvable, ErrorImpact::Fatal,
			ErrorResolutionPolicy::None, ErrorResolution::Halted,
			"PreparedFrameStale", "Prepared UI data does not match its sealed storage frame."};
	case ErrorCode::FrameCapacityExceeded:
		return {code, ErrorDomain::Frame, ErrorCategory::Unwind, ErrorBoundary::WindowFrame,
			ErrorResolvability::CallerResolvable, ErrorImpact::FrameCanceled,
			ErrorResolutionPolicy::CallerDefined, ErrorResolution::Propagated,
			"FrameCapacityExceeded", "The frame exceeded a configured FlowUi capacity."};

	case ErrorCode::VulkanVersionUnsupported:
	case ErrorCode::VulkanExtensionMissing:
	case ErrorCode::VulkanDeviceUnavailable:
	case ErrorCode::VulkanFeatureMissing:
		return {code, ErrorDomain::Vulkan, ErrorCategory::Unwind, ErrorBoundary::AppLifecycle,
			ErrorResolvability::CallerResolvable, ErrorImpact::AppUnavailable,
			ErrorResolutionPolicy::CallerDefined, ErrorResolution::Propagated,
			code == ErrorCode::VulkanVersionUnsupported ? "VulkanVersionUnsupported" :
				(code == ErrorCode::VulkanExtensionMissing ? "VulkanExtensionMissing" :
				(code == ErrorCode::VulkanDeviceUnavailable ? "VulkanDeviceUnavailable" : "VulkanFeatureMissing")),
			code == ErrorCode::VulkanVersionUnsupported ? "The required Vulkan version is unavailable." :
				(code == ErrorCode::VulkanExtensionMissing ? "A required Vulkan extension is unavailable." :
				(code == ErrorCode::VulkanDeviceUnavailable ? "No compatible Vulkan device is available." : "The selected Vulkan device lacks a required feature."))};
	case ErrorCode::VulkanNativeCallFailed:
		return {code, ErrorDomain::Vulkan, ErrorCategory::Unwind, ErrorBoundary::WindowFrame,
			ErrorResolvability::CallerResolvable, ErrorImpact::FrameCanceled,
			ErrorResolutionPolicy::CallerDefined, ErrorResolution::Propagated,
			"VulkanNativeCallFailed", "A Vulkan operation failed."};
	case ErrorCode::VulkanDeviceLost:
		return {code, ErrorDomain::Vulkan, ErrorCategory::Unwind, ErrorBoundary::AppLifecycle,
			ErrorResolvability::RecreateApp, ErrorImpact::AppUnavailable,
			ErrorResolutionPolicy::CallerDefined, ErrorResolution::Propagated,
			"VulkanDeviceLost", "The Vulkan device was lost."};
	case ErrorCode::SwapchainUnavailable: return localDescriptor(code, "SwapchainUnavailable", "A usable swapchain could not be created.", ErrorBoundary::WindowLifecycle);
	case ErrorCode::SwapchainOutOfDate:
		return {code, ErrorDomain::Vulkan, ErrorCategory::Local, ErrorBoundary::WindowLifecycle,
			ErrorResolvability::AutomaticallyResolved, ErrorImpact::FrameCanceled,
			ErrorResolutionPolicy::HardCoded, ErrorResolution::RecreatedWindow,
			"SwapchainOutOfDate", "The window swapchain is out of date."};
	case ErrorCode::SwapchainImageInvalid:
		return {code, ErrorDomain::Vulkan, ErrorCategory::Fatal, ErrorBoundary::Process,
			ErrorResolvability::NotResolvable, ErrorImpact::Fatal,
			ErrorResolutionPolicy::None, ErrorResolution::Terminated,
			"SwapchainImageInvalid", "The acquired swapchain image is inconsistent with FlowUi state."};
	case ErrorCode::PresentationFailed:
		return {code, ErrorDomain::Vulkan, ErrorCategory::Unwind, ErrorBoundary::WindowLifecycle,
			ErrorResolvability::RecreateWindow, ErrorImpact::WindowUnavailable,
			ErrorResolutionPolicy::CallerDefined, ErrorResolution::Propagated,
			"PresentationFailed", "Presentation of a FlowUi window failed."};
	case ErrorCode::PresentIdSpaceExhausted:
		return {code, ErrorDomain::Vulkan, ErrorCategory::Fatal, ErrorBoundary::Process,
			ErrorResolvability::NotResolvable, ErrorImpact::Fatal,
			ErrorResolutionPolicy::None, ErrorResolution::Terminated,
			"PresentIdSpaceExhausted", "The swapchain presentation identity space was exhausted."};

	case ErrorCode::ShaderUnavailable:
	case ErrorCode::ShaderInvalid:
	case ErrorCode::RendererConfigurationInvalid:
		return {code, ErrorDomain::Renderer, ErrorCategory::Unwind, ErrorBoundary::AppLifecycle,
			ErrorResolvability::CallerResolvable, ErrorImpact::AppUnavailable,
			ErrorResolutionPolicy::CallerDefined, ErrorResolution::Propagated,
			code == ErrorCode::ShaderUnavailable ? "ShaderUnavailable" :
				(code == ErrorCode::ShaderInvalid ? "ShaderInvalid" : "RendererConfigurationInvalid"),
			code == ErrorCode::ShaderUnavailable ? "A required renderer shader is unavailable." :
				(code == ErrorCode::ShaderInvalid ? "A renderer shader payload is invalid." : "The renderer configuration is invalid.")};
	case ErrorCode::RendererCapacityExceeded:
		return {code, ErrorDomain::Renderer, ErrorCategory::Unwind, ErrorBoundary::WindowFrame,
			ErrorResolvability::CallerResolvable, ErrorImpact::FrameCanceled,
			ErrorResolutionPolicy::CallerDefined, ErrorResolution::Propagated,
			"RendererCapacityExceeded", "The renderer exceeded a configured capacity."};
	case ErrorCode::RenderCommandInvalid:
		return {code, ErrorDomain::Renderer, ErrorCategory::Unwind, ErrorBoundary::WindowFrame,
			ErrorResolvability::CallerResolvable, ErrorImpact::FrameCanceled,
			ErrorResolutionPolicy::CallerDefined, ErrorResolution::Propagated,
			"RenderCommandInvalid", "The UI render command stream is invalid."};
	case ErrorCode::RendererGenerationStale:
	case ErrorCode::RendererNativeResourceInvalid:
		return {code, ErrorDomain::Renderer, ErrorCategory::Fatal, ErrorBoundary::Process,
			ErrorResolvability::NotResolvable, ErrorImpact::Fatal,
			ErrorResolutionPolicy::None, ErrorResolution::Terminated,
			code == ErrorCode::RendererGenerationStale ? "RendererGenerationStale" : "RendererNativeResourceInvalid",
			code == ErrorCode::RendererGenerationStale ? "The renderer received a stale resource generation." : "The renderer received invalid committed native resources."};

	case ErrorCode::StorageConfigurationInvalid: return localDescriptor(code, "StorageConfigurationInvalid", "The storage configuration is invalid.");
	case ErrorCode::StorageBudgetExceeded:
	case ErrorCode::StorageCapacityExceeded:
		return {code, ErrorDomain::Storage, ErrorCategory::Local, ErrorBoundary::DirectCaller,
			ErrorResolvability::CallerResolvable, ErrorImpact::OperationFailed,
			ErrorResolutionPolicy::AppConfigured, ErrorResolution::Rejected,
			code == ErrorCode::StorageBudgetExceeded ? "StorageBudgetExceeded" : "StorageCapacityExceeded",
			code == ErrorCode::StorageBudgetExceeded ? "A FlowUi storage budget was exceeded." : "A FlowUi storage capacity was exceeded."};
	case ErrorCode::StorageMutationSealed: return localDescriptor(code, "StorageMutationSealed", "Storage mutation is not allowed while a sealed snapshot is active.");
	case ErrorCode::StorageFrameProtocolViolation:
		return localDescriptor(code, "StorageFrameProtocolViolation",
			"The storage frame operation violates the current frame state.", ErrorBoundary::WindowFrame);
	case ErrorCode::StorageSubmissionProtocolViolation:
		return {code, ErrorDomain::Storage, ErrorCategory::Fatal, ErrorBoundary::Process,
			ErrorResolvability::NotResolvable, ErrorImpact::Fatal,
			ErrorResolutionPolicy::None, ErrorResolution::Terminated,
			"StorageSubmissionProtocolViolation", "The storage submission ownership protocol was violated."};
	case ErrorCode::StoragePublicationFailed: return localDescriptor(code, "StoragePublicationFailed", "Storage could not publish the requested record or resource.");
	case ErrorCode::StorageGenerationExhausted:
		return {code, ErrorDomain::Storage, ErrorCategory::Fatal, ErrorBoundary::Process,
			ErrorResolvability::NotResolvable, ErrorImpact::Fatal,
			ErrorResolutionPolicy::None, ErrorResolution::Terminated,
			"StorageGenerationExhausted", "A storage generation space was exhausted."};

	case ErrorCode::InvalidResourceKey: return localDescriptor(code, "InvalidResourceKey", "The resource key is invalid for this manager.");
	case ErrorCode::ResourceNotFound: return localDescriptor(code, "ResourceNotFound", "The requested FlowUi resource was not found.");
	case ErrorCode::ResourceHandleInvalid: return localDescriptor(code, "ResourceHandleInvalid", "The FlowUi resource handle is invalid or stale.");
	case ErrorCode::ResourceKindMismatch: return localDescriptor(code, "ResourceKindMismatch", "The FlowUi resource has the wrong kind.");
	case ErrorCode::ResourceWindowMismatch: return localDescriptor(code, "ResourceWindowMismatch", "The FlowUi resource belongs to a different window.");
	case ErrorCode::ResourceCreationFailed: return localDescriptor(code, "ResourceCreationFailed", "The FlowUi resource could not be created.");
	case ErrorCode::ResourcePublicationFailed: return localDescriptor(code, "ResourcePublicationFailed", "The FlowUi resource could not be published.");
	case ErrorCode::ResourceInUse: return localDescriptor(code, "ResourceInUse", "The FlowUi resource is still in use.");
	case ErrorCode::ResourceGenerationExhausted:
		return {code, ErrorDomain::Resource, ErrorCategory::Fatal, ErrorBoundary::Process,
			ErrorResolvability::NotResolvable, ErrorImpact::Fatal,
			ErrorResolutionPolicy::None, ErrorResolution::Terminated,
			"ResourceGenerationExhausted", "A resource generation space was exhausted."};

	case ErrorCode::AssetPathEmpty: return localDescriptor(code, "AssetPathEmpty", "The asset path is empty.");
	case ErrorCode::AssetNotFound: return localDescriptor(code, "AssetNotFound", "The requested asset does not exist.");
	case ErrorCode::AssetOpenFailed: return localDescriptor(code, "AssetOpenFailed", "The asset could not be opened.");
	case ErrorCode::AssetReadFailed: return localDescriptor(code, "AssetReadFailed", "The asset could not be read.");
	case ErrorCode::AssetFormatUnsupported: return localDescriptor(code, "AssetFormatUnsupported", "The asset format is unsupported.");
	case ErrorCode::AssetPayloadInvalid: return localDescriptor(code, "AssetPayloadInvalid", "The asset payload is invalid.");
	case ErrorCode::AssetDecodeFailed: return localDescriptor(code, "AssetDecodeFailed", "The asset could not be decoded.");

	case ErrorCode::FontFamilyAlreadyExists: return localDescriptor(code, "FontFamilyAlreadyExists", "The font family is already registered.");
	case ErrorCode::FontFamilyNotFound: return localDescriptor(code, "FontFamilyNotFound", "The font family is not registered.");
	case ErrorCode::FontBakeUnavailable: return localDescriptor(code, "FontBakeUnavailable", "Runtime font baking is unavailable in this build.");
	case ErrorCode::FontBakeFailed: return localDescriptor(code, "FontBakeFailed", "Runtime font baking failed.");
	case ErrorCode::FontAtlasCapacityExceeded:
		return {code, ErrorDomain::Font, ErrorCategory::Local, ErrorBoundary::ManagerOperation,
			ErrorResolvability::CallerResolvable, ErrorImpact::OperationFailed,
			ErrorResolutionPolicy::AppConfigured, ErrorResolution::Rejected,
			"FontAtlasCapacityExceeded", "The font atlas capacity was exceeded."};
	case ErrorCode::FontIdSpaceExhausted: return localDescriptor(code, "FontIdSpaceExhausted", "The font identity space was exhausted.", ErrorBoundary::ManagerOperation);
	case ErrorCode::DefaultFontUnavailable:
		return {code, ErrorDomain::Font, ErrorCategory::Local, ErrorBoundary::ManagerOperation,
			ErrorResolvability::CallerResolvable, ErrorImpact::Degraded,
			ErrorResolutionPolicy::AppConfigured, ErrorResolution::UsedFallback,
			"DefaultFontUnavailable", "No configured default font could be loaded."};

	case ErrorCode::ImageDecodeFailed: return localDescriptor(code, "ImageDecodeFailed", "The image could not be decoded.");
	case ErrorCode::ImageSizeOverflow: return localDescriptor(code, "ImageSizeOverflow", "The decoded image is too large to address.");
	case ErrorCode::ImagePublicationFailed: return localDescriptor(code, "ImagePublicationFailed", "The decoded image could not be published.");

	case ErrorCode::IconSourceInvalid: return localDescriptor(code, "IconSourceInvalid", "The icon source is invalid.");
	case ErrorCode::IconNotFound: return localDescriptor(code, "IconNotFound", "The requested icon is not registered.");
	case ErrorCode::IconRasterizationFailed: return localDescriptor(code, "IconRasterizationFailed", "The icon could not be rasterized.");
	case ErrorCode::IconRasterInvalid: return localDescriptor(code, "IconRasterInvalid", "The icon raster payload is invalid.");
	case ErrorCode::IconAtlasCapacityExceeded:
		return {code, ErrorDomain::Icon, ErrorCategory::Local, ErrorBoundary::ManagerOperation,
			ErrorResolvability::CallerResolvable, ErrorImpact::Degraded,
			ErrorResolutionPolicy::AppConfigured, ErrorResolution::UsedFallback,
			"IconAtlasCapacityExceeded", "The icon atlas has no usable capacity."};
	case ErrorCode::IconKeyCollision:
		return {code, ErrorDomain::Icon, ErrorCategory::Fatal, ErrorBoundary::Process,
			ErrorResolvability::NotResolvable, ErrorImpact::Fatal,
			ErrorResolutionPolicy::None, ErrorResolution::Terminated,
			"IconKeyCollision", "An internally generated icon key collided."};

	case ErrorCode::InvalidElementId: return localDescriptor(code, "InvalidElementId", "The Flow element identity is invalid.");
	case ErrorCode::ElementDefinitionConflict: return localDescriptor(code, "ElementDefinitionConflict", "The Flow element definition conflicts with an existing definition.", ErrorBoundary::ManagerOperation);
	case ErrorCode::ElementStateUnavailable: return localDescriptor(code, "ElementStateUnavailable", "The Flow element state is unavailable.");
	case ErrorCode::ElementResourceUnavailable: return localDescriptor(code, "ElementResourceUnavailable", "The Flow element resource is unavailable.");
	case ErrorCode::ElementResourceConstructionFailed:
		return {code, ErrorDomain::Element, ErrorCategory::Unwind, ErrorBoundary::WindowFrame,
			ErrorResolvability::CallerResolvable, ErrorImpact::FrameCanceled,
			ErrorResolutionPolicy::CallerDefined, ErrorResolution::Propagated,
			"ElementResourceConstructionFailed", "Flow element resource construction failed."};
	case ErrorCode::ElementResourceRecursiveConstruction: return localDescriptor(code, "ElementResourceRecursiveConstruction", "Flow element resource construction is recursive.");
	case ErrorCode::ElementResourceDestroying: return localDescriptor(code, "ElementResourceDestroying", "Flow element resources are being destroyed.");
	case ErrorCode::ElementStorageStale:
		return {code, ErrorDomain::Element, ErrorCategory::Fatal, ErrorBoundary::Process,
			ErrorResolvability::NotResolvable, ErrorImpact::Fatal,
			ErrorResolutionPolicy::None, ErrorResolution::Terminated,
			"ElementStorageStale", "Committed Flow element storage became stale."};

	case ErrorCode::InvalidActionId: return localDescriptor(code, "InvalidActionId", "The action identity is invalid.");
	case ErrorCode::ActionAlreadyBound: return localDescriptor(code, "ActionAlreadyBound", "The action is already bound.");
	case ErrorCode::ActionNotBound: return localDescriptor(code, "ActionNotBound", "The action is not bound.");
	case ErrorCode::ActionDisabled: return localDescriptor(code, "ActionDisabled", "The action is disabled.");
	case ErrorCode::ActionResultTypeMismatch: return localDescriptor(code, "ActionResultTypeMismatch", "The action result type does not match the invocation.");
	case ErrorCode::ActionPublicationConflict:
		return {code, ErrorDomain::Action, ErrorCategory::Fatal, ErrorBoundary::Process,
			ErrorResolvability::NotResolvable, ErrorImpact::Fatal,
			ErrorResolutionPolicy::None, ErrorResolution::Terminated,
			"ActionPublicationConflict", "Action binding state changed during publication."};

	case ErrorCode::ThemeTypeNotFound: return localDescriptor(code, "ThemeTypeNotFound", "The requested theme type is not registered.");
	case ErrorCode::ThemeVariantNotFound: return localDescriptor(code, "ThemeVariantNotFound", "The requested theme variant is not registered.");
	case ErrorCode::ThemeActiveVariantMissing: return localDescriptor(code, "ThemeActiveVariantMissing", "The theme type has no active variant.");
	case ErrorCode::ThemeMutationFailed:
		return {code, ErrorDomain::Theme, ErrorCategory::Unwind, ErrorBoundary::AppPoll,
			ErrorResolvability::CallerResolvable, ErrorImpact::OperationFailed,
			ErrorResolutionPolicy::CallerDefined, ErrorResolution::Propagated,
			"ThemeMutationFailed", "A staged theme mutation failed."};

	case ErrorCode::InputFieldUnavailable: return localDescriptor(code, "InputFieldUnavailable", "The requested input field is unavailable.");
	case ErrorCode::InputRangeInvalid: return localDescriptor(code, "InputRangeInvalid", "The input edit range is invalid.");
	case ErrorCode::InputUtf8Invalid: return localDescriptor(code, "InputUtf8Invalid", "The input text is not valid UTF-8.");
	case ErrorCode::InputSizeLimitExceeded: return localDescriptor(code, "InputSizeLimitExceeded", "The input text exceeds its configured size limit.");

	case ErrorCode::PopupFrameInactive: return localDescriptor(code, "PopupFrameInactive", "The popup operation requires an active UI frame.");
	case ErrorCode::PopupDuplicateSubmission:
		return {code, ErrorDomain::Popup, ErrorCategory::Local, ErrorBoundary::ManagerOperation,
			ErrorResolvability::CallerResolvable, ErrorImpact::Degraded,
			ErrorResolutionPolicy::AppConfigured, ErrorResolution::Rejected,
			"PopupDuplicateSubmission", "The popup was submitted more than once in one frame."};
	case ErrorCode::PopupAnchorNotFound:
		return {code, ErrorDomain::Popup, ErrorCategory::Local, ErrorBoundary::ManagerOperation,
			ErrorResolvability::AutomaticallyResolved, ErrorImpact::Degraded,
			ErrorResolutionPolicy::AppConfigured, ErrorResolution::Skipped,
			"PopupAnchorNotFound", "The popup anchor was not found."};
	case ErrorCode::PopupCapacityExceeded:
		return {code, ErrorDomain::Popup, ErrorCategory::Local, ErrorBoundary::ManagerOperation,
			ErrorResolvability::AutomaticallyResolved, ErrorImpact::Degraded,
			ErrorResolutionPolicy::AppConfigured, ErrorResolution::Skipped,
			"PopupCapacityExceeded", "The popup capacity was exceeded."};

	case ErrorCode::ShortcutInvalid: return localDescriptor(code, "ShortcutInvalid", "The shortcut registration is invalid.");
	case ErrorCode::ShortcutIdSpaceExhausted: return localDescriptor(code, "ShortcutIdSpaceExhausted", "The shortcut identity space was exhausted.", ErrorBoundary::ManagerOperation);
	case ErrorCode::ShortcutNotFound: return localDescriptor(code, "ShortcutNotFound", "The shortcut registration was not found.");

	case ErrorCode::ViewportDetached: return localDescriptor(code, "ViewportDetached", "The viewport is detached from storage.");
	case ErrorCode::ViewportConfigurationInvalid: return localDescriptor(code, "ViewportConfigurationInvalid", "The viewport configuration is invalid.");
	case ErrorCode::ViewportNotFound:
		return {code, ErrorDomain::Viewport, ErrorCategory::Local, ErrorBoundary::ManagerOperation,
			ErrorResolvability::AutomaticallyResolved, ErrorImpact::Degraded,
			ErrorResolutionPolicy::AppConfigured, ErrorResolution::UsedFallback,
			"ViewportNotFound", "The requested viewport was not found."};
	case ErrorCode::ViewportKeyCollision:
		return localDescriptor(code, "ViewportKeyCollision", "The viewport key is already registered.", ErrorBoundary::ManagerOperation);
	case ErrorCode::ViewportGenerationIncomplete:
	case ErrorCode::ViewportGenerationExhausted:
		return {code, ErrorDomain::Viewport, ErrorCategory::Fatal, ErrorBoundary::Process,
			ErrorResolvability::NotResolvable, ErrorImpact::Fatal,
			ErrorResolutionPolicy::None, ErrorResolution::Terminated,
			code == ErrorCode::ViewportGenerationIncomplete ? "ViewportGenerationIncomplete" : "ViewportGenerationExhausted",
			code == ErrorCode::ViewportGenerationIncomplete ? "A published viewport generation is incomplete." : "The viewport generation identity space was exhausted."};
	case ErrorCode::ViewportRecordingFailed:
		return {code, ErrorDomain::Viewport, ErrorCategory::Unwind, ErrorBoundary::WindowFrame,
			ErrorResolvability::CallerResolvable, ErrorImpact::FrameCanceled,
			ErrorResolutionPolicy::CallerDefined, ErrorResolution::Propagated,
			"ViewportRecordingFailed", "Viewport command recording failed."};
	}
	return {code, detail::errorDomainFromValue(code), ErrorCategory::Fatal,
		ErrorBoundary::Process, ErrorResolvability::NotResolvable, ErrorImpact::Fatal,
		ErrorResolutionPolicy::None, ErrorResolution::Terminated,
		"UnknownErrorCode", "An unknown FlowUi error code was observed."};
}

[[nodiscard]] constexpr std::string_view errorName(ErrorCode code) noexcept {
	return describeError(code).name;
}

[[nodiscard]] constexpr std::string_view errorMessage(ErrorCode code) noexcept {
	return describeError(code).message;
}

/**
 * Small, allocation-free production error value.
 *
 * Debug names, source locations, stack traces, and state snapshots belong to the
 * optional development sidecar keyed by occurrence.
 */
struct FlowUiError final {
	ErrorCode code = ErrorCode::None;
	ErrorDomain domain = ErrorDomain::None;
	ErrorSubjectKind subjectKind = ErrorSubjectKind::None;
	std::uint32_t nativeCode = 0u;
	std::uint64_t subject = 0u;
	std::uint64_t auxiliary = 0u;
	std::uint64_t occurrence = 0u;

	[[nodiscard]] constexpr bool hasError() const noexcept {
		return code != ErrorCode::None;
	}

	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return hasError();
	}

	[[nodiscard]] constexpr ErrorDescriptor descriptor() const noexcept {
		return describeError(code);
	}

	[[nodiscard]] constexpr std::string_view message() const noexcept {
		return errorMessage(code);
	}
};

static_assert(std::is_trivially_copyable_v<FlowUiError>);
static_assert(std::is_trivially_destructible_v<FlowUiError>);
static_assert(sizeof(FlowUiError) <= 32u);

/** Construct a production error without allocating or formatting text. */
[[nodiscard]] constexpr FlowUiError makeError(
	ErrorCode code,
	ErrorSubjectKind subjectKind = ErrorSubjectKind::None,
	std::uint64_t subject = 0u,
	std::uint64_t auxiliary = 0u,
	std::uint32_t nativeCode = 0u,
	std::uint64_t occurrence = 0u) noexcept {
	return FlowUiError{
		.code = code,
		.domain = detail::errorDomainFromValue(code),
		.subjectKind = subjectKind,
		.nativeCode = nativeCode,
		.subject = subject,
		.auxiliary = auxiliary,
		.occurrence = occurrence,
	};
}

#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L

/** Canonical local-failure result. */
template <typename T>
using Result = std::expected<T, FlowUiError>;

/** Canonical no-value local-failure result. */
using Status = Result<void>;

[[nodiscard]] constexpr std::unexpected<FlowUiError> unexpectedError(FlowUiError error) noexcept {
	return std::unexpected<FlowUiError>(error);
}

#else

namespace detail {
struct UnexpectedFlowUiError {
	FlowUiError error{};
};
} // namespace detail

/** Compatibility result used only when the selected C++ library lacks expected. */
template <typename T>
class Result {
public:
	Result() requires std::is_default_constructible_v<T> : storage_(T{}) {}
	Result(const T& value) : storage_(value) {}
	Result(T&& value) : storage_(std::move(value)) {}
	Result(detail::UnexpectedFlowUiError failure) : storage_(failure.error) {}

	[[nodiscard]] bool has_value() const noexcept { return storage_.index() == 0u; }
	[[nodiscard]] explicit operator bool() const noexcept { return has_value(); }
	[[nodiscard]] T& value() & { return std::get<0>(storage_); }
	[[nodiscard]] const T& value() const& { return std::get<0>(storage_); }
	[[nodiscard]] T&& value() && { return std::get<0>(std::move(storage_)); }
	[[nodiscard]] T& operator*() & { return value(); }
	[[nodiscard]] const T& operator*() const& { return value(); }
	[[nodiscard]] T* operator->() { return &value(); }
	[[nodiscard]] const T* operator->() const { return &value(); }
	[[nodiscard]] FlowUiError& error() & { return std::get<1>(storage_); }
	[[nodiscard]] const FlowUiError& error() const& { return std::get<1>(storage_); }

	template <typename U>
	[[nodiscard]] T value_or(U&& fallback) const& {
		return has_value() ? std::get<0>(storage_) : static_cast<T>(std::forward<U>(fallback));
	}

private:
	std::variant<T, FlowUiError> storage_;
};

template <>
class Result<void> {
public:
	Result() = default;
	Result(detail::UnexpectedFlowUiError failure) : error_(failure.error), hasValue_(false) {}
	[[nodiscard]] bool has_value() const noexcept { return hasValue_; }
	[[nodiscard]] explicit operator bool() const noexcept { return has_value(); }
	void value() const {
		if (!hasValue_) throw std::bad_variant_access{};
	}
	[[nodiscard]] FlowUiError& error() & { return error_; }
	[[nodiscard]] const FlowUiError& error() const& { return error_; }

private:
	FlowUiError error_{};
	bool hasValue_ = true;
};

using Status = Result<void>;

[[nodiscard]] constexpr detail::UnexpectedFlowUiError unexpectedError(FlowUiError error) noexcept {
	return detail::UnexpectedFlowUiError{error};
}

#endif

/** Typed distant-unwind carrier for a FlowUi-authored error. */
class FlowUiException final : public std::exception {
public:
	explicit FlowUiException(FlowUiError error) noexcept : error_(error) {}

	[[nodiscard]] const FlowUiError& error() const noexcept { return error_; }

	[[nodiscard]] const char* what() const noexcept override {
		// All descriptor messages are null-terminated string literals.
		return errorMessage(error_.code).data();
	}

private:
	FlowUiError error_{};
};

enum class ErrorEventKind : std::uint8_t {
	Raised = 0,
	Propagated,
	Resolved,
	Escalated,
	FatalHalt,
};

/** Allocation-free view delivered to production and development observers. */
struct ErrorEventView {
	FlowUiError error{};
	ErrorEventKind kind = ErrorEventKind::Raised;
	ErrorBoundary boundary = ErrorBoundary::DirectCaller;
	ErrorResolution resolution = ErrorResolution::None;
};

using ErrorObserverCallback = void(*)(void* userData, const ErrorEventView& event) noexcept;

/** Non-owning observer plumbing. It cannot alter the error's semantics. */
struct ErrorObserver {
	void* userData = nullptr;
	ErrorObserverCallback callback = nullptr;

	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return callback != nullptr;
	}

	void notify(const ErrorEventView& event) const noexcept {
		if (callback) callback(userData, event);
	}
};

/** What in-process evidence access remains trustworthy after a fatal error. */
enum class FatalInspectionCapability : std::uint8_t {
	None = 0,
	TerminalOnly,
	ImmutableSnapshot,
	ReadOnlyInterface,
};

/** A fatal request is terminal even when a dev build freezes before termination. */
struct FatalErrorEvent {
	FlowUiError error{};
	FatalInspectionCapability inspection = FatalInspectionCapability::None;
};

using FatalErrorHandler = void(*)(void* userData, const FatalErrorEvent& event) noexcept;

struct FatalErrorSink {
	void* userData = nullptr;
	FatalErrorHandler handler = nullptr;

	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return handler != nullptr;
	}

	/** Report evidence and return to the fatal machinery, which still terminates. */
	void notify(const FatalErrorEvent& event) const noexcept {
		if (handler) handler(userData, event);
	}
};

namespace detail {

/** Notify the evidence-only sink, then enforce the no-continuation contract. */
[[noreturn]] inline void terminateForFatalError(
	FlowUiError error,
	const FatalErrorSink& sink = {}) noexcept {
	sink.notify(FatalErrorEvent{
		.error = error,
		.inspection = FatalInspectionCapability::TerminalOnly,
	});
	std::terminate();
}

} // namespace detail

static_assert(noexcept(makeError(ErrorCode::None)));
static_assert(noexcept(std::declval<const ErrorObserver&>().notify(std::declval<const ErrorEventView&>())));
static_assert(noexcept(std::declval<const FatalErrorSink&>().notify(std::declval<const FatalErrorEvent&>())));

} // namespace FlowUi
