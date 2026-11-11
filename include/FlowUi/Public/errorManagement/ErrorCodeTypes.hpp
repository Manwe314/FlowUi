#pragma once

#include "FlowUi/Public/errorManagement/ErrorTypes.hpp"
#include <cstdint>
#include <string_view>

namespace FlowUi {

/**
 * Initial stable code space for known FlowUi error families.
 *
 * Values are explicitly grouped by domain. Once a value is part of a published
 * API it must not be reused for a different meaning. More specific codes can be
 * added without changing the compact FlowUiError representation.
 */
enum class ErrorCode : std::uint16_t {
	None = 0x0000, /**< No error. */

	InternalInvariantBroken = 0x0101, /**< A required FlowUi internal invariant was broken. */
	IdentitySpaceExhausted = 0x0102, /**< A non-reusable FlowUi identity space was exhausted. */
	ArithmeticOverflow = 0x0103, /**< A FlowUi size or offset calculation overflowed. */
	AllocationFailed = 0x0104, /**< FlowUi could not allocate required memory. */
	ObjectNotInitialized = 0x0105, /**< The FlowUi object is not initialized. */
	ObjectAlreadyInitialized = 0x0106, /**< The FlowUi object is already initialized. */
	UnsupportedBuildFeature = 0x0107, /**< The requested FlowUi feature is not present in this build. */
	WrongThread = 0x0108, /**< The operation was called from the wrong thread. */
	CleanupIncomplete = 0x0109, /**< FlowUi could not complete required cleanup. */
	ForeignExceptionObserved = 0x010a, /**< User or third-party code threw while FlowUi was active. */

	AppUnavailable = 0x0201, /**< The FlowUi application is unavailable. */
	AppInitializationFailed = 0x0202, /**< FlowUi application initialization failed. */
	AppTickSpaceExhausted = 0x0203, /**< The FlowUi application tick identity space was exhausted. */

	PlatformInitializationFailed = 0x0301, /**< The platform window system could not be initialized. */
	PlatformMonitorUnavailable = 0x0302, /**< The requested platform monitor is unavailable. */
	PlatformRequiredExtensionMissing = 0x0303, /**< A required platform extension is unavailable. */
	PlatformSurfaceCreationFailed = 0x0304, /**< The platform rendering surface could not be created. */
	PlatformInputQueueOverflow = 0x0305, /**< The bounded platform input queue overflowed. */

	InvalidWindowId = 0x0401, /**< The window identity is invalid or no longer registered. */
	InvalidWindowConfiguration = 0x0402, /**< The window configuration is invalid. */
	WindowCreationFailed = 0x0403, /**< The FlowUi window could not be created. */
	WindowIdSpaceExhausted = 0x0404, /**< The non-reusable window identity space is exhausted. */
	WindowRegistryCollision = 0x0405, /**< A window identity collided with an existing registry entry. */
	MainWindowDestructionForbidden = 0x0406, /**< The semantic main window cannot be destroyed explicitly. */
	WindowPresentationUnsupported = 0x0407, /**< The selected device cannot present to this window. */
	WindowRecreationFailed = 0x0408, /**< The FlowUi window rendering resources could not be recreated. */
	WindowOperationUnsupported = 0x0409, /**< The window backend does not support this native operation. */
	WindowOperationFailed = 0x040a, /**< The native window operation could not be completed. */

	FrameAlreadyActive = 0x0501, /**< Another FlowUi window frame is already active. */
	FramePhaseViolation = 0x0502, /**< The frame operation is invalid in the current phase. */
	FrameNotReady = 0x0503, /**< The FlowUi window is not ready for the requested frame operation. */
	FrameNumberSpaceExhausted = 0x0504, /**< The FlowUi frame identity space was exhausted. */
	PreparedFrameStale = 0x0505, /**< Prepared UI data does not match its sealed storage frame. */
	FrameCapacityExceeded = 0x0506, /**< The frame exceeded a configured FlowUi capacity. */
	UiBuildCallbackFailed = 0x0507, /**< A managed window UI build callback threw an exception. */

	VulkanVersionUnsupported = 0x0601, /**< The selected device does not support the required Vulkan version. */
	VulkanExtensionMissing = 0x0602, /**< A required Vulkan extension is unavailable. */
	VulkanDeviceUnavailable = 0x0603, /**< No usable Vulkan physical or logical device is available. */
	VulkanFeatureMissing = 0x0604, /**< The selected Vulkan device lacks a required feature. */
	VulkanNativeCallFailed = 0x0605, /**< A Vulkan operation failed. */
	VulkanDeviceLost = 0x0606, /**< The Vulkan device was lost. */
	SwapchainUnavailable = 0x0607, /**< A usable swapchain could not be created. */
	SwapchainOutOfDate = 0x0608, /**< The window swapchain is out of date. */
	SwapchainImageInvalid = 0x0609, /**< The acquired swapchain image is inconsistent with FlowUi state. */
	PresentationFailed = 0x060a, /**< Presentation of a FlowUi window failed. */
	PresentIdSpaceExhausted = 0x060b, /**< The swapchain presentation identity space was exhausted. */

	ShaderUnavailable = 0x0701, /**< The requested shader asset cannot be loaded. */
	ShaderInvalid = 0x0702, /**< The shader payload is invalid for the requested pipeline. */
	RendererConfigurationInvalid = 0x0703, /**< The renderer configuration is invalid. */
	RendererCapacityExceeded = 0x0704, /**< The renderer exceeded a configured capacity. */
	RenderCommandInvalid = 0x0705, /**< The UI render command stream is invalid. */
	RendererGenerationStale = 0x0706, /**< The requested renderer generation is no longer current. */
	RendererNativeResourceInvalid = 0x0707, /**< The renderer received invalid committed native resources. */

	StorageConfigurationInvalid = 0x0801, /**< The storage configuration is invalid. */
	StorageBudgetExceeded = 0x0802, /**< The requested storage allocation exceeds its configured budget. */
	StorageCapacityExceeded = 0x0803, /**< A FlowUi storage capacity was exceeded. */
	StorageMutationSealed = 0x0804, /**< Storage mutation is not allowed while a sealed snapshot is active. */
	StorageFrameProtocolViolation = 0x0805, /**< The storage frame operation violates the current frame state. */
	StorageSubmissionProtocolViolation = 0x0806, /**< The storage submission ownership protocol was violated. */
	StoragePublicationFailed = 0x0807, /**< Storage could not publish the requested record or resource. */
	StorageGenerationExhausted = 0x0808, /**< A storage generation space was exhausted. */

	InvalidResourceKey = 0x0901, /**< The resource key is invalid for this manager. */
	ResourceNotFound = 0x0902, /**< The requested FlowUi resource was not found. */
	ResourceHandleInvalid = 0x0903, /**< The FlowUi resource handle is invalid or stale. */
	ResourceKindMismatch = 0x0904, /**< The FlowUi resource has the wrong kind. */
	ResourceWindowMismatch = 0x0905, /**< The FlowUi resource belongs to a different window. */
	ResourceCreationFailed = 0x0906, /**< The FlowUi resource could not be created. */
	ResourcePublicationFailed = 0x0907, /**< The FlowUi resource could not be published. */
	ResourceInUse = 0x0908, /**< The FlowUi resource is still in use. */
	ResourceGenerationExhausted = 0x0909, /**< A resource generation space was exhausted. */

	AssetPathEmpty = 0x0a01, /**< The asset path is empty. */
	AssetNotFound = 0x0a02, /**< The requested asset does not exist. */
	AssetOpenFailed = 0x0a03, /**< The asset could not be opened. */
	AssetReadFailed = 0x0a04, /**< The asset could not be read. */
	AssetFormatUnsupported = 0x0a05, /**< The asset format is unsupported. */
	AssetPayloadInvalid = 0x0a06, /**< The asset payload is invalid. */
	AssetDecodeFailed = 0x0a07, /**< The asset could not be decoded. */

	FontFamilyAlreadyExists = 0x0b01, /**< The font family is already registered. */
	FontFamilyNotFound = 0x0b02, /**< The font family is not registered. */
	FontBakeUnavailable = 0x0b03, /**< Runtime font baking is unavailable in this build. */
	FontBakeFailed = 0x0b04, /**< Runtime font baking failed. */
	FontAtlasCapacityExceeded = 0x0b05, /**< The font atlas capacity was exceeded. */
	FontIdSpaceExhausted = 0x0b06, /**< The font identity space was exhausted. */
	DefaultFontUnavailable = 0x0b07, /**< No configured default font could be loaded. */

	ImageDecodeFailed = 0x0c01, /**< The image could not be decoded. */
	ImageSizeOverflow = 0x0c02, /**< The decoded image is too large to address. */
	ImagePublicationFailed = 0x0c03, /**< The decoded image could not be published. */

	IconSourceInvalid = 0x0d01, /**< The icon source is invalid. */
	IconNotFound = 0x0d02, /**< The requested icon is not registered. */
	IconRasterizationFailed = 0x0d03, /**< The icon could not be rasterized. */
	IconRasterInvalid = 0x0d04, /**< The icon raster payload is invalid. */
	IconAtlasCapacityExceeded = 0x0d05, /**< The icon atlas has no usable capacity. */
	IconKeyCollision = 0x0d06, /**< An internally generated icon key collided. */

	InvalidElementId = 0x0e01, /**< The Flow element identity is invalid. */
	ElementDefinitionConflict = 0x0e02, /**< The Flow element definition conflicts with an existing definition. */
	ElementStateUnavailable = 0x0e03, /**< The Flow element state is unavailable. */
	ElementResourceUnavailable = 0x0e04, /**< The Flow element resource is unavailable. */
	ElementResourceConstructionFailed = 0x0e05, /**< Flow element resource construction failed. */
	ElementResourceRecursiveConstruction = 0x0e06, /**< Flow element resource construction is recursive. */
	ElementResourceDestroying = 0x0e07, /**< Flow element resources are being destroyed. */
	ElementStorageStale = 0x0e08, /**< Committed Flow element storage became stale. */

	InvalidActionId = 0x0f01, /**< The action identity is invalid. */
	ActionAlreadyBound = 0x0f02, /**< The action is already bound. */
	ActionNotBound = 0x0f03, /**< The action is not bound. */
	ActionDisabled = 0x0f04, /**< The action is disabled. */
	ActionResultTypeMismatch = 0x0f05, /**< The action result type does not match the invocation. */
	ActionPublicationConflict = 0x0f06, /**< Action binding state changed during publication. */

	ThemeTypeNotFound = 0x1001, /**< The requested theme type is not registered. */
	ThemeVariantNotFound = 0x1002, /**< The requested theme variant is not registered. */
	ThemeActiveVariantMissing = 0x1003, /**< The theme type has no active variant. */
	ThemeMutationFailed = 0x1004, /**< A staged theme mutation failed. */

	InputFieldUnavailable = 0x1101, /**< The requested input field is unavailable. */
	InputRangeInvalid = 0x1102, /**< The input edit range is invalid. */
	InputUtf8Invalid = 0x1103, /**< The input text is not valid UTF-8. */
	InputSizeLimitExceeded = 0x1104, /**< The input text exceeds its configured size limit. */

	PopupFrameInactive = 0x1201, /**< The popup operation requires an active UI frame. */
	PopupDuplicateSubmission = 0x1202, /**< The popup was submitted more than once in one frame. */
	PopupAnchorNotFound = 0x1203, /**< The popup anchor was not found. */
	PopupCapacityExceeded = 0x1204, /**< The popup capacity was exceeded. */

	ShortcutInvalid = 0x1301, /**< The shortcut registration is invalid. */
	ShortcutIdSpaceExhausted = 0x1302, /**< The shortcut identity space was exhausted. */
	ShortcutNotFound = 0x1303, /**< The shortcut registration was not found. */

	ViewportDetached = 0x1401, /**< The viewport is detached from storage. */
	ViewportConfigurationInvalid = 0x1402, /**< The viewport configuration is invalid. */
	ViewportNotFound = 0x1403, /**< The requested viewport was not found. */
	ViewportKeyCollision = 0x1404, /**< The viewport key is already registered. */
	ViewportGenerationIncomplete = 0x1405, /**< The viewport target generation has not completed initialization. */
	ViewportGenerationExhausted = 0x1406, /**< The viewport generation identity space was exhausted. */
	ViewportRecordingFailed = 0x1407, /**< Viewport command recording failed. */

	AgenticInvalidRequest = 0x1501, /**< The agentic request is invalid. */
	AgenticUnavailable = 0x1502, /**< The agentic workflow service is unavailable. */
	AgenticBusy = 0x1503, /**< Another agentic operation prevents this request. */
	AgenticStaleGeneration = 0x1504, /**< The request references a stale generation. */
	AgenticInvalidScope = 0x1505, /**< The agentic scope is invalid. */
	AgenticBudgetExhausted = 0x1506, /**< The configured agentic budget is exhausted. */
	AgenticIncompleteCapture = 0x1507, /**< The requested evidence capture is incomplete. */
	AgenticPublicationFailed = 0x1508, /**< Agentic evidence could not be published. */
	AgenticIncompatibleComparison = 0x1509, /**< Evidence inputs cannot be compared. */
	AgenticCanceled = 0x150a, /**< The agentic operation was canceled. */
	AgenticTimeout = 0x150b, /**< The agentic operation timed out. */
	AgenticIntegrityFailed = 0x150c, /**< Agentic evidence integrity validation failed. */
};

/** Static catalog data associated with one ErrorCode. */
struct ErrorDescriptor {
	/** Stable code described by this catalog entry. */
	ErrorCode code = ErrorCode::None;
	/** Subsystem that owns the code. */
	ErrorDomain domain = ErrorDomain::None;
	/** Transport mechanism used to reach the resolution boundary. */
	ErrorCategory category = ErrorCategory::Local;
	/** Boundary intended to select or apply a resolution. */
	ErrorBoundary boundary = ErrorBoundary::DirectCaller;
	/** Available continuation/recreation options. */
	ErrorResolvability resolvability = ErrorResolvability::CallerResolvable;
	/** Expected effect after the default resolution. */
	ErrorImpact impact = ErrorImpact::None;
	/** Authority allowed to choose the resolution. */
	ErrorResolutionPolicy resolutionPolicy = ErrorResolutionPolicy::CallerDefined;
	/** Default catalog resolution before any configured policy is applied. */
	ErrorResolution defaultResolution = ErrorResolution::None;
	/** Stable name backed by a static string literal. */
	std::string_view name = "None";
	/** Human-readable message backed by a static string literal. */
	std::string_view message = "No FlowUi error.";
};

} // namespace FlowUi
